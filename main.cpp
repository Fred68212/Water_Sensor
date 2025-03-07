
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Arduino_JSON.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>
#include <sys/unistd.h>
#include <esp_sleep.h>
#include <secrets.h>

// unsigned long long uS_TO_S_FACTOR = 1000000;  // Conversion factor for microseconds to seconds
// unsigned long long TIME_TO_SLEEP = 5 * 60; // Time ESP32 will go to sleep (in minutes)

// Identify the unit
//const String unitID = "Basement_Sink";
const char* hostName = "watersensor";

// Static IP configuration
IPAddress staticIP(192, 168, 1, 40); // ESP32 static IP
IPAddress gateway(192, 168, 1, 1);    // IP Address of your network gateway (router)
IPAddress subnet(255, 255, 255, 0);   // Subnet mask
IPAddress primaryDNS(192, 168, 1, 11); // Primary DNS (optional)
IPAddress secondaryDNS(0, 0, 0, 0);   // Secondary DNS (optional)


// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create a WebSocket object
AsyncWebSocket ws("/ws");

// Json Variable to Hold Sensor Readings
JSONVar readings;

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 10 * 60000; // 10 minutes

// HC-SR04 Variables and Pin assignments
const int trigPin = 5;
const int echoPin = 18;

// define sound speed in cm/uS
#define SOUND_SPEED 0.034
#define CM_TO_INCH 0.393701

// Variables for the duration and distance
long duration;
unsigned long distanceCm;
unsigned long distanceInch;
int inches;

// Save last reading. If current reading is the same as last reading do not send it
int lastReading = 0; 

int ledPin = 2;

// int number;

// Server information
const String dbServer PROGMEM = "http://192.168.1.11/iot.php?inches=";
const String statusServer PROGMEM = "http://192.168.1.11/status.php?unitID=Basement_Sink&unitStatus=";

String systemStatus = "ok";
// Alarm variables
volatile int lastAlarm = 0;

RTC_DS3231 rtc; // Initialize an instance of the RTC_DS3231 class

// the pin that is connected to SQW
#define CLOCK_INTERRUPT_PIN 4

// Function to be called when the alarm goes off
void onAlarm() {
 
  // Set the flag
  // digitalWrite(ledPin, HIGH); // Turn on LED for troubleshooting

  if (lastAlarm == 1) {
    lastAlarm = 0;  // set alarm 1 flag to false  
  } else {
    lastAlarm = 1;  // set alarm 1 flag to true
  }
  
} 

void printTwoDigits(int number) {

  if (number < 10) {
  
  Serial.print("0"); // Add a leading zero for single-digit numbers
  
  }
  
  Serial.print(number);
  
  }

void showTime() {

  int number;
  DateTime now = rtc.now();
  Serial.println();
  Serial.print("Current Date: ");
  Serial.print(now.month(), DEC);
  Serial.print("/");
  Serial.print(now.day(), DEC);
  Serial.print("/");
  Serial.print(now.year(), DEC);
  Serial.print(" Current Time: ");
  Serial.print(now.hour(), DEC);
  Serial.print(":");
  number = (now.minute());
  printTwoDigits(number);
  Serial.print(":");
  number = (now.second());
  printTwoDigits(number);
  Serial.println();

}

/*
void printAlarm() {
  // Is alarm set and for when
  DateTime alarm = rtc.getAlarm1();
  Serial.print("Alarm set for: ");
  printTwoDigits(alarm.hour());
  Serial.print(":");
  printTwoDigits(alarm.minute());
  Serial.println();
}
*/

void dbInsert(const char* urlString) {
  HTTPClient http;
  http.begin(urlString);
  int httpCode = http.GET();
  if (httpCode > 0) { //Check for the returning code
    if (httpCode == HTTP_CODE_OK) { 
      // get payload with http.getString();
      Serial.println(httpCode);
      // Serial.println(payload);
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
  } else {
    Serial.println(F("Error on HTTP request"));
  }
  Serial.flush();
  http.end();
}


// Get Sensor Readings and return JSON object
String getSensorReadings() {
  showTime();
  Serial.println();
//  Serial.println(lastAlarm);
    // Clears the trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);
  
  // Calculate the distance
  distanceCm = duration * SOUND_SPEED/2;
  
  // Convert to inches
  distanceInch = distanceCm * CM_TO_INCH;

  // Convert to integer to help stop sending the same reading
  inches = int(distanceInch);

  readings["distanceInch"] = String(distanceInch);
  String jsonString = JSON.stringify(readings);

  if (inches != lastReading) {
    lastReading = inches;
  // Send the distance to the server
    String urlString = dbServer + String(distanceInch);
    Serial.println(urlString);
    dbInsert(urlString.c_str());
  }

  showTime();

  // Prints the distance in the Serial Monitor
  Serial.println();
  Serial.print(F("Distance (cm): "));
  Serial.println(distanceCm);
  Serial.print(F("Distance (inch): "));
  Serial.println(distanceInch);
  Serial.println();
  /*
  readings["distanceInch"] = String(distanceInch);
  String jsonString = JSON.stringify(readings);
  return jsonString;
  */
  return jsonString;  
}

// Initialize LittleFS
void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println(F("An error has occurred while mounting LittleFS"));
  }
  Serial.println(F("LittleFS mounted successfully"));
}

// Initialize WiFi
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
    // Configuring static IP
  if(!WiFi.config(staticIP, gateway, subnet, primaryDNS, secondaryDNS)) {
      Serial.println(F("Failed to configure Static IP"));
  } else {
      Serial.println(F("Static IP configured!"));
      WiFi.setHostname(hostName);
  }
    
  Serial.print(F("ESP32 IP Address: "));
  Serial.println(WiFi.localIP());  // Print the ESP32 IP address to Serial Monitor
   
  Serial.println(WiFi.localIP());
  Serial.print(F("Hostname: "));
  Serial.println(WiFi.getHostname());
}

void dbInsert(const char* urlString);

void notifyClients(String sensorReadings) {
  ws.textAll(sensorReadings);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      String sensorReadings = getSensorReadings();
      Serial.print(sensorReadings);
      notifyClients(sensorReadings);
    
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup() {
  setCpuFrequencyMhz(80);
  Serial.begin(115200);
  Wire.begin();
  initWiFi();
  initLittleFS();
  initWebSocket();
  Serial.println("CPU Frequency: " + String(getCpuFrequencyMhz()));

// HC-SR04
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT); // Sets the echoPin as an Input
  pinMode(ledPin, OUTPUT); // Turn on LED for troubleshooting

  digitalWrite(ledPin, LOW); // Turn on LED for troubleshooting

// initialize the RTC
  if (!rtc.begin()) {

    Serial.println(F("RTC not detected"));

    while (1); // Hang indefinitely if RTC is not found

  }

//Uncomment the below line to set the initial date and time

//rtc.adjust(DateTime(__DATE__, __TIME__));  

// Set the SQW pin to generate a 1Hz signal
  rtc.writeSqwPinMode(DS3231_SquareWave1Hz);

// Show the current time in the Serial Monitor  
  showTime();

  pinMode(CLOCK_INTERRUPT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CLOCK_INTERRUPT_PIN), onAlarm, FALLING);

// set alarm 1, 2 flag to false (so alarm 1, 2 didn't happen so far)
// if not done, this easily leads to problems, as both register aren't reset on reboot/recompile
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);
// stop oscillating signals at SQW Pin otherwise setAlarm1 will fail
  rtc.writeSqwPinMode(DS3231_OFF);

  // Set the alarm to go off at 9:00 AM
  rtc.setAlarm1(DateTime(0, 0, 0, 9, 0, 0), DS3231_A1_Hour);
 // rtc.setAlarm1(rtc.now() + TimeSpan(0, 0, 20, 0), DS3231_A1_Minute);
 // printAlarm();
  // Is alarm set and for when
  DateTime alarm = rtc.getAlarm1();
  Serial.print("Alarm set for: ");
  printTwoDigits(alarm.hour());
  Serial.print(":");
  printTwoDigits(alarm.minute());
  Serial.println();
 

  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.serveStatic("/", LittleFS, "/");

  // Start server
  server.begin();
  systemStatus = "Running";
  String urlString = statusServer + systemStatus;
  Serial.println(urlString);
  dbInsert(urlString.c_str());
  
}

void loop() {
// Check if the alarm flag is set and set pm alarm if it is not set
// Put ESP32 to sleep at 21:00

  digitalWrite(ledPin, LOW); // Turn off LED for troubleshooting

  if (rtc.now().hour() == 21 && rtc.now().minute() == 3) {

    systemStatus = "Sleeping";
    String urlString = statusServer + systemStatus;
    Serial.println(urlString);
    dbInsert(urlString.c_str());
    Serial.println(F("Going to sleep..."));
    Serial.flush();
    delay(5000);
//    esp_err_t rtc_gpio_pullup_en(GPIO_NUM_4);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_4, 0);
    esp_deep_sleep_start();
  }
 
  // Wake up ESP32 at 9:00 AM
  if (rtc.alarmFired(1)) {
    Serial.println("Waking up...");
    rtc.clearAlarm(1); // Clear the alarm flag
  }

  if ((millis() - lastTime) > timerDelay) {
    String sensorReadings = getSensorReadings();
    Serial.print(sensorReadings);
    notifyClients(sensorReadings);
    lastTime = millis();
  }
    
  ws.cleanupClients();
  
}
