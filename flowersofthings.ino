
/*__________________________________________________________Libraries__________________________________________________________*/
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <pins_arduino.h>
#include <ArduinoJson.h>

/*__________________________________________________________General_things__________________________________________________________*/
#define ONE_HOUR 3600000UL


#include "DHT.h"
#define DHTPIN 4     // what digital pin the DHT22 is conected to
#define DHTTYPE DHT22   // there are multiple kinds of DHT sensors
DHT dht(DHTPIN, DHTTYPE);
float h = 0;  // variable for air humidity
float t = 0; // variable for air temperature

int humiditylimit = 750;  // soil humidity threshold - when to water
int waterduration = 10; // how long should one watering period last
int waitingtime = 30; // how long should be waited after one watering
uint32_t lastwater = 0; // saves the timestamp of the last watering

ESP8266WebServer server(80);             // create a web server on port 80

File fsUploadFile;                                    // a File variable to temporarily store the received file

ESP8266WiFiMulti wifiMulti;    // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'

const char *OTAName = "ESP8266";         // A name and a password for the OTA service
const char *OTAPassword = "esp8266";

const char* mdnsName = "esp8266";        // Domain name for the mDNS responder

WiFiUDP UDP;                   // Create an instance of the WiFiUDP class to send and receive UDP messages

IPAddress timeServerIP;        // The time.nist.gov NTP server's IP address
const char* ntpServerName = "time.nist.gov";

const int NTP_PACKET_SIZE = 48;          // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[NTP_PACKET_SIZE];      // A buffer to hold incoming and outgoing packets


/*__________________________________________________________Config_file__________________________________________________________*/
bool loadConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println("Failed to parse config file");
    return false;
  }
  
  const String hl = json["humiditylimit"];
  humiditylimit = hl.toInt();
  
 const String wd = json["waterduration"];
  waterduration = wd.toInt();

const String wt = json["waitingtime"];
  waitingtime = wt.toInt();

const String lw = json["lastwater"];
  lastwater = lw.toInt();

  
  // Real world application would store these values in some variables for
  // later use.
Serial.println("values in config.json");
  Serial.println(humiditylimit);
  Serial.println(waterduration);
  Serial.println(waitingtime);
  Serial.println(lastwater);
  return true;
}

/*__________________________________________________________SETUP__________________________________________________________*/

void setup() {
  Serial.begin(115200);        // Start the Serial communication to send messages to the computer
  delay(10);
  Serial.println("\r\n");
  
  dht.begin(); // initalize DHT
  pinMode(D1, OUTPUT); // initialize pin for pump

  startWiFi();                 // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection

  startOTA();                  // Start the OTA service

  startSPIFFS();               // Start the SPIFFS and list all contents

  startMDNS();                 // Start the mDNS responder

  startServer();               // Start a HTTP server with a file read handler and an upload handler

  startUDP();                  // Start listening for UDP messages to port 123

  

  WiFi.hostByName(ntpServerName, timeServerIP); // Get the IP address of the NTP server
  Serial.print("Time server IP:\t");
  Serial.println(timeServerIP);

  sendNTPpacket(timeServerIP);
  delay(500);
}

/*__________________________________________________________LOOP__________________________________________________________*/

const unsigned long intervalNTP = ONE_HOUR; // Update the time every hour
unsigned long prevNTP = 0;
unsigned long lastNTPResponse = millis();

const unsigned long intervalTemp = 6000;   // Do a temperature measurement every minute
unsigned long prevTemp = 0;
bool tmpRequested = false;
const unsigned long DS_delay = 750;         // Reading the temperature from the DS18x20 can take up to 750ms

uint32_t timeUNIX = 0;                      // The most recent timestamp received from the time server

void loop() {

  
  unsigned long currentMillis = millis();

  if (currentMillis - prevNTP > intervalNTP) { // Request the time from the time server every hour
    prevNTP = currentMillis;
    sendNTPpacket(timeServerIP);
  }

  uint32_t time = getTime();                   // Check if the time server has responded, if so, get the UNIX time
  if (time) {
    timeUNIX = time;
    Serial.print("NTP response:\t");
    Serial.println(timeUNIX);
    lastNTPResponse = millis();
  } else if ((millis() - lastNTPResponse) > 24UL * ONE_HOUR) {
    Serial.println("More than 24 hours since last NTP response. Rebooting.");
    Serial.flush();
    ESP.reset();
  }

  if (timeUNIX != 0) {
    if (currentMillis - prevTemp > intervalTemp) {  // Every minute, request the temperature

      
      
      
      //tempSensors.requestTemperatures(); // Request the temperature from the sensor (it takes some time to read it)
      tmpRequested = true;
      prevTemp = currentMillis;
      //Serial.println("Temperature requested");
    }
    if (currentMillis - prevTemp > DS_delay && tmpRequested) { // 750 ms after requesting the temperature
      uint32_t actualTime = timeUNIX + (currentMillis - lastNTPResponse) / 1000;
      // The actual time is the last NTP time plus the time that has elapsed since the last NTP response
      tmpRequested = false;
      //float temp = tempSensors.getTempCByIndex(0); // Get the temperature from the sensor

      h = dht.readHumidity();
      t = dht.readTemperature();
      delay(3000);
      h = dht.readHumidity(); // read twice to avoid nans
      t = dht.readTemperature();
     
      int humidity =  analogRead(0); 
      

      if (humidity>humiditylimit && (actualTime-lastwater)>waitingtime){
        // change here when using multiple soil humidity sensors with multiple if cases testing each sensor
         water_plants("1",waterduration);
         
         lastwater=actualTime;
         changeConfig("lastwater",lastwater);
        }


      Serial.printf("Appending parameters to file at time: %lu, \n", actualTime);
      Serial.println(String(humidity));
      Serial.println(String(t));
      Serial.println(String(h));
      

      File tempLog = SPIFFS.open("/data.csv", "a"); // Write the time and the temperature to the csv file
      tempLog.print(actualTime);
      tempLog.print(',');
      tempLog.print(t);
      tempLog.print(',');
      tempLog.print(h);
      tempLog.print(',');
      tempLog.println(humidity);
      int filesize = tempLog.size();
      Serial.println(String(filesize));
      tempLog.close();

      if(filesize>1500000){
        //remove data if file gets to big, can be done better by deleting just the oldest data
        Serial.println("deleted data file because it was too big");
        SPIFFS.remove("/data.csv");
        }
      
    }
  } else {                                    // If we didn't receive an NTP response yet, send another request
    sendNTPpacket(timeServerIP);
    delay(500);
  }

  server.handleClient();                      // run the server
  ArduinoOTA.handle();                        // listen for OTA events
}

/*__________________________________________________________SETUP_FUNCTIONS__________________________________________________________*/

void startWiFi() { // Try to connect to some given access points. Then wait for a connection
  wifiMulti.addAP("lab","magnusb1");

  Serial.println("Connecting");
  while (wifiMulti.run() != WL_CONNECTED) {  // Wait for the Wi-Fi to connect
    delay(250);
    Serial.print('.');
  }
  Serial.println("\r\n");
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());             // Tell us what network we're connected to
  Serial.print("IP address:\t");
  Serial.print(WiFi.localIP());            // Send the IP address of the ESP8266 to the computer
  Serial.println("\r\n");
}

void startUDP() {
  Serial.println("Starting UDP");
  UDP.begin(123);                          // Start listening for UDP messages to port 123
  Serial.print("Local port:\t");
  Serial.println(UDP.localPort());
}

void startOTA() { // Start the OTA service
  ArduinoOTA.setHostname(OTAName);
  ArduinoOTA.setPassword(OTAPassword);

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\r\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA ready\r\n");
}

void startSPIFFS() { // Start the SPIFFS and list all contents
  SPIFFS.begin();                             // Start the SPI Flash File System (SPIFFS)
  Serial.println("SPIFFS started. Contents:");
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {                      // List the file system contents
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("\tFS File: %s, size: %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    Serial.printf("\n");
  }


//  if (!saveConfig()) {
//    Serial.println("Failed to save config");
//  } else {
//    Serial.println("Config saved");
//  }

  if (!loadConfig()) {
    Serial.println("Failed to load config");
  } else {
    Serial.println("Config loaded");
  }
  
}

void startMDNS() { // Start the mDNS responder
  MDNS.begin(mdnsName);                        // start the multicast domain name server
  Serial.print("mDNS responder started: http://");
  Serial.print(mdnsName);
  Serial.println(".local");
}

void startServer() { // Start a HTTP server with a file read handler and an upload handler
  server.on("/edit.html",  HTTP_POST, []() {  // If a POST request is sent to the /edit.html address,
    server.send(200, "text/plain", "");
  }, handleFileUpload);                       // go to 'handleFileUpload'

    server.on("/water", handleWater);
    server.on("/soil", handleSoil);
    server.on("/hum", handleHum);
    server.on("/temp", handleTemp);
    server.on("/setwaterduration", handleSetWaterDuration);
    server.on("/setwaiting", handleSetWaiting);
    server.on("/sethumidity", handleSetHumidity);
    

  server.onNotFound(handleNotFound);          // if someone requests any other file or page, go to function 'handleNotFound'
  // and check if the file exists

  server.begin();                             // start the HTTP server
  Serial.println("HTTP server started.");
}

/*__________________________________________________________SERVER_HANDLERS__________________________________________________________*/

void handleNotFound() { // if the requested file or page doesn't exist, return a 404 not found error
  if (!handleFileRead(server.uri())) {        // check if the file exists in the flash memory (SPIFFS), if so, send it
    server.send(404, "text/plain", "404: File Not Found");
  }
}

bool handleFileRead(String path) { // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";          // If a folder is requested, send the index file
  String contentType = getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) { // If the file exists, either as a compressed archive, or normal
    if (SPIFFS.exists(pathWithGz))                         // If there's a compressed version available
      path += ".gz";                                         // Use the compressed verion
    File file = SPIFFS.open(path, "r");                    // Open the file
    size_t sent = server.streamFile(file, contentType);    // Send it to the client
    file.close();                                          // Close the file again
    Serial.println(String("\tSent file: ") + path);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path);   // If the file doesn't exist, return false
  return false;
}

void handleFileUpload() { // upload a new file to the SPIFFS
  HTTPUpload& upload = server.upload();
  String path;
  if (upload.status == UPLOAD_FILE_START) {
    path = upload.filename;
    if (!path.startsWith("/")) path = "/" + path;
    if (!path.endsWith(".gz")) {                         // The file server always prefers a compressed version of a file
      String pathWithGz = path + ".gz";                  // So if an uploaded file is not compressed, the existing compressed
      if (SPIFFS.exists(pathWithGz))                     // version of that file must be deleted (if it exists)
        SPIFFS.remove(pathWithGz);
    }
    Serial.print("handleFileUpload Name: "); Serial.println(path);
    fsUploadFile = SPIFFS.open(path, "w");               // Open the file for writing in SPIFFS (create if it doesn't exist)
    path = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {                                   // If the file was successfully created
      fsUploadFile.close();                               // Close the file again
      Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
      server.sendHeader("Location", "/success.html");     // Redirect the client to the success page
      server.send(303);
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}

/*__________________________________________________________HELPER_FUNCTIONS__________________________________________________________*/

String formatBytes(size_t bytes) { // convert sizes in bytes to KB and MB
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  }
}

String getContentType(String filename) { // determine the filetype of a given filename, based on the extension
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

unsigned long getTime() { // Check if the time server has responded, if so, get the UNIX time, otherwise, return 0
  if (UDP.parsePacket() == 0) { // If there's no response (yet)
    return 0;
  }
  UDP.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
  // Combine the 4 timestamp bytes into one 32-bit number
  uint32_t NTPTime = (packetBuffer[40] << 24) | (packetBuffer[41] << 16) | (packetBuffer[42] << 8) | packetBuffer[43];
  // Convert NTP time to a UNIX timestamp:
  // Unix time starts on Jan 1 1970. That's 2208988800 seconds in NTP time:
  const uint32_t seventyYears = 2208988800UL;
  // subtract seventy years:
  uint32_t UNIXTime = NTPTime - seventyYears;
  return UNIXTime;
}


void sendNTPpacket(IPAddress& address) {
  Serial.println("Sending NTP request");
  memset(packetBuffer, 0, NTP_PACKET_SIZE);  // set all bytes in the buffer to 0
  // Initialize values needed to form NTP request
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode

  // send a packet requesting a timestamp:
  UDP.beginPacket(address, 123); // NTP requests are to port 123
  UDP.write(packetBuffer, NTP_PACKET_SIZE);
  UDP.endPacket();
}

/*__________________________________________________________flowersofthings_functions__________________________________________________________*/

void water_plants(String plant, int duration) {
  if (plant == "1") {
         //digitalWrite(D2, 1); // switch all valves
         //digitalWrite(D3, 1); // switch all valves
         //delay(250); //wait
         digitalWrite(D1, 1); // turn on pump
         delay(duration*250);
         digitalWrite(D1, 0);
      }

if (plant == "2") {
// add valves that have to be switched
// add pump
      }
  }


void handleWater() { 
  
String plant = "";

int duration = 0;

if (server.arg("plant")== ""){     //Parameter not found

Serial.println("Argument not found");

}else{     //Parameter found

plant = server.arg("plant");     //Gets the value of the query parameter

}

if (server.arg("duration")== ""){     //Parameter not found

Serial.println("Argument not found");

}else{     //Parameter found

duration = server.arg("duration").toInt();     //Gets the value of the query parameter

}

water_plants(plant,duration);

Serial.println("manually water plant "+String(plant)+" for "+String(duration*250)+" seconds");

server.send(200, "text/plain", "OK_"+String(plant)+"_"+String(duration));          //Returns the HTTP response

}

void handleSoil() { 
Serial.println("Soil");

int humidity = analogRead(A0);

Serial.println("soil humidity requested manually "+String(humidity));

server.send(200, "text/plain", String(humidity));          //Returns the HTTP response

}

void handleTemp() { 
Serial.println("Temp");

float t = dht.readTemperature();

Serial.println("temperature requested manually "+String(t));

server.send(200, "text/plain", String(t));          //Returns the HTTP response

}


void handleHum() { 

float h = dht.readHumidity();

Serial.println("humidity requested manually "+String(h));

server.send(200, "text/plain", String(h));          //Returns the HTTP response

}

void changeConfig(String key,int var){

   File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println("Failed to parse config file");
  }
  
  Serial.println(key+String(var));
  json[key] = String(var);

  if (!configFile) {
    Serial.println("Failed to open config file for writing");
  }

  SPIFFS.remove("/config.json");
  File newconfigFile = SPIFFS.open("/config.json", "w");
  
  json.printTo(newconfigFile);

  }
  

void handleSetHumidity() { 

if (server.arg("humidity")== ""){     //Parameter not found

server.send(200, "text/plain", "Argument not found");          //Returns the HTTP response

}else{     //Parameter found

humiditylimit = server.arg("humidity").toInt();     //Gets the value of the query parameter

changeConfig("humiditylimit",humiditylimit);

server.send(200, "text/plain", "OK_"+String(humiditylimit));          //Returns the HTTP response
}

}


void handleSetWaterDuration() { 

if (server.arg("duration")== ""){     //Parameter not found

server.send(200, "text/plain", "Argument not found");          //Returns the HTTP response

}else{     //Parameter found

waterduration = server.arg("duration").toInt();     //Gets the value of the query parameter

changeConfig("waterduration",waterduration);

server.send(200, "text/plain", "OK_"+String(waterduration));          //Returns the HTTP response
}

}


void handleSetWaiting() { 

if (server.arg("time")== ""){     //Parameter not found

server.send(200, "text/plain", "Argument not found");          //Returns the HTTP response

}else{     //Parameter found

waitingtime = server.arg("time").toInt();     //Gets the value of the query parameter

changeConfig("waitingtime",waitingtime);

server.send(200, "text/plain", "OK_"+String(waitingtime));          //Returns the HTTP response
}

}
