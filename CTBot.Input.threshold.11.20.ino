/*
 Name:        CTBot.Input.threshold.ino
 Created:     10/2022
 Author:      Avgeros Kostas
 Description: A usefull way for watering plants through telegram input commands
              From sunset to midnight (or sunrise)
*/

/* Version */
const char* version = "CTBot.Input.threshold.11.xx";

/* Capacitive Soil Moisture */
const int SensorPin = A0;     // values between 0 and 1023
const int AirValue = 725;
const int WaterValue = 293;
int soilMoistureValue = 0;
int soilMoistureSensor = 50;  // no value to set properly

long sunriseTomorrow;
long sunrise;
long sunset;
long duration;
long timezone_offset;
int timezoneInHours;
int getHoursNTP;

int DSTflag; 
int flagTgmCheck;             // call through telegram -> flag it
int flagTgmDebug = 0;         // Debug mode -> flag it
int wateringUntilMidinghtFlag = 1;  // Watering till midnight
int nightFlag = 0;            // Check if Night is here -> flag it    ?????

/* Configuration of TZ - DST */
// #define MY_NTP_SERVER "pool.ntp.org"  
// #define MY_TZ "EET-2EEST,M3.5.0/3,M10.5.0/4"  

/* Server, file and port */
const char hostname[] = "api.openweathermap.org";
const String uri = "/data/2.5/onecall?lat=37.98&lon=23.72&appid=460338d1d4c136ea7c0abad824505331&units=metric";
const int port = 80;

/* Valve pins */
const int relay1 = 5; // D1, Close Valve
const int relay2 = 4; // D2, Open Valve

/* Variables for time and duration */
unsigned long previousMillis;
unsigned long epochTime;

/* - - - - - - - - LIBRARIES - - - - - - - - - -  */

/* My Libraries */
#include "settings.h"

/* Libraries */
#include <ESP8266WiFi.h>     // WiFI
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>     // API call
#include <NTPClient.h>       // NTP Client to get time
#include <WiFiUdp.h>         // Udp, socket
#include "CTBot.h"
CTBot myBot;
TBMessage msg;
CTBotReplyKeyboard myKbd;   // reply keyboard object helper
bool isKeyboardActive;      // store if the reply keyboard is shown

/* Convert Unixtime in readable format */
#include <RTClib.h>  till v7, many times site is down // DateTime

/* Configuration of TZ - DST 
#include <time.h>
#define MY_NTP_SERVER "pool.ntp.org"
#define MY_TZ "EET-2EEST,M3.5.0/3,M10.5.0/4"  */

/* Globals TM - DST 
time_t now;  // this is the epoch
tm tm;       // the structure tm holds time information in a more convient way */

/* Define NTP Client to get time */
const long utcOffsetInSeconds = 0; //Athens time zone is GMT+3 = 3*60*60 = 10800seconds difference
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

// ----------------------------------------------------------------------------------------

/* Connect to WiFi */
void ConnectWiFi() {
  Serial.println();
  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.println();
      
  /* ATTEMPTS TO JOIN A NWRmoistureThreshold WITH PASSWORD */
  WiFi.begin(ssid, password);  // MAGICAL COMMAND OF ESP8266!!
  int counter = 0;
  while ( WiFi.status() != WL_CONNECTED ) {
    delay(500);
    if (++counter > 20) 
      ESP.restart();
    Serial.print(".");
  }

  /* Show that we are connected */
  Serial.print("Connected to: "); 
  Serial.println(ssid);
  Serial.println();
} 

// ----------------------------------------------------------------------------------------

void millis_cycle() {
  yield();
  
  /* This part runs every second, millis START */ 
  if ( millis() - previousMillis >= 1000UL) {    // one second, 'UL' means unsigned long
    previousMillis += 1000UL;
    
    telegramKBDbot();
   
    /* Every 60 seconds */ 
    static int count60s;
    count60s++;
    if ( count60s >= 60 ) {
      count60s = 0;
      Serial.println("------every 60 seconds!---------"); // maybe cause of restart
      timeClient.update();
      epochTime = timeClient.getEpochTime();  // unsigned long -> errors
      Serial.println(epochTime);
      if ( flagTgmDebug ==1 ) {  // testing Debug mode
        myBot.sendMessage(-76578790164140034, "epochTime", "");  // msg.group.id --> 
      }
      
      /* Every 5 minutes */
      static int count5m;
      count5m++;
      if ( count5m >= 5 ) {
        count5m = 0;
        Serial.println("------every 5 minutes!---------");

        NTPupdate();
        
        checkSunset();  // tip: checkSunset(); <---- apiCall(); after NightFlag
      }
      
      /* Every 10 minutes */
      static int count10m;
      count10m++;
      if ( count10m >= 10 ) {
        count10m = 0;
        Serial.println("------every 10 minutes!---------");
        // ...
      }

      /* Every 30 minutes */
      static int count30m;
      count30m++;
      if ( count30m >= 30 ) {
        count30m = 0;
        Serial.println("------every 30 minutes!---------");
        timeClient.update();
        checkSensor();
        // apiCall();   
      }
        
      /* Every 1 hour */
      static int count60m;
      count60m++;
      if ( count60m >= 60 ) {
        count60m = 0;
        
        Serial.println("------every 1 hour!---------");

        /* Every 8 hours */
        static int count8h;
        count8h++;
        if ( count8h >= 8 ) {
          count8h = 0;
          Serial.println("------every 8 hours!---------");
          if ( flagTgmDebug == 1 ) {  // Debug mode
            myBot.sendMessage(msg.group.id, "------every 8 hours!---------", "");
          }
        }
      }
    }
  }
}

// ----------------------------------------------------------------------------------------

void NTPupdate() {
  yield();
  timeClient.update();

  Serial.println(" * * * * * NTPupdate * * * * * ");
  if ( timezoneInHours == 3) {
    getHoursNTP = (timeClient.getHours() + timezoneInHours);
    String message = "NTPupdate: SUMMER \n";
           message += "timezoneInHours: " + String(timezoneInHours) + " \n";
           message += "getHoursNTP: " + String(getHoursNTP);  
           Serial.println(message);
    if (flagTgmDebug ==1) {  // Debug mode
      myBot.sendMessage(msg.group.id, message, " "); // msg.group.id --> 
    }
  } else {
    getHoursNTP = (timeClient.getHours() + timezoneInHours);
    String message = "NTPupdate: WINTER \n";  
           message += "timezoneInHours: " + String(timezoneInHours) + " \n";
           message += "getHoursNTP: " + String(getHoursNTP);  
           Serial.println(message);
    if (flagTgmDebug ==1) {  // Debug mode
      myBot.sendMessage(msg.group.id, message, " "); // msg.group.id --> 
    }
  }
  Serial.println(" * * * * * * * * * * * * * * * ");
}

// ----------------------------------------------------------------------------------------

void checkSunset() {  // tip: checkSunset(); <---- apiCall();
  if (epochTime > sunset) {
    
    if (epochTime < sunriseTomorrow) {
      nightFlag = 1;

      if ( (getHoursNTP == 24) && (wateringUntilMidinghtFlag == 1) ) { // When it is midnight
        nightFlag = 0;  // RESET FLAG
        String message = "-- RESET FLAG -- midnight apiCall -- \n" ;
               message += "nightFlag: " + String(nightFlag) + "\n";
               message += "Watering till midnight: " + String(wateringUntilMidinghtFlag);
        Serial.println(message);
        if (flagTgmDebug == 1) {  // Debug mode
          myBot.sendMessage(msg.group.id, message, "");
        }
        apiCall();
        return;
      } else {
        String message = "--------- N I G H T ---------  \n";
               message += "nightFlag: " + String(nightFlag);
        Serial.println(message);
        if ( flagTgmDebug == 1 ) {  // Debug mode
          myBot.sendMessage(msg.group.id, message, "");
        }
      }  

    } else if (epochTime > sunriseTomorrow) {   /* testing mode 1 call per day */
      nightFlag = 0;  // RESET FLAG
      apiCall();  //  <--------------------------------------  
      String message = "--------- RESET nightFlag ---------  APICALL \n" ;
             message += " nightFlag: " + String(nightFlag);
      Serial.println(message);
      if ( flagTgmDebug == 1 ) {  // Debug mode
        myBot.sendMessage(msg.group.id, message, "");
      } 
    }
    
  } else {  //  epochTime < sunset 
    nightFlag = 0;
    String message = "--------- Out of schedule --------- ";
           message += "nightFlag: " + String(nightFlag);
    Serial.println(message);
    if (flagTgmDebug == 1) {  // Debug mode
      myBot.sendMessage(msg.group.id, message, ""); // msg.group.id --> 
    }
  }
}

// ----------------------------------------------------------------------------------------

String sendStatus() {  // string, NOT void
  //----------------- Soil Moisture Sensor --------------------//
  soilMoistureValue = analogRead(SensorPin);  //put Sensor insert into soil
  // soilMoistureSensor = map(soilMoistureValue, AirValue, WaterValue, 0, 100);
  
  String message = "soilMoistureSensor: " + String(soilMoistureSensor) + " % \n";
    message += "Moisture Threshold: " + String(moistureThreshold) + " % \n";
    message += "Watering Time: " + String((WateringTime)/1000) + " secs \n";
    message += "Watering till midnight: " + String(wateringUntilMidinghtFlag) + " \n";
    if ( flagTgmDebug == 1 ) {  // Debug mode
      message += "Night Flag: " + String(nightFlag) + " \n";
    }
    message += "Debug Mode: " + String(flagTgmDebug);
    
  Serial.println(message);
  myBot.sendMessage(msg.group.id, message, "");
  return message; // if not, system goes restart
}

// ----------------------------------------------------------------------------------------

void checkSensor() {      
  //----------------- Soil Moisture Sensor --------------------//
  soilMoistureValue = analogRead(SensorPin);  //put Sensor insert into soil
  // soilMoistureSensor = map(soilMoistureValue, AirValue, WaterValue, 0, 100);
  
  soilMoistureSensor = 60;   // testing mode  // testing mode  // testing mode  // testing mode

  /*
  Serial.println("Sending sensor's data.. :");
  Serial.print("Value: ");
  Serial.print(soilMoistureValue);
  Serial.print("\t");
  Serial.print("Percent: ");
  Serial.print(soilMoistureSensor);
  Serial.println("%");
  Serial.println();
  */
  
  //----------------- Soil Moisture Sensor --------------------// 

  // ************ Heart of Sketch ************
  if ( nightFlag == 1 ) {
    Serial.println(" nightFlag == 1 ");
    if (soilMoistureSensor < moistureThreshold) {
      Serial.println(" - Moisture SENSOR < moisture THRESHOLD - ");
      Serial.println("---------");
      Serial.println("SQUIRT TIME !!");
      myBot.sendMessage(msg.group.id, "SQUIRT TIME !!");
      Serial.println("---------");
      delay(500);
      digitalWrite(relay2, HIGH);  // OPEN VALVE!!
      Serial.println("Open Valve");
      delay(500);
      digitalWrite(relay2, LOW);
      delay(WateringTime);         // WATERING TIME
      digitalWrite(relay1, HIGH);  // CLOSE VALVE!!
      Serial.println("Close Valve");
      delay(500);
      digitalWrite(relay2, LOW);
      delay(500);
      Serial.println("sendStatus");
      sendStatus();
    } else if (flagTgmCheck == 1) {
      Serial.println(" - Sensor < Threshold - ");
      myBot.sendMessage(msg.group.id, " - Sensor < Threshold - ", " ");
      Serial.println("sendStatus");
      sendStatus();
      Serial.println();
    } else {
      Serial.println("Moisture sensor above Threshold ");
      myBot.sendMessage(msg.group.id, "Moisture sensor above Threshold", " ");
      Serial.println("sendStatus");
      sendStatus();
      return;
    }
  } else if (flagTgmCheck == 1) {
    Serial.println("Out of time schedule.. TgmCall");
    Serial.print("flagTgmCheck: ");
    Serial.println(flagTgmCheck);
    myBot.sendMessage(msg.group.id, "Out of time schedule.. TgmCall", " ");
    Serial.println("sendStatus");
    Serial.println("***********************");
    sendStatus();
  } else {
    Serial.println("Out of time schedule..");
    if ( flagTgmDebug ==1 ) {
      myBot.sendMessage(msg.group.id, "Out of time schedule..", " ");
    }
    Serial.println("***********************");
  }
}

// ----------------------------------------------------------------------------------------

/* apiCall */
void apiCall() {
  if ( flagTgmDebug == 1 ) {  // Debug mode
    myBot.sendMessage(msg.group.id, "API CALL", "");
  }
  
  // WiFi client
  WiFiClient client; // hold our tcp connection info for us
  
  // Connect to Server
  Serial.print("Connecting to: ");
  Serial.println(hostname);
  if ( client.connect(hostname, port) == 0 ) {
    Serial.println("Connection failed");
  } else {
    Serial.println("Connected!");
  }
  
  // Send request for file from server
  client.print( "GET " + uri + " HTTP/1.1\r\n" + 
              "Host: " + hostname + "\r\n" +
              "Connection: close\r\n" + 
              "\r\n");
  delay(500);

  // Check HTTP status
  char status[32] = {0};
  client.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
    Serial.print(F("Unexpected response: "));
    Serial.println(status);
    myBot.sendMessage(msg.group.id, status);
    myBot.sendMessage(msg.group.id, "HTTP error, HTTP/1.1 200 NotOK, 10s delay. System restart!  : #1");
    // after delay , system hault until next midnight, maybe should use flag ?
    delay(10000);
    ESP.restart();
    // return;
  }
  
  // Skip HTTP headers
  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders)) {
    Serial.println(F("Invalid response"));
    myBot.sendMessage(msg.group.id, "Invalid response");
    myBot.sendMessage(msg.group.id, "HTTP error, -Invalid response- 10s delay : #2");
    delay(10000);
    return;
  }

 //----------------- JSON ------------------------//
  // Allocate the JSON document
  // Use https://arduinojson.org/v6/assistant/  assistant to compute the capacity.  
  DynamicJsonDocument doc(24576);

  // Allocate a temporary memory pool
  Serial.println("JSON Buffer is: ");
  Serial.println(24576);

  // Parse JSON object
  DeserializationError error = deserializeJson(doc, client);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    myBot.sendMessage(msg.group.id, "API CALL ERROR, deserializeJson() failed : #3");
    return;
  }

  timeClient.update();
  epochTime = timeClient.getEpochTime();  // unsigned long -> errors
  String formattedTime = timeClient.getFormattedTime();
  
  // read temp from API, only float working, not char cause of (*), string required
  sunriseTomorrow = doc["daily"][1]["sunrise"];
  sunrise = doc["daily"][0]["sunrise"];
  sunset = doc["daily"][0]["sunset"];
  timezone_offset = doc["timezone_offset"];
  timezoneInHours = (timezone_offset / 3600);
  getHoursNTP = (timeClient.getHours() + timezoneInHours) ;

  Serial.println("***********************");
  String message;
    message = "[GMT] Response APICALL(): \n";
    message += "epochTime: " + String(epochTime) + " \n";
    message += "formattedTime: " + String(formattedTime) + " \n";
    message += "sunriseTomorrow: " + String(sunriseTomorrow) + " \n";
    message += "sunset: " + String(sunset) + " \n";
    message += "sunrise: " + String(sunrise) + " \n";
    message += "timezone_offset: " + String(timezone_offset) + " \n";
    message += "timezoneInHours: " + String(timezoneInHours) + " \n";
    message += "getHoursNTP: " + String(getHoursNTP) + " \n";
  Serial.print(message);
  if ( flagTgmDebug ==1 ) {  // Debug mode
      myBot.sendMessage(msg.group.id, message, "");
    }
  Serial.println("***********************");
  
  /* TEST MODE 
  epochTime = 1652583508;
  Serial.println("TEST MODE");
  Serial.println(epochTime);
  Serial.println();
  /* TEST MODE */
}

// ----------------------------------------------------------------------------------------

void telegramKBDbot() {
  // check if there is a new incoming message
  if (CTBotMessageText == myBot.getNewMessage(msg)) {
    
      // received a text message
      if (msg.text.equalsIgnoreCase( ("/show_keyboard") + String(botname) ) || (msg.text.equalsIgnoreCase( ("/show_keyboard") ) ) ) {
        // the user is asking to show the reply keyboard --> show it
        myBot.sendMessage(msg.group.id, "Reply Keyboard enable. You may send a command..", myKbd);
        isKeyboardActive = true;
      }
      // check if the reply keyboard is active 
      else if (isKeyboardActive) {
          // is active -> manage the text messages sent by pressing the reply keyboard buttons
          if (msg.text.equalsIgnoreCase("Hide replyKeyboard")) {
            // sent the "hide keyboard" message --> hide the reply keyboard
            myBot.removeReplyKeyboard(msg.group.id, "Reply keyboard removed");
            isKeyboardActive = false;
          } else {        
            // print every others messages received
            myBot.sendMessage(msg.group.id, msg.text);
          }
      } else {
        // the user write anything else and the reply keyboard is not active --> show a hint message
        // no input command 
        String reply;
           reply = (String)"Welcome!" + (" ") + (String)"Try command \n";
           reply += "/show_keyboard \n";
        Serial.println(reply);
        myBot.sendMessage(msg.group.id, reply);
      }




      /* ================== TEST ================== */
      /* ================== TEST ================== */
if (msg.text.equalsIgnoreCase( ("/set_threshold") + String(botname) ) || (msg.text.equalsIgnoreCase( ("/set_threshold") ) ) ) {  // #1
    Serial.println("Please set a value between 10 to 90..");
    myBot.sendMessage(msg.group.id, "Please set a value between 10 to 90..", " ");
    delay(6000);
      if (CTBotMessageText == myBot.getNewMessage(msg)) {    // 2nd check for msg
        // Check if 2nd msg is a valid msg
        if ( (msg.text.toInt() > 10 ) && (msg.text.toInt() <= 90 ) ) {
          moistureThreshold = msg.text.toInt();
          Serial.println("-  -  -  -  -  -  -");
          Serial.println("Value is valid! ");
            String msgThreshold = "Moisture Threshold set to: " + String(moistureThreshold) + " %";
          Serial.println(msgThreshold);
          myBot.sendMessage(msg.group.id, msgThreshold, "");
        } else {
          Serial.println("-  -  -  -  -  -  -");
          Serial.println("Value is NOT valid");
            String msgThreshold = "Moisture Threshold set to: " + String(moistureThreshold) + " %";
          Serial.println(msgThreshold);
          myBot.sendMessage(msg.group.id, "Value is NOT valid");
        }
      } else {
        Serial.println("No input");
        myBot.sendMessage(msg.group.id, "No input", "");
      }
      sendStatus();
      return;
      
    } else if (msg.text.equalsIgnoreCase( ("/set_watering_time") + String(botname) ) || (msg.text.equalsIgnoreCase( ("/set_watering_time") ) ) ) {  // #2
      Serial.println("Please set a value up to 300 secs..");
      myBot.sendMessage(msg.group.id, "Please set a value up to 300 secs..", " ");
      delay(6000);
      if (CTBotMessageText == myBot.getNewMessage(msg)) {
        // Check if 2nd msg is a valid msg
        if ( (msg.text.toInt() > 0 ) && (msg.text.toInt() <= 300 ) ) {
          WateringTime = (1000*(msg.text.toInt() ) );
          Serial.println("-  -  -  -  -  -  -");
          Serial.println("Value is valid: ");
          String msgWateringTime = "Watering Time set to: " + String((WateringTime)/1000) + " secs";
          myBot.sendMessage(msg.group.id, msgWateringTime, "");
          Serial.println(msgWateringTime);
        } else {
          Serial.println("Value is NOT valid");
          myBot.sendMessage(msg.group.id, "Value is NOT valid");
        }
      } else {
        Serial.println("No input");
        myBot.sendMessage(msg.group.id, "No input", "");
      }
      sendStatus();
      return;
      
    } else if (msg.text.equalsIgnoreCase( ("/status") + String(botname) ) || (msg.text.equalsIgnoreCase( ("/status") ) ) ) {  // #3
      Serial.println();
      Serial.println("command : status");
      // String readings = sendStatus();
      sendStatus();
      NTPupdate();
      return;

      // system goes resart all the time - system bug
    /* } else if (msg.text.equalsIgnoreCase( ("/restart") + String(botname) ) || (msg.text.equalsIgnoreCase( ("/restart") ) ) ) {  // #6
      Serial.println("System goes RESTART");
      delay(500);
      ESP.restart();
      return; */ 

    } else if (msg.text.equalsIgnoreCase( ("/check") + String(botname) ) || (msg.text.equalsIgnoreCase( ("/check") ) ) ) {    // #4
      Serial.println();
      Serial.println("command : check");
      flagTgmCheck = 1;                                         // if we call it through telegram
      Serial.print("flagTgmCheck: ");
      Serial.println(flagTgmCheck);
      Serial.println("Reading sensor and values");
      myBot.sendMessage(msg.group.id, "Reading sensor and values", "");
      
      checkSunset();  // tip: checkSunset(); <---- apiCall();
      
      checkSensor();
      
      flagTgmCheck = 0;  // reset flag
      Serial.print("flagTgmCheck: ");
      Serial.println(flagTgmCheck);
      Serial.println();
      return;
      
    } else if (msg.text.equalsIgnoreCase( ("/debug") + String(botname) ) || (msg.text.equalsIgnoreCase( ("/debug") ) ) ) {   // #5
      Serial.println();
      Serial.println("-  -  -  -  -  -  -");
      Serial.println("command : debug");
      Serial.println("Debug mode: N = 0 or Y = 1");
      myBot.sendMessage(msg.group.id, "Debug mode: N = 0 or Y = 1", " ");
      delay(6000);
      if (CTBotMessageText == myBot.getNewMessage(msg)) {    // 2nd check for msg
        // Check if 2nd msg is a valid msg
        if ( (msg.text.toInt() == 0 ) || (msg.text.toInt() == 1 ) ) {
          Serial.println("Value is valid! ");
          flagTgmDebug = msg.text.toInt();
          if ( flagTgmDebug == 1 ) {
            flagTgmDebug = 1;                    // if we call it through telegram
            String msgTgmDebug = "Debug Mode set to: " + String(flagTgmDebug) + " ";
            myBot.sendMessage(msg.group.id, msgTgmDebug, "");
            Serial.println(msgTgmDebug);
            Serial.println();
            sendStatus();
            return;
          } else {
            // flagTgmDebug = 0;
            String msgTgmDebug = "Debug Mode set to: " + String(flagTgmDebug) + " ";
            myBot.sendMessage(msg.group.id, msgTgmDebug, "");
            Serial.println(msgTgmDebug);
            Serial.println();
          }  
        } else {
          Serial.println("Value is NOT valid");
          myBot.sendMessage(msg.group.id, "Value is NOT valid");
        }
      } else {
        Serial.println("No input");
        myBot.sendMessage(msg.group.id, "No input", "");
      }
      sendStatus();
      return;

    } else if (msg.text.equalsIgnoreCase( ("/watering_till_minight") + String(botname) ) || (msg.text.equalsIgnoreCase( ("/watering_till_minight") ) ) ) {   // #5
      Serial.println();
      Serial.println("-  -  -  -  -  -  -");
      Serial.println("command : watering_untill_minight");
      Serial.println("Watering till minight? N = 0 or Y = 1");
      myBot.sendMessage(msg.group.id, "Watering till minight? N = 0 or Y = 1", " ");
      delay(6000);
      if (CTBotMessageText == myBot.getNewMessage(msg)) {    // 2nd check for msg
        // Check if 2nd msg is a valid msg
        if ( (msg.text.toInt() == 0 ) || (msg.text.toInt() == 1 ) ) {
          Serial.println("Value is valid! ");
          wateringUntilMidinghtFlag = msg.text.toInt();
          if ( wateringUntilMidinghtFlag == 1 ) {
            wateringUntilMidinghtFlag = 1;                    // if we call it through telegram
            String message = "Watering till midnight set to: " + String(wateringUntilMidinghtFlag) + " ";
            myBot.sendMessage(msg.group.id, message, "");
            Serial.println(message);
            Serial.println();
            sendStatus();
            return;
          } else {
            // wateringUntilMidinghtFlag = 0;
            String message = "Watering till midnight set to: " + String(wateringUntilMidinghtFlag) + " ";
            myBot.sendMessage(msg.group.id, message, "");
            Serial.println(message);
            Serial.println();
          }  
        } else {
          Serial.println("Value is NOT valid");
          myBot.sendMessage(msg.group.id, "Value is NOT valid");
        }
      } else {
        Serial.println("No input");
        myBot.sendMessage(msg.group.id, "No input", "");
      }
      sendStatus();
      return;
    }


          /* ================== TEST ================== */
          /* ================== TEST ================== */
          

      
    }   // CTBotMessageText        
  // wait 500 milliseconds
  delay(500);
}
  

// ----------------------------------------------------------------------------------------

void setup() {
  TBMessage msg;
  
  // initialize the Serial
  Serial.begin(115200);
    while (!Serial) continue;

  delay(500);
  Serial.println();
  Serial.println("*********** Starting... ***********");
  Serial.println("==================================="); 
  Serial.println(version);

  /* WiFi */
  ConnectWiFi();
  delay(500);
  
  /* API CALL for sunset */
  apiCall();
  delay(1000);

  /* NTP update */
  NTPupdate();
  delay(500);
  
  checkSunset();  // tip: checkSunset(); <---- apiCall();
  delay(500);
  
  /* TELEGRAM */
  // set the telegram bot token
  myBot.setTelegramToken(token);
    
  // check if all things are ok
  if (myBot.testConnection()) {
    Serial.println("\ntestConnection OK");
    Serial.println(version);
    myBot.sendMessage(1261340431, "============ ============"); // msg.group.id issue
    myBot.sendMessage(1261340431, version);  // kostas
    myBot.sendMessage(msg.group.id, "Hello sweetie! Give me an order..");  // kostas
    
    myBot.sendMessage(-1715779907, "group id"); // msg.group.id 
    myBot.sendMessage(-76578790164140034, "version"); // 
    myBot.sendMessage(5582715298, "bot");  // bot
    
    Serial.println(msg.sender.id);
    Serial.println(msg.group.id);
    Serial.println();
  } else {
    Serial.println("\ntestConnection NOK");
    ESP.restart();
  }

  // reply keyboard customization
  // add a button that send a message with "......" text
  myKbd.addButton("/set_threshold");
  // add a button that send a message with "......" text
  myKbd.addButton("/status");
  // add a button that send a message with "......" text
  myKbd.addButton("/set_watering_time");
  // add a new empty button row
  myKbd.addRow();
  // add a button that send a message with "......" text
  myKbd.addButton("/check");
  // add a button that send a message with "......" text
  myKbd.addButton("/watering_till_minight");
  // add a button that send a message with "......" text
  myKbd.addButton("/debug");
  // add a new empty button row
  myKbd.addRow();
  // add a button that send a message with "Hide replyKeyboard" text
  // (it will be used to hide the reply keyboard)
  myKbd.addButton("Hide replyKeyboard");
  // add a URL button to the second row of the inline keyboard
  myKbd.addButton("see docs", "https://github.com/kostasa1/SmartWateringCTBot", CTBotKeyboardButtonURL);
  // resize the keyboard to fit only the needed space
  myKbd.enableResize();
  isKeyboardActive = false;

  /*
  // Output to check timeClient.getHours();
  Serial.print(daysOfTheWeek[timeClient.getDay()]);
  Serial.print(", ");
  Serial.print(timeClient.getHours());
  Serial.print(":");
  Serial.print(timeClient.getMinutes());
  Serial.print(":");
  Serial.println(timeClient.getSeconds()); */
  
  /* millis() at the end of setup() */
  previousMillis = millis();
}
 
// ----------------------------------------------------------------------------------------

void loop() {
  
  millis_cycle();

}

// ----------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------