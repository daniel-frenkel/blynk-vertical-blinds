


int sun_delay = 0;
long check_timer = 0;
long daylight_timer = 0;
time_store times[4];
long last_timezone_offset=-1;

void save_time(int i);

int command = -1;

WidgetRTC rtc;
BLYNK_CONNECTED() {
  // Synchronize time on connection
  rtc.begin();
}

WiFiClient client;

// Variables to validate
// response from S3
int contentLength = 0;
bool isValidContentType = false;

// S3 Bucket Config for OTA
String host = ""; // Host => bucket-name.s3.region.amazonaws.com
int port = 80; // Non https. For HTTPS 443. As of today, HTTPS doesn't work.
String bin = "/blynk-vertical-blinds.ino.bin"; // bin file name with a slash in front.

// Utility to extract header value from headers
String getHeaderValue(String header, String headerName) {
  return header.substring(strlen(headerName.c_str()));
}

// OTA Logic
void execOTA() {

  DEBUG_STREAM.println("Connecting to: " + String(host));
  // Connect to S3
  if (client.connect(host.c_str(), port)) {
    // Connection Succeed.
    // Fecthing the bin
    DEBUG_STREAM.println("Fetching Bin: " + String(bin));

    // Get the contents of the bin file
    client.print(String("GET ") + bin + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Cache-Control: no-cache\r\n" +
                 "Connection: close\r\n\r\n");

    // Check what is being sent
    //    DEBUG_STREAM.print(String("GET ") + bin + " HTTP/1.1\r\n" +
    //                 "Host: " + host + "\r\n" +
    //                 "Cache-Control: no-cache\r\n" +
    //                 "Connection: close\r\n\r\n");

    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        DEBUG_STREAM.println("Client Timeout !");
        client.stop();
        return;
      }
    }
    // Once the response is available,
    // check stuff

    /*
       Response Structure
        HTTP/1.1 200 OK
        x-amz-id-2: NVKxnU1aIQMmpGKhSwpCBh8y2JPbak18QLIfE+OiUDOos+7UftZKjtCFqrwsGOZRN5Zee0jpTd0=
        x-amz-request-id: 2D56B47560B764EC
        Date: Wed, 14 Jun 2017 03:33:59 GMT
        Last-Modified: Fri, 02 Jun 2017 14:50:11 GMT
        ETag: "d2afebbaaebc38cd669ce36727152af9"
        Accept-Ranges: bytes
        Content-Type: application/octet-stream
        Content-Length: 357280
        Server: AmazonS3

        {{BIN FILE CONTENTS}}

    */
    while (client.available()) {
      // read line till /n
      String line = client.readStringUntil('\n');
      // remove space, to check if the line is end of headers
      line.trim();

      // if the the line is empty,
      // this is end of headers
      // break the while and feed the
      // remaining `client` to the
      // Update.writeStream();
      if (!line.length()) {
        //headers ended
        break; // and get the OTA started
      }

      // Check if the HTTP Response is 200
      // else break and Exit Update
      if (line.startsWith("HTTP/1.1")) {
        if (line.indexOf("200") < 0) {
          DEBUG_STREAM.println("Got a non 200 status code from server. Exiting OTA Update.");
          break;
        }
      }

      // extract headers here
      // Start with content length
      if (line.startsWith("Content-Length: ")) {
        contentLength = atoi((getHeaderValue(line, "Content-Length: ")).c_str());
        DEBUG_STREAM.println("Got " + String(contentLength) + " bytes from server");
      }

      // Next, the content type
      if (line.startsWith("Content-Type: ")) {
        String contentType = getHeaderValue(line, "Content-Type: ");
        DEBUG_STREAM.println("Got " + contentType + " payload.");
        if (contentType == "application/octet-stream") {
          isValidContentType = true;
        }
      }
    }
  } else {
    // Connect to S3 failed
    // Please try again
    // Probably a choppy network?
    DEBUG_STREAM.println("Connection to " + String(host) + " failed. Please try again");
    // retry??
    // execOTA();
  }

  // Check what is the contentLength and if content type is `application/octet-stream`
  DEBUG_STREAM.println("contentLength : " + String(contentLength) + ", isValidContentType : " + String(isValidContentType));

  // check contentLength and content type
  if (contentLength && isValidContentType) {
    // Check if there is enough to OTA Update
    bool canBegin = Update.begin(contentLength);

    // If yes, begin
    if (canBegin) {
      DEBUG_STREAM.println("Begin OTA. This may take 2 - 5 mins to complete. Things might be quite for a while.. Patience!");
      // No activity would appear on the Serial monitor
      // So be patient. This may take 2 - 5mins to complete
      size_t written = Update.writeStream(client);

      if (written == contentLength) {
        DEBUG_STREAM.println("Written : " + String(written) + " successfully");
      } else {
        DEBUG_STREAM.println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?" );
        // retry??
        // execOTA();
      }

      if (Update.end()) {
        DEBUG_STREAM.println("OTA done!");
        if (Update.isFinished()) {
          DEBUG_STREAM.println("Update successfully completed. Rebooting.");
          ESP.restart();
        } else {
          DEBUG_STREAM.println("Update not finished? Something went wrong!");
        }
      } else {
        DEBUG_STREAM.println("Error Occurred. Error #: " + String(Update.getError()));
      }
    } else {
      // not enough space to begin OTA
      // Understand the partitions and
      // space availability
      DEBUG_STREAM.println("Not enough space to begin OTA");
      client.flush();
    }
  } else {
    DEBUG_STREAM.println("There was no content in the response");
    client.flush();
  }
}

BLYNK_WRITE(V1) { // Update OTA
  if(param.asInt()!=0){
    execOTA(); // Executes OTA Update
  }
}

BLYNK_WRITE(V64) { // sunrise/sunset delay
  sun_delay = param.asInt();
  preferences.putInt("sun_delay", sun_delay);
}

BLYNK_WRITE(V12) { // track close now
  if(param.asInt()!=0){
    move_close(); // tell control loop what to do
  }
}

BLYNK_WRITE(V13) { // track open now
  if(param.asInt()!=0){
    move_open(); // tell control loop what to do
  }
}

BLYNK_WRITE(V14) { // shaft close now
  if(param.asInt()!=0){
    move_shaft_close(); // tell control loop what to do
  }
}

BLYNK_WRITE(V15) { // shaft open now
  if(param.asInt()!=0){
    move_shaft_open(); // tell control loop what to do
  }
}

BLYNK_WRITE(V5) { // open1
  times[0].active=param.asInt()!=0;
  preferences.putUChar("active_0", times[0].active);
}

BLYNK_WRITE(V7) { // close1
  times[1].active=param.asInt()!=0;
  preferences.putUChar("active_1", times[0].active);
}

BLYNK_WRITE(V9) { // open2
  times[2].active=param.asInt()!=0;
  preferences.putUChar("active_2", times[0].active);
}

BLYNK_WRITE(V11) { // close2
  times[3].active=param.asInt()!=0;
  preferences.putUChar("active_3", times[0].active);
}

BLYNK_WRITE(V31) { // set acceleration value shaft
  DEBUG_STREAM.print("set acceleration 1: ");
  long q=param.asInt()*1000L;
  preferences.putLong("accel_1", q);
  DEBUG_STREAM.println(q);
  sendData(0xA6, q);     // AMAX_M1
}
BLYNK_WRITE(V32) { // set velocity value shaft
  DEBUG_STREAM.print("set velocity 1: ");
  long q=param.asInt()*1000;
  preferences.putLong("velocity_1", q);
  DEBUG_STREAM.println(q);
  sendData(0xA7, q);     // VMAX_M1
}
BLYNK_WRITE(V33) { // set acceleration value track
  DEBUG_STREAM.print("set acceleration: ");
  long q=param.asInt()*1000L;
  preferences.putLong("accel_2", q);
  DEBUG_STREAM.println(q);
  sendData(0xC6, q);     // AMAX_M2
}
BLYNK_WRITE(V34) { // set velocity value track
  DEBUG_STREAM.print("set velocity: ");
  long q=param.asInt()*1000;
  preferences.putLong("velocity_2", q);
  DEBUG_STREAM.println(q);
  sendData(0xC7, q);     // VMAX_M2
}
BLYNK_WRITE(V123) { // set stallguard value shaft
  DEBUG_STREAM.print("set M1 stall: ");
  int q=param.asInt()-64;
  if(q>63)q=63;
  if(q<-64)q=-64;
  preferences.putInt("stallguard_1", q);
  DEBUG_STREAM.println(q);
  q&=0x7F;
  q=q<<16;
  sendData(0xED, COOLCONF_DEFAULT|q);     // STALLGUARD_M1
}
BLYNK_WRITE(V124) { // set stallguard value track
  DEBUG_STREAM.print("set M2 stall: ");
  int q=param.asInt()-64;
  DEBUG_STREAM.println(q);
  if(q>63)q=63;
  if(q<-64)q=-64;
  preferences.putInt("stallguard_2", q);
  q&=0x7F;
  q=q<<16;
  sendData(0xFD, COOLCONF_DEFAULT|q);     // STALLGUARD_M2
}

BLYNK_WRITE(V4) {
  TimeInputParam t(param);

  // Process start time
  if (t.hasStartTime())
  {
    times[0].type=0;
    times[0].hour=t.getStartHour();
    times[0].minute=t.getStartMinute();
    times[0].second=t.getStartSecond();
  }
  else if (t.isStartSunrise())
  {
    times[0].type=1;
  }
  else if (t.isStartSunset())
  {
    times[0].type=2;
  }
  else
  {
    // Do nothing
  }
  for (int i = 1; i <= 7; i++) {
    times[0].day_sel[i-1]=t.isWeekdaySelected(i);
  }
  times[0].offset=t.getTZ_Offset();
  save_time(0);
}
BLYNK_WRITE(V6) {
  TimeInputParam t(param);

  // Process start time
  if (t.hasStartTime())
  {
    times[1].type=0;
    times[1].hour=t.getStartHour();
    times[1].minute=t.getStartMinute();
    times[1].second=t.getStartSecond();
  }
  else if (t.isStartSunrise())
  {
    times[1].type=1;
  }
  else if (t.isStartSunset())
  {
    times[1].type=2;
  }
  else
  {
    // Do nothing
  }
  for (int i = 1; i <= 7; i++) {
    times[1].day_sel[i-1]=t.isWeekdaySelected(i);
  }
  times[1].offset=t.getTZ_Offset();
  save_time(1);
}
BLYNK_WRITE(V8) {
  TimeInputParam t(param);

  // Process start time
  if (t.hasStartTime())
  {
    times[2].type=0;
    times[2].hour=t.getStartHour();
    times[2].minute=t.getStartMinute();
    times[2].second=t.getStartSecond();
  }
  else if (t.isStartSunrise())
  {
    times[2].type=1;
  }
  else if (t.isStartSunset())
  {
    times[2].type=2;
  }
  else
  {
    // Do nothing
  }
  for (int i = 1; i <= 7; i++) {
    times[2].day_sel[i-1]=t.isWeekdaySelected(i);
  }
  times[2].offset=t.getTZ_Offset();
  save_time(2);
}
BLYNK_WRITE(V10) {
  TimeInputParam t(param);

  // Process start time
  if (t.hasStartTime())
  {
    times[3].type=0;
    times[3].hour=t.getStartHour();
    times[3].minute=t.getStartMinute();
    times[3].second=t.getStartSecond();
  }
  else if (t.isStartSunrise())
  {
    times[3].type=1;
  }
  else if (t.isStartSunset())
  {
    times[3].type=2;
  }
  else
  {
    // Do nothing
  }
  for (int i = 1; i <= 7; i++) {
    times[3].day_sel[i-1]=t.isWeekdaySelected(i);
  }
  times[3].offset=t.getTZ_Offset();
  save_time(3);
}
// GPS code here
float lslat=-1; // NEVER use these variables in the code above, only below.  They are a security risk otherwise.
float lslon=-1;
float lat;
float lon;
BLYNK_WRITE(V127) {
  GpsParam gps(param);
  lslat=gps.getLat();
  lslon=gps.getLon();
}
BLYNK_WRITE(V126) {
  if(param.asInt()!=0){
    DEBUG_STREAM.println();
    DEBUG_STREAM.print("Lat: ");
    DEBUG_STREAM.println(lslat, 7);
    DEBUG_STREAM.print("Lon: ");
    DEBUG_STREAM.println(lslon, 7);
    if(lslat==-1||lslon==-1){
      DEBUG_STREAM.println("Invalid!");
    }else{
      lon=lslon;
      lat=lslat;
      preferences.putFloat("lat", lat);
      preferences.putFloat("lon", lon);
    }
  }
}
