#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>

// ============ CONFIGURATION ============
const char* WIFI_SSID = "";
const char* WIFI_PASSWORD = "";
const char* NTP_SERVER = "pool.ntp.org";

// Timezone configuration using POSIX timezone strings
// For London (GMT/BST with automatic DST):
const char* TZ_INFO = "GMT0BST,M3.5.0/1,M10.5.0";

// Other timezone examples (POSIX format):
// UTC: "UTC0"
// US Eastern (EST/EDT): "EST5EDT,M3.2.0,M11.1.0"
// US Pacific (PST/PDT): "PST8PDT,M3.2.0,M11.1.0"
// CET/CEST (Central Europe): "CET-1CEST,M3.5.0,M10.5.0"
// IST (India): "IST-5:30"

// LED Configuration
const int WARM_PIN = 18;
const int COOL_PIN = 19;
const int PWM_FREQ = 1000;                        // 1 kHz PWM frequency
const int PWM_RESOLUTION = 8;                     // 8-bit resolution (0-255)
const int PWM_CHANNEL_WARM = 0;
const int PWM_CHANNEL_COOL = 1;

// Sunrise Configuration
const int SUNRISE_DURATION_MINUTES = 15;          // Duration of sunrise fade
const unsigned long SUNRISE_DURATION_MS = SUNRISE_DURATION_MINUTES * 60 * 1000;

// ============ GLOBAL VARIABLES ============
Preferences preferences;
WebServer server(80);

struct {
  int hour = 6;
  int minute = 30;
  bool isAlarmSet = false;
  bool isSunriseActive = false;
  unsigned long sunriseStartTime = 0;
  int currentWarmBrightness = 0;
  int currentCoolBrightness = 0;
} alarmState;

// ============ FUNCTION DECLARATIONS ============
void setupWiFi();
void setupNTP();
void setupWebServer();
void setupLED();
void handleSetAlarm();
void handleGetAlarm();
void handleManualOn();
void handleManualOff();
void handleStatus();
void handleNotFound();
void loadAlarmFromStorage();
void saveAlarmToStorage();
void updateSunrise();
void setBrightness(int warm, int cool);
void startSunrise();

// ============ SETUP ============
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\nStarting Wake-Up LED Strip...");

  // Initialize preferences for persistent storage
  preferences.begin("alarm", false);

  setupLED();
  setupWiFi();
  setupNTP();
  setupWebServer();

  loadAlarmFromStorage();

  Serial.println("Setup complete!");
}

// ============ MAIN LOOP ============
void loop() {
  server.handleClient();
  updateSunrise();
  delay(100);
}

// ============ LED SETUP ============
void setupLED() {
  Serial.println("Setting up LED pins...");

  // Configure PWM channels
  ledcSetup(PWM_CHANNEL_WARM, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_COOL, PWM_FREQ, PWM_RESOLUTION);

  // Attach pins to PWM channels
  ledcAttachPin(WARM_PIN, PWM_CHANNEL_WARM);
  ledcAttachPin(COOL_PIN, PWM_CHANNEL_COOL);

  // Start with lights off
  setBrightness(0, 0);

  Serial.println("LED setup complete");
}

// ============ WiFi SETUP ============
void setupWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi");
  }
}

// ============ NTP SETUP ============
void setupNTP() {
  Serial.println("Setting up NTP time synchronization...");

  // Configure time with NTP server and POSIX timezone string (handles DST automatically)
  configTime(0, 0, NTP_SERVER, "time.nist.gov", "time.google.com");
  setenv("TZ", TZ_INFO, 1);
  tzset();

  // Wait for time to be set
  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  int attempts = 0;
  while (now < 24 * 3600 && attempts < 20) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    attempts++;
  }

  Serial.println();
  struct tm timeinfo = *localtime(&now);
  Serial.print("Current time: ");
  Serial.println(asctime(&timeinfo));
}

// ============ WEB SERVER SETUP ============
void setupWebServer() {
  Serial.println("Setting up REST API server...");

  server.on("/set-alarm", HTTP_POST, handleSetAlarm);
  server.on("/get-alarm", HTTP_GET, handleGetAlarm);
  server.on("/manual-on", HTTP_POST, handleManualOn);
  server.on("/manual-off", HTTP_POST, handleManualOff);
  server.on("/status", HTTP_GET, handleStatus);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Web server started on port 80");
}

// ============ WEB HANDLERS ============
void handleSetAlarm() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "No body");
    return;
  }

  String body = server.arg("plain");

  // Simple JSON parsing (looking for "hour" and "minute")
  int hourPos = body.indexOf("\"hour\":");
  int minutePos = body.indexOf("\"minute\":");

  if (hourPos == -1 || minutePos == -1) {
    server.send(400, "text/plain", "Invalid JSON format");
    return;
  }

  int hour = atoi(body.c_str() + hourPos + 7);
  int minute = atoi(body.c_str() + minutePos + 9);

  if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
    server.send(400, "text/plain", "Invalid time values");
    return;
  }

  alarmState.hour = hour;
  alarmState.minute = minute;
  alarmState.isAlarmSet = true;

  saveAlarmToStorage();

  String response = "Alarm set to " + String(hour) + ":" + String(minute < 10 ? "0" : "") + String(minute);
  server.send(200, "text/plain", response);

  Serial.println(response);
}

void handleGetAlarm() {
  String response = "{\"hour\":" + String(alarmState.hour) + ",\"minute\":" + String(alarmState.minute) + ",\"isSet\":" + (alarmState.isAlarmSet ? "true" : "false") + "}";
  server.send(200, "application/json", response);
}

void handleManualOn() {
  alarmState.isSunriseActive = false;
  setBrightness(255, 255);
  server.send(200, "text/plain", "Lights turned on");
  Serial.println("Manual: Lights turned on");
}

void handleManualOff() {
  alarmState.isSunriseActive = false;
  setBrightness(0, 0);
  server.send(200, "text/plain", "Lights turned off");
  Serial.println("Manual: Lights turned off");
}

void handleStatus() {
  time_t now = time(nullptr);
  struct tm timeinfo = *localtime(&now);

  char timeStr[20];
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);

  String response = "{";
  response += "\"currentTime\":\"" + String(timeStr) + "\",";
  response += "\"alarmTime\":\"" + String(alarmState.hour) + ":" + String(alarmState.minute < 10 ? "0" : "") + String(alarmState.minute) + "\",";
  response += "\"isAlarmSet\":" + String(alarmState.isAlarmSet ? "true" : "false") + ",";
  response += "\"isSunriseActive\":" + String(alarmState.isSunriseActive ? "true" : "false") + ",";
  response += "\"warmBrightness\":" + String(alarmState.currentWarmBrightness) + ",";
  response += "\"coolBrightness\":" + String(alarmState.currentCoolBrightness);
  response += "}";

  server.send(200, "application/json", response);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}

// ============ STORAGE FUNCTIONS ============
void saveAlarmToStorage() {
  preferences.putInt("alarm_hour", alarmState.hour);
  preferences.putInt("alarm_min", alarmState.minute);
  preferences.putBool("alarm_set", alarmState.isAlarmSet);
  Serial.println("Alarm saved to persistent storage");
}

void loadAlarmFromStorage() {
  alarmState.hour = preferences.getInt("alarm_hour", 6);
  alarmState.minute = preferences.getInt("alarm_min", 30);
  alarmState.isAlarmSet = preferences.getBool("alarm_set", false);
  Serial.printf("Alarm loaded: %d:%02d (Set: %s)\n", alarmState.hour, alarmState.minute, alarmState.isAlarmSet ? "Yes" : "No");
}

// ============ LED CONTROL FUNCTIONS ============
void setBrightness(int warm, int cool) {
  // Clamp values to 0-255
  warm = constrain(warm, 0, 255);
  cool = constrain(cool, 0, 255);

  alarmState.currentWarmBrightness = warm;
  alarmState.currentCoolBrightness = cool;

  ledcWrite(PWM_CHANNEL_WARM, warm);
  ledcWrite(PWM_CHANNEL_COOL, cool);
}

// ============ SUNRISE LOGIC ============
void startSunrise() {
  Serial.println("Starting sunrise...");
  alarmState.isSunriseActive = true;
  alarmState.sunriseStartTime = millis();
}

void updateSunrise() {
  if (!alarmState.isSunriseActive) {
    // Check if we need to start sunrise
    if (!alarmState.isAlarmSet) {
      return;
    }

    time_t now = time(nullptr);
    struct tm timeinfo = *localtime(&now);

    if (timeinfo.tm_hour == alarmState.hour && timeinfo.tm_min == alarmState.minute) {
      startSunrise();
    }
    return;
  }

  // Sunrise is active - update brightness
  unsigned long elapsedTime = millis() - alarmState.sunriseStartTime;

  if (elapsedTime >= SUNRISE_DURATION_MS) {
    // Sunrise complete - set to full warm light
    setBrightness(255, 0);
    alarmState.isSunriseActive = false;
    Serial.println("Sunrise complete!");
    return;
  }

  // Calculate brightness levels
  // Start with cool light (dawn), transition to warm light (sunrise)
  float progress = (float)elapsedTime / SUNRISE_DURATION_MS;

  int coolBrightness = (int)(255 * (1.0 - progress));      // Cool fades out
  int warmBrightness = (int)(255 * progress);                // Warm fades in

  setBrightness(warmBrightness, coolBrightness);
}
