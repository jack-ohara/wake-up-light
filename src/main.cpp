#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <time.h>
#include <cmath>

// ============ CONFIGURATION ============
const char *WIFI_SSID = "";
const char *WIFI_PASSWORD = "";
const char *NTP_SERVER = "pool.ntp.org";

// Timezone configuration using POSIX timezone strings
// For London (GMT/BST with automatic DST):
const char *TZ_INFO = "GMT0BST,M3.5.0/1,M10.5.0";

// Other timezone examples (POSIX format):
// UTC: "UTC0"
// US Eastern (EST/EDT): "EST5EDT,M3.2.0,M11.1.0"
// US Pacific (PST/PDT): "PST8PDT,M3.2.0,M11.1.0"
// CET/CEST (Central Europe): "CET-1CEST,M3.5.0,M10.5.0"
// IST (India): "IST-5:30"

// LED Configuration
const int WARM_PIN = 18;
const int COOL_PIN = 19;
const int PWM_FREQ = 5000;     // 5 kHz PWM frequency (increased for better linearity)
const int PWM_RESOLUTION = 10; // 10-bit resolution (0-1023) for finer control
const int PWM_CHANNEL_WARM = 0;
const int PWM_CHANNEL_COOL = 1;

// Sunrise Configuration
const int SUNRISE_DURATION_MINUTES = 15; // Duration of sunrise fade
const unsigned long SUNRISE_DURATION_MS = SUNRISE_DURATION_MINUTES * 60 * 1000;
// Manual fade configuration (milliseconds)
const unsigned long MANUAL_FADE_MS = 350; // 350 milliseconds fade for manual on/off
// Auto-off configuration
const int DEFAULT_AUTO_OFF_MINUTES = 45; // Default time to auto-off after sunrise completes

// ============ GLOBAL VARIABLES ============
Preferences preferences;
WebServer server(80);

struct
{
  int hour = 8;
  int minute = 30;
  bool isAlarmSet = false;
  bool isSunriseActive = false;
  unsigned long sunriseStartTime = 0;
  int currentWarmBrightness = 0;
  int currentCoolBrightness = 0;
  // Manual fade state
  bool isManualFadeActive = false;
  unsigned long manualFadeStartTime = 0;
  unsigned long manualFadeDuration = 0;
  int manualStartWarm = 0;
  int manualStartCool = 0;
  int manualTargetWarm = 0;
  int manualTargetCool = 0;
  // Auto-off configuration and state
  bool autoOffEnabled = true;
  int autoOffMinutes = DEFAULT_AUTO_OFF_MINUTES;
  unsigned long sunriseCompleteTime = 0;
  bool autoOffScheduled = false;
} alarmState;

// ============ FUNCTION DECLARATIONS ============
void setupWiFi();
void setupNTP();
void setupWebServer();
void setupLED();
void setupOTA();
void handleSetAlarm();
void handleGetAlarm();
void handleManualOn();
void handleManualOff();
void handleSetBrightness();
void handleToggleAlarm();
void handleStatus();
void handleNotFound();
void loadAlarmFromStorage();
void saveAlarmToStorage();
void updateSunrise();
void updateManualFade();
void updateAutoOff();
void handleSetAutoOff();
void handleGetAutoOff();

// Smoothstep easing function: starts and ends gently
static float smoothstepf(float x)
{
  if (x <= 0.0f)
    return 0.0f;
  if (x >= 1.0f)
    return 1.0f;
  return x * x * (3.0f - 2.0f * x);
}

// Smoother ease-in-out using a sine curve
static float easeInOutSine(float x)
{
  if (x <= 0.0f)
    return 0.0f;
  if (x >= 1.0f)
    return 1.0f;
  return 0.5f * (1.0f - cosf(x * M_PI));
}

// Apply simple gamma correction for perceptual brightness
// Gamma settings
const float DEFAULT_GAMMA = 2.2f;
const float SUNRISE_GAMMA = 1.0f; // linear response during sunrise for smooth fade-up

// applyGamma: map 0-1023 value through gamma correction
static int applyGamma(int v, float gamma)
{
  // clamp
  if (v <= 0)
    return 0;
  if (v >= 1023)
    return 1023;
  float normalized = (float)v / 1023.0f;
  float corrected = powf(normalized, gamma);
  return (int)(corrected * 1023.0f + 0.5f);
}
void setBrightness(int warm, int cool);
void startSunrise();

// ============ SETUP ============
void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\nStarting Wake-Up LED Strip...");

  // Initialize preferences for persistent storage
  preferences.begin("alarm", false);

  setupLED();
  setupWiFi();
  setupNTP();
  setupWebServer();
  setupOTA();

  loadAlarmFromStorage();

  Serial.println("Setup complete!");
}

// ============ MAIN LOOP ============
void loop()
{
  ArduinoOTA.handle(); // Handle OTA updates
  server.handleClient();
  // Update manual fading (if active) and sunrise logic
  updateManualFade();
  updateSunrise();
  updateAutoOff(); // Check if auto-off should trigger
  // Faster update interval for smoother fades
  delay(20);
}

// ============ LED SETUP ============
void setupLED()
{
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
void setupWiFi()
{
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20)
  {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("\nFailed to connect to WiFi");
  }
}

// ============ NTP SETUP ============
void setupNTP()
{
  Serial.println("Setting up NTP time synchronization...");

  // Configure time with NTP server and POSIX timezone string (handles DST automatically)
  configTime(0, 0, NTP_SERVER, "time.nist.gov", "time.google.com");
  setenv("TZ", TZ_INFO, 1);
  tzset();

  // Wait for time to be set
  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  int attempts = 0;
  while (now < 24 * 3600 && attempts < 20)
  {
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
void setupWebServer()
{
  Serial.println("Setting up REST API server...");

  // Enable CORS for all responses
  server.enableCORS(true);

  // OPTIONS handlers for preflight requests
  server.on("/set-alarm", HTTP_OPTIONS, []()
            { server.send(204); });
  server.on("/get-alarm", HTTP_OPTIONS, []()
            { server.send(204); });
  server.on("/manual-on", HTTP_OPTIONS, []()
            { server.send(204); });
  server.on("/manual-off", HTTP_OPTIONS, []()
            { server.send(204); });
  server.on("/status", HTTP_OPTIONS, []()
            { server.send(204); });
  server.on("/set-brightness", HTTP_OPTIONS, []()
            { server.send(204); });
  server.on("/toggle-alarm", HTTP_OPTIONS, []()
            { server.send(204); });
  server.on("/set-auto-off", HTTP_OPTIONS, []()
            { server.send(204); });
  server.on("/get-auto-off", HTTP_OPTIONS, []()
            { server.send(204); });

  // Actual endpoint handlers
  server.on("/set-alarm", HTTP_POST, handleSetAlarm);
  server.on("/get-alarm", HTTP_GET, handleGetAlarm);
  server.on("/manual-on", HTTP_POST, handleManualOn);
  server.on("/manual-off", HTTP_POST, handleManualOff);
  server.on("/set-brightness", HTTP_POST, handleSetBrightness);
  server.on("/toggle-alarm", HTTP_POST, handleToggleAlarm);
  server.on("/set-auto-off", HTTP_POST, handleSetAutoOff);
  server.on("/get-auto-off", HTTP_GET, handleGetAutoOff);
  server.on("/status", HTTP_GET, handleStatus);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Web server started on port 80");
}

// ============ OTA UPDATE SETUP ============
void setupOTA()
{
  // Set hostname for OTA updates
  ArduinoOTA.setHostname("wake-up-light");

  // Set authentication password (optional, but recommended)
  // ArduinoOTA.setPassword("your_ota_password");

  ArduinoOTA.onStart([]()
                     { Serial.println("OTA: Starting update..."); });

  ArduinoOTA.onEnd([]()
                   { Serial.println("\nOTA: Update finished!"); });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        { Serial.printf("OTA: Progress: %u%%\r", (progress / (total / 100))); });

  ArduinoOTA.onError([](ota_error_t error)
                     {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed"); });

  ArduinoOTA.begin();
  Serial.println("OTA ready - device can be updated wirelessly");
}

// ============ WEB HANDLERS ============
void handleSetAlarm()
{
  if (!server.hasArg("plain"))
  {
    server.send(400, "text/plain", "No body");
    return;
  }

  String body = server.arg("plain");

  // Simple JSON parsing (looking for "hour" and "minute")
  int hourPos = body.indexOf("\"hour\":");
  int minutePos = body.indexOf("\"minute\":");

  if (hourPos == -1 || minutePos == -1)
  {
    server.send(400, "text/plain", "Invalid JSON format");
    return;
  }

  int hour = atoi(body.c_str() + hourPos + 7);
  int minute = atoi(body.c_str() + minutePos + 9);

  if (hour < 0 || hour > 23 || minute < 0 || minute > 59)
  {
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

void handleGetAlarm()
{
  String response = "{\"hour\":" + String(alarmState.hour) + ",\"minute\":" + String(alarmState.minute) + ",\"isSet\":" + (alarmState.isAlarmSet ? "true" : "false") + "}";
  server.send(200, "application/json", response);
}

void handleManualOn()
{
  // Cancel sunrise and start a manual fade up to full brightness
  alarmState.isSunriseActive = false;
  alarmState.isManualFadeActive = true;
  alarmState.manualFadeStartTime = millis();
  alarmState.manualFadeDuration = MANUAL_FADE_MS;
  alarmState.manualStartWarm = alarmState.currentWarmBrightness;
  alarmState.manualStartCool = alarmState.currentCoolBrightness;
  alarmState.manualTargetWarm = 1023;
  alarmState.manualTargetCool = 1023;

  server.send(200, "text/plain", "Lights fading on");
  Serial.println("Manual: fading lights on");
}

void handleManualOff()
{
  // Cancel sunrise and start a manual fade down to zero
  alarmState.isSunriseActive = false;
  alarmState.isManualFadeActive = true;
  alarmState.manualFadeStartTime = millis();
  alarmState.manualFadeDuration = MANUAL_FADE_MS;
  alarmState.manualStartWarm = alarmState.currentWarmBrightness;
  alarmState.manualStartCool = alarmState.currentCoolBrightness;
  alarmState.manualTargetWarm = 0;
  alarmState.manualTargetCool = 0;

  server.send(200, "text/plain", "Lights fading off");
  Serial.println("Manual: fading lights off");
}

void handleSetBrightness()
{
  if (!server.hasArg("plain"))
  {
    server.send(400, "text/plain", "No body");
    return;
  }

  String body = server.arg("plain");

  // Simple JSON parsing (looking for "warm" and "cool")
  int warmPos = body.indexOf("\"warm\":");
  int coolPos = body.indexOf("\"cool\":");

  if (warmPos == -1 || coolPos == -1)
  {
    server.send(400, "text/plain", "Invalid JSON format");
    return;
  }

  int warm = atoi(body.c_str() + warmPos + 7);
  int cool = atoi(body.c_str() + coolPos + 7);

  if (warm < 0 || warm > 1023 || cool < 0 || cool > 1023)
  {
    server.send(400, "text/plain", "Invalid brightness values (must be 0-1023)");
    return;
  }

  // Cancel any active sunrise
  alarmState.isSunriseActive = false;

  // Start a manual fade to the target brightness values
  alarmState.isManualFadeActive = true;
  alarmState.manualFadeStartTime = millis();
  alarmState.manualFadeDuration = MANUAL_FADE_MS;
  alarmState.manualStartWarm = alarmState.currentWarmBrightness;
  alarmState.manualStartCool = alarmState.currentCoolBrightness;
  alarmState.manualTargetWarm = warm;
  alarmState.manualTargetCool = cool;

  String response = "{\"warm\":" + String(warm) + ",\"cool\":" + String(cool) + ",\"fading\":true}";
  server.send(200, "application/json", response);

  Serial.printf("Brightness fading to: warm=%d cool=%d\n", warm, cool);
}

void handleToggleAlarm()
{
  if (!server.hasArg("plain"))
  {
    server.send(400, "text/plain", "No body");
    return;
  }

  String body = server.arg("plain");

  // Simple JSON parsing (looking for "enabled")
  int enabledPos = body.indexOf("\"enabled\":");

  if (enabledPos == -1)
  {
    server.send(400, "text/plain", "Invalid JSON format");
    return;
  }

  // Parse boolean value
  bool enabled = false;
  if (body.indexOf("true", enabledPos) != -1)
  {
    enabled = true;
  }
  else if (body.indexOf("false", enabledPos) == -1)
  {
    server.send(400, "text/plain", "Invalid boolean value for enabled");
    return;
  }

  alarmState.isAlarmSet = enabled;

  // If disabling, cancel any active sunrise
  if (!enabled)
  {
    alarmState.isSunriseActive = false;
  }

  saveAlarmToStorage();

  String response = "{\"isAlarmSet\":" + String(enabled ? "true" : "false") +
                    ",\"alarmTime\":\"" + String(alarmState.hour) + ":" +
                    String(alarmState.minute < 10 ? "0" : "") + String(alarmState.minute) + "\"}";
  server.send(200, "application/json", response);

  Serial.printf("Alarm %s\n", enabled ? "enabled" : "disabled");
}

void handleStatus()
{
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

void handleNotFound()
{
  server.send(404, "text/plain", "Not Found");
}

void handleSetAutoOff()
{
  if (!server.hasArg("plain"))
  {
    server.send(400, "text/plain", "No body");
    return;
  }

  String body = server.arg("plain");

  // Simple JSON parsing (looking for "enabled" and "minutes")
  int enabledPos = body.indexOf("\"enabled\":");
  int minutesPos = body.indexOf("\"minutes\":");

  if (enabledPos == -1 || minutesPos == -1)
  {
    server.send(400, "text/plain", "Invalid JSON format");
    return;
  }

  // Parse boolean value for enabled
  bool enabled = false;
  if (body.indexOf("true", enabledPos) != -1)
  {
    enabled = true;
  }
  else if (body.indexOf("false", enabledPos) == -1)
  {
    server.send(400, "text/plain", "Invalid boolean value for enabled");
    return;
  }

  // Parse minutes value
  int minutes = atoi(body.c_str() + minutesPos + 10);

  if (minutes < 1 || minutes > 1440)
  {
    server.send(400, "text/plain", "Invalid minutes value (must be 1-1440)");
    return;
  }

  alarmState.autoOffEnabled = enabled;
  alarmState.autoOffMinutes = minutes;
  saveAlarmToStorage();

  String response = "{\"autoOffEnabled\":" + String(enabled ? "true" : "false") +
                    ",\"autoOffMinutes\":" + String(minutes) + "}";
  server.send(200, "application/json", response);

  Serial.printf("Auto-off: %s (%d minutes)\n", enabled ? "enabled" : "disabled", minutes);
}

void handleGetAutoOff()
{
  String response = "{\"autoOffEnabled\":" + String(alarmState.autoOffEnabled ? "true" : "false") +
                    ",\"autoOffMinutes\":" + String(alarmState.autoOffMinutes) + "}";
  server.send(200, "application/json", response);
}

// ============ STORAGE FUNCTIONS ============
void saveAlarmToStorage()
{
  preferences.putInt("alarm_hour", alarmState.hour);
  preferences.putInt("alarm_min", alarmState.minute);
  preferences.putBool("alarm_set", alarmState.isAlarmSet);
  preferences.putBool("autooff_enabled", alarmState.autoOffEnabled);
  preferences.putInt("autooff_mins", alarmState.autoOffMinutes);
  Serial.println("Alarm saved to persistent storage");
}

void loadAlarmFromStorage()
{
  alarmState.hour = preferences.getInt("alarm_hour", 6);
  alarmState.minute = preferences.getInt("alarm_min", 30);
  alarmState.isAlarmSet = preferences.getBool("alarm_set", false);
  alarmState.autoOffEnabled = preferences.getBool("autooff_enabled", true);
  alarmState.autoOffMinutes = preferences.getInt("autooff_mins", DEFAULT_AUTO_OFF_MINUTES);
  Serial.printf("Alarm loaded: %d:%02d (Set: %s)\n", alarmState.hour, alarmState.minute, alarmState.isAlarmSet ? "Yes" : "No");
  Serial.printf("Auto-off: %s (%d minutes)\n", alarmState.autoOffEnabled ? "enabled" : "disabled", alarmState.autoOffMinutes);
}

// ============ LED CONTROL FUNCTIONS ============
void setBrightness(int warm, int cool)
{
  // Clamp values to 0-1023 (10-bit resolution)
  warm = constrain(warm, 0, 1023);
  cool = constrain(cool, 0, 1023);

  alarmState.currentWarmBrightness = warm;
  alarmState.currentCoolBrightness = cool;

  // Choose gamma depending on whether we're in sunrise mode (tweak perceptual curve)
  float gamma = alarmState.isSunriseActive ? SUNRISE_GAMMA : DEFAULT_GAMMA;
  int pwmWarm = applyGamma(warm, gamma);
  int pwmCool = applyGamma(cool, gamma);

  ledcWrite(PWM_CHANNEL_WARM, pwmWarm);
  ledcWrite(PWM_CHANNEL_COOL, pwmCool);
}

// ============ SUNRISE LOGIC ============
void startSunrise()
{
  Serial.println("Starting sunrise...");
  alarmState.isSunriseActive = true;
  alarmState.sunriseStartTime = millis();
}

void updateSunrise()
{
  // If sunrise not active, check whether we should start it
  if (!alarmState.isSunriseActive)
  {
    if (!alarmState.isAlarmSet)
    {
      return;
    }

    time_t now = time(nullptr);
    struct tm timeinfo = *localtime(&now);

    if (timeinfo.tm_hour == alarmState.hour && timeinfo.tm_min == alarmState.minute)
    {
      startSunrise();
      Serial.println("Sunrise started");
    }
    return;
  }

  // Sunrise is active - update brightness progressively using easing
  unsigned long elapsedTime = millis() - alarmState.sunriseStartTime;

  if (elapsedTime >= SUNRISE_DURATION_MS)
  {
    // Sunrise complete - set to target brightness (1023 warm, 409 cool in 10-bit)
    setBrightness(1023, 409);
    alarmState.isSunriseActive = false;

    // Schedule auto-off if enabled
    if (alarmState.autoOffEnabled)
    {
      alarmState.sunriseCompleteTime = millis();
      alarmState.autoOffScheduled = true;
      Serial.printf("Auto-off scheduled in %d minutes\n", alarmState.autoOffMinutes);
    }

    Serial.println("Sunrise complete!");
    return;
  }

  // Normalized progress 0.0 .. 1.0
  float progress = (float)elapsedTime / (float)SUNRISE_DURATION_MS;
  if (progress < 0.0f)
    progress = 0.0f;
  if (progress > 1.0f)
    progress = 1.0f;

  // Simple linear fade from (0, 0) to (1023, 409) in 10-bit resolution
  int warmBrightness = (int)(1023.0f * progress);
  int coolBrightness = (int)(409.0f * progress);

  setBrightness(warmBrightness, coolBrightness);

  // Throttled debug printing (every ~5s) to avoid flooding serial
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 5000)
  {
    Serial.printf("Sunrise progress: %.1f%% warm=%d cool=%d\n", progress * 100.0f, warmBrightness, coolBrightness);
    lastPrint = millis();
  }
}

// Update manual fade effect (called from loop)
void updateManualFade()
{
  if (!alarmState.isManualFadeActive)
    return;

  unsigned long elapsed = millis() - alarmState.manualFadeStartTime;
  unsigned long duration = alarmState.manualFadeDuration;

  if (duration == 0 || elapsed >= duration)
  {
    // Finish immediately
    setBrightness(alarmState.manualTargetWarm, alarmState.manualTargetCool);
    alarmState.isManualFadeActive = false;
    Serial.println("Manual fade complete");
    return;
  }

  float progress = (float)elapsed / (float)duration;
  if (progress < 0.0f)
    progress = 0.0f;
  if (progress > 1.0f)
    progress = 1.0f;

  // Use ease-in-out sine easing for a gentler ramp
  float eased = easeInOutSine(progress);

  int warm = alarmState.manualStartWarm + (int)((alarmState.manualTargetWarm - alarmState.manualStartWarm) * eased);
  int cool = alarmState.manualStartCool + (int)((alarmState.manualTargetCool - alarmState.manualStartCool) * eased);

  setBrightness(warm, cool);
}

// Update auto-off timer (called from loop)
void updateAutoOff()
{
  if (!alarmState.autoOffScheduled)
    return;

  unsigned long elapsed = millis() - alarmState.sunriseCompleteTime;
  unsigned long autoOffDuration = (unsigned long)alarmState.autoOffMinutes * 60 * 1000;

  if (elapsed >= autoOffDuration)
  {
    // Time to turn off - start manual fade down to zero
    alarmState.isManualFadeActive = true;
    alarmState.manualFadeStartTime = millis();
    alarmState.manualFadeDuration = MANUAL_FADE_MS;
    alarmState.manualStartWarm = alarmState.currentWarmBrightness;
    alarmState.manualStartCool = alarmState.currentCoolBrightness;
    alarmState.manualTargetWarm = 0;
    alarmState.manualTargetCool = 0;

    alarmState.autoOffScheduled = false;
    Serial.println("Auto-off triggered: fading lights off");
  }
}
