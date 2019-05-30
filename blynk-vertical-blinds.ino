

#define APP_DEBUG        // Comment this out to disable debug prints

#define BLYNK_PRINT Serial
#include <BlynkSimpleEsp32.h>
#include <SPI.h> 
#include <WiFi.h> 
#include <HTTPClient.h> 
#include <ArduinoJson.h> 
#include <WiFiClientSecure.h> 
#include <TimeLib.h>
#include <WidgetRTC.h>
#include <Update.h>
#define DEBUG_STREAM Serial//terminal
#include <Preferences.h>
#include "time_store.cpp"
#include "time.h"
#include "cred.h"
#include "pins.h"
#include "motor_control.h"
#include "blynk_pins.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"

Preferences preferences;

WidgetTerminal terminal(V2);


TaskHandle_t TaskA;

const char* ntpServer = "pool.ntp.org"; // where to get the current date and time

String sunsetsunrisebaseurl="https://api.sunrise-sunset.org/json?formatted=0&lng=";

void setup() {
  Serial.begin(115200);

  pinMode(btn1,INPUT_PULLUP);
  pinMode(btn2,INPUT_PULLUP);

  // for storing the times over power cycle
  preferences.begin("auto-curtain", false);

  setup_motors();
  xTaskCreatePinnedToCore(
   IndependentTask,                  /* pvTaskCode */
   "NoBlynkSafe",            /* pcName */
   8192,                   /* usStackDepth */
   NULL,                   /* pvParameters */
   1,                      /* uxPriority */
   &TaskA,                 /* pxCreatedTask */
   1);                     /* xCoreID */ 

  //delay(50000);
  Serial.println("Connecting...");
  Blynk.begin(auth, ssid, pass, "morningrod.blynk.cc", 8080);
//  Blynk.begin(auth, ssid, pass);
  
  while(!Blynk.connected()){
    delay(300);
    Serial.print(".");
  }
 
  DEBUG_STREAM.println("Connected!");
  //setSyncInterval(10 * 60);
  //DEBUG_STREAM.println("Done.");
  
  lat=preferences.getFloat("lat", -1);
  lon=preferences.getFloat("lon", -1);
  DEBUG_STREAM.println("Loading timetables...");
  for(int i=0;i<4;i++)load_time(i); // load time configuration
  sun_delay = preferences.getInt("sun_delay",0); // load the delay time (in minutes) between sunset/sunrise and open/close
  //reset_motors(1);
  DEBUG_STREAM.println("Ready!");
  sendData(0x21,0);

  check_timer=millis()+10000;
  daylight_timer=millis()+9000;

  for(int i=0;i<4;i++){
    if(times[i].active)
      last_timezone_offset=times[i].offset;
  }
  //configTime(last_timezone_offset, 0, ntpServer, "time.nist.gov", "time.windows.com");
}

time_store sunrise;
time_store sunset;

void IndependentTask( void * parameter ){  
  // the internet should not be used AT ALL in this function!!!!
  /*stopM2();
  setM1dir(1);
  waitStallM2(8000);
  delay(3000);
  setM1dir(2);
  waitStallM2(8000);
  delay(3000);*/
  
  /*stopM1();
  setM2dir(1);
  waitStallM2(8000);
  delay(3000);
  setM2dir(2);
  waitStallM2(8000);
  delay(3000);//*/

  /*stall_turn_steps(2,200000);
  Serial.println(sendData(0x21, 0),DEC);
  delay(3000);

  stall_turn_steps(1,200000);

  Serial.println(sendData(0x21, 0),DEC);
  
  while(true){
    waitStallM1(1);
    waitStallM2(1);
  }//*/
  while(true) {
    // buttons

    // A press sets the command to open or close the track motor.
    if(!digitalRead(btn1)){
      command = TRACK_OPEN;
    }
    if(!digitalRead(btn2)){
      command = TRACK_CLOSE;
    }

    // shaft motor is M1, track motor is M2

    // commands are sent from other threads so that blocking function calls
    // (like trackClose(), shaftClose(), trackOpen(), and shaftOpen()) can be
    // called without causing bizzare hickups in the other threads, namely the main thread
    // which controls Blynk.
    if(command==TRACK_CLOSE){
      move_close();
    }else if(command==TRACK_OPEN){
      move_open();
    }else if(command==SHAFT_CLOSE){
      move_shaft_close();
    }else if(command==SHAFT_OPEN){
      move_shaft_open();
    }
    command = -1;
  }//*/
}


void loop() {
  // This handles the network and cloud connection
  Blynk.run();
  
  if(check_timer<millis()){
    check_timer=millis()+20000; // check every 20 seconds. (avoid missing minutes)
    // get the current time
    struct tm ctime;
    ctime.tm_hour=hour();
    ctime.tm_min=minute();
    DEBUG_STREAM.print(" ---- Time is ");
    DEBUG_STREAM.print(ctime.tm_hour);
    DEBUG_STREAM.print(":");
    DEBUG_STREAM.print(ctime.tm_min);
    DEBUG_STREAM.println(" ----");

    if(ctime.tm_hour==2){
      daylight_timer=millis()+10; // trigger very soon to get daylight data back
    }

    DEBUG_STREAM.print("Timezone Offset: ");
    DEBUG_STREAM.println(last_timezone_offset);
    DEBUG_STREAM.print("GPS looks valid: ");
    DEBUG_STREAM.println((lon==0||lat==0||lon==-1||lat==-1) ? "NO" : "YES");
    
    for(int i=0;i<4;i++){
      DEBUG_STREAM.print("Check: ");
      DEBUG_STREAM.print(i);
      DEBUG_STREAM.print(" is ");
      DEBUG_STREAM.println((bool)times[i].active);
      // don't waste CPU time
      if(!times[i].active)continue;
      
      if(times[i].type==1){ // sunrise
        // change times[i] to target the sunrise
        times[i].minute=sunrise.minute;
        times[i].hour=sunrise.hour;

        // add the wait time for sunrise/sunset
        /*times[i].minute+=sun_delay;
        times[i].hour+=times[i].minute/60;
        times[i].minute=times[i].minute%60;*/
      }if(times[i].type==2){ // sunset
        // change times[i] to target the sunset
        times[i].minute=sunset.minute;
        times[i].hour=sunset.hour;

        // add the wait time for sunrise/sunset
        times[i].minute+=sun_delay;
        times[i].hour+=times[i].minute/60;
        times[i].minute=times[i].minute%60;
      }
      DEBUG_STREAM.print("  Check is ");
      DEBUG_STREAM.print(times[i].hour);
      DEBUG_STREAM.print(":");
      DEBUG_STREAM.println(times[i].minute);
      // check time first (lest likely to match first = fastest computation)
      if(ctime.tm_min==times[i].minute&&ctime.tm_hour==times[i].hour){
        // check week day (day_sel is days since monday, tm_wday is days since sunday)
        if(times[i].day_sel[(ctime.tm_wday+6)%7]){
          // Activate!  Change direction!

          // TODO: fix timing.  Exact operation for this is unknown.
          
          //q=(1+(1&i)); // odd indexes close and even indexes open <- old way to set motor direction
        }
      }
    }
  }
  
  // find out what time the sunrise/sunset is once a day, super early in the morning
  if(daylight_timer<millis()){
    daylight_timer=millis()+86400000;
    HTTPClient http;
    http.begin(sunsetsunrisebaseurl+String(lon)+"&lat="+String(lat));
    
    if(http.GET()<0)return; // error
    const char* data=http.getString().c_str();
    StaticJsonDocument<800> doc;
    DeserializationError error = deserializeJson(doc, data);

    // Test if parsing succeeds.
    if (error) {
      DEBUG_STREAM.print(F("deserializeJson() failed: "));
      DEBUG_STREAM.println(error.c_str());
      return;
    }
  
    // Get the root object in the document
    JsonObject root = doc.as<JsonObject>()["results"];
    String sunrise_str = root["sunrise"]; // "2018-09-09T05:55:31+00:00"
    String sunset_str = root["sunset"]; // "2018-09-09T18:34:09+00:00"
    sunrise_str=sunrise_str.substring(11,16);
    sunset_str=sunset_str.substring(11,16);
    int sunset_hr=(sunset_str.substring(0,2).toInt()+(last_timezone_offset/3600)+24)%24;
    int sunrise_hr=(sunrise_str.substring(0,2).toInt()+(last_timezone_offset/3600)+24)%24;
    int sunset_min=sunset_str.substring(3).toInt();
    int sunrise_min=sunrise_str.substring(3).toInt();
    sunrise.hour=sunrise_hr;
    sunrise.minute=sunrise_min;
    sunset.hour=sunset_hr;
    sunset.minute=sunset_min;
    //last_timezone_offset=-1;// force update of timezone and time data
  } 
}

void save_time(int i){
  DEBUG_STREAM.print("Saving time #");
  DEBUG_STREAM.println(i);
  if(times[i].type==0){
    DEBUG_STREAM.print(" > Active at ");
    DEBUG_STREAM.print(times[i].hour);
    DEBUG_STREAM.print(":");
    DEBUG_STREAM.println(times[i].minute);
  }else if(times[i].type==1){
    DEBUG_STREAM.println(" > Active at sunrise.");
  }else
    DEBUG_STREAM.println(" > Active at sunset.");
  // make one string variable to save CPU power and memory
  String num_=String(i); 
  preferences.putUInt(("hour_"+num_).c_str(), times[i].hour);
  preferences.putUInt(("minute_"+num_).c_str(), times[i].minute);
  preferences.putUInt(("type_"+num_).c_str(), times[i].type);
  preferences.putLong(("offset_"+num_).c_str(), times[i].offset);
  preferences.putUChar(("active_"+num_).c_str(), times[i].active);
  for(int n=0;n<7;n++){
    preferences.putUChar(("day_sel_"+num_+"_"+n).c_str(), times[i].day_sel[n]);
  }
}
void load_time(int i){
  // make one string variable to save CPU power and memory
  String num_=String(i); 
  DEBUG_STREAM.print("Reloading timer ");
  DEBUG_STREAM.println(i);
  times[i].hour=preferences.getUInt(("hour_"+num_).c_str(),12);
  times[i].minute=preferences.getUInt(("minute_"+num_).c_str(), 0);
  times[i].type=preferences.getUInt(("type_"+num_).c_str(), 0);
  times[i].offset=preferences.getLong(("offset_"+num_).c_str(), -25200);
  // only set the first two as active by default
  times[i].active=preferences.getUChar(("active_"+num_).c_str(), 0);
  for(int n=0;n<7;n++){
    // by default select all days.
    times[i].day_sel[n]=preferences.getUChar(("day_sel_"+num_+"_"+n).c_str(), 1);
  }
}
