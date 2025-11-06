# Wake-Up Light Control System

An ESP32-based wake-up light controller with WiFi connectivity, REST API, and configurable sunrise simulation.

## Overview

This project implements a smart wake-up light system that gradually increases brightness to simulate sunrise, helping you wake up naturally. The system is controlled via WiFi through a REST API and supports over-the-air (OTA) firmware updates.

### Key Features

- **Sunrise Alarm**: 15-minute gradual brightness fade from 0 to full brightness at your set alarm time
- **Manual Control**: Turn lights on/off with smooth 350ms fade transitions
- **Fine-Grained Brightness Control**: Set warm and cool LED channels independently (0-1023 range)
- **Auto-Off Timer**: Automatically fade lights off after a configurable duration (default: 45 minutes)
- **WiFi Connectivity**: Full REST API for remote control
- **Over-The-Air Updates**: Upload firmware wirelessly after initial USB setup
- **Persistent Storage**: Alarm settings and configuration survive reboots
- **CORS Support**: Compatible with web frontends from any origin
- **Real-Time Updates**: Monitor device status and brightness levels

## Hardware

### Microcontroller
- **ESP32-DOIT-DevKit-v1** (or compatible ESP32 board)
- 240 MHz Xtensa dual-core processor
- 520KB SRAM
- WiFi 802.11 b/g/n

### LED Configuration
- **Warm LED**: GPIO 18 (PWM Channel 0)
- **Cool LED**: GPIO 19 (PWM Channel 1)
- **PWM Frequency**: 5 kHz
- **PWM Resolution**: 10-bit (0-1023 range)

### Power Requirements
- ESP32: 5V via USB
- LEDs: Depends on specific LED strips (typically 12V with integrated driver)

## Getting Started

### Prerequisites

1. **PlatformIO** - Framework for ESP32 development
   ```bash
   pip install platformio
   ```

2. **USB Cable** - For initial programming and serial debugging

3. **WiFi Network** - For device connectivity (update SSID/password in code)

### Initial Setup

1. **Clone/Download the repository**
   ```bash
   cd wake-up-light
   ```

2. **Update WiFi Credentials**
   - Edit `src/main.cpp` lines 10-11
   - Replace with your WiFi SSID and password
   - ⚠️ **Important**: Do NOT commit credentials to public repositories

3. **Connect ESP32 via USB**

4. **Build and Upload**
   ```bash
   pio run -t upload
   ```

5. **Monitor Serial Output**
   ```bash
   pio device monitor
   ```

   Watch for startup messages including WiFi connection and assigned IP address.

## Features Documentation

### Sunrise Alarm

Automatically starts at your configured alarm time and gradually increases brightness.

- **Duration**: 15 minutes (configurable via `SUNRISE_DURATION_MINUTES` in code)
- **Brightness Progression**: Linear fade from (0, 0) to (1023 warm, 409 cool)
- **Response to Sunrise**: Switches to linear gamma (1.0) for smooth, consistent fade
- **Automatic Off**: After reaching max brightness, optionally fades off after configured time

### Manual Control

Instant on/off control with smooth fade transitions.

- **Fade Duration**: 350 milliseconds
- **Manual On**: Fades to full brightness (1023, 1023)
- **Manual Off**: Fades to zero brightness (0, 0)
- **Cancels Active Sunrise**: Manual control interrupts any running alarm

### Brightness Control

Fine-grained control over individual LED channels for precise color temperature adjustment.

- **Range**: 0-1023 per channel (10-bit resolution)
- **Channels**: `warm` (warm white) and `cool` (cool white)
- **Fade Behavior**: Transitions smoothly over 350ms
- **Gamma Correction**: Applied for perceptual brightness linearity

### Auto-Off Timer

Automatically fades lights off after sunrise completes.

- **Default**: 45 minutes
- **Configurable**: 1-1440 minutes (1 minute to 24 hours)
- **Behavior**: Fades smoothly to off using same mechanism as manual control
- **Persistence**: Settings survive reboots

### OTA Updates

Upload new firmware wirelessly without USB connection.

**After initial USB upload**, update via WiFi:

```bash
# Find device IP from serial output or router DHCP list
pio run -t upload --upload-port <ESP32_IP_ADDRESS>

# Or use mDNS hostname
pio run -t upload --upload-port wake-up-light.local
```

Progress and errors are reported to serial output.

## REST API

All responses include CORS headers (`Access-Control-Allow-Origin: *`).

### Alarm Endpoints

#### Set Alarm
```
POST /set-alarm
Content-Type: application/json

Request:
{"hour": 7, "minute": 30}

Response:
"Alarm set to 7:30"
```

#### Get Alarm
```
GET /get-alarm

Response:
{"hour": 7, "minute": 30, "isSet": true}
```

#### Toggle Alarm
```
POST /toggle-alarm
Content-Type: application/json

Request:
{"enabled": true}  // or false to disable

Response:
{"isAlarmSet": true, "alarmTime": "7:30"}
```

### Control Endpoints

#### Manual On
```
POST /manual-on

Response:
"Lights fading on"
```

#### Manual Off
```
POST /manual-off

Response:
"Lights fading off"
```

#### Set Brightness
```
POST /set-brightness
Content-Type: application/json

Request:
{"warm": 800, "cool": 400}

Response:
{"warm": 800, "cool": 400, "fading": true}
```

### Configuration Endpoints

#### Set Auto-Off
```
POST /set-auto-off
Content-Type: application/json

Request:
{"enabled": true, "minutes": 45}

Response:
{"autoOffEnabled": true, "autoOffMinutes": 45}
```

#### Get Auto-Off
```
GET /get-auto-off

Response:
{"autoOffEnabled": true, "autoOffMinutes": 45}
```

### Status Endpoints

#### Get Status
```
GET /status

Response:
{
  "currentTime": "07:25:30",
  "alarmTime": "7:30",
  "isAlarmSet": true,
  "isSunriseActive": false,
  "warmBrightness": 0,
  "coolBrightness": 0
}
```

## API Examples

### Using cURL

**Set alarm for 7:30 AM**
```bash
curl -X POST http://<ESP32_IP>/set-alarm \
  -H "Content-Type: application/json" \
  -d '{"hour": 7, "minute": 30}'
```

**Turn lights on**
```bash
curl -X POST http://<ESP32_IP>/manual-on
```

**Set specific brightness**
```bash
curl -X POST http://<ESP32_IP>/set-brightness \
  -H "Content-Type: application/json" \
  -d '{"warm": 1023, "cool": 200}'
```

**Configure 60-minute auto-off**
```bash
curl -X POST http://<ESP32_IP>/set-auto-off \
  -H "Content-Type: application/json" \
  -d '{"enabled": true, "minutes": 60}'
```

**Check device status**
```bash
curl http://<ESP32_IP>/status
```

### Using JavaScript/Fetch

```javascript
const ESP_IP = '192.168.1.100';

// Set alarm
fetch(`http://${ESP_IP}/set-alarm`, {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ hour: 7, minute: 30 })
});

// Turn lights on
fetch(`http://${ESP_IP}/manual-on`, { method: 'POST' });

// Get status
fetch(`http://${ESP_IP}/status`)
  .then(r => r.json())
  .then(data => console.log(data));
```

## Configuration

### WiFi Settings
```cpp
// src/main.cpp, lines 10-11
const char *WIFI_SSID = "Your-SSID";
const char *WIFI_PASSWORD = "Your-Password";
```

### Timezone
```cpp
// src/main.cpp, line 16
// Default: London (GMT/BST)
const char *TZ_INFO = "GMT0BST,M3.5.0/1,M10.5.0";

// Other examples:
// UTC: "UTC0"
// US Eastern: "EST5EDT,M3.2.0,M11.1.0"
// US Pacific: "PST8PDT,M3.2.0,M11.1.0"
// Central Europe: "CET-1CEST,M3.5.0,M10.5.0"
```

### Sunrise Duration
```cpp
// src/main.cpp, line 34
const int SUNRISE_DURATION_MINUTES = 15;
```

### Manual Fade Speed
```cpp
// src/main.cpp, line 37
const unsigned long MANUAL_FADE_MS = 350;  // milliseconds
```

### Auto-Off Default
```cpp
// src/main.cpp, line 39
const int DEFAULT_AUTO_OFF_MINUTES = 45;
```

### PWM Settings
```cpp
// src/main.cpp, lines 27-28
const int PWM_FREQ = 5000;      // 5 kHz
const int PWM_RESOLUTION = 10;  // 10-bit (0-1023)
```

## Finding Device IP

### Method 1: Serial Monitor
After startup, the IP address is printed:
```
IP address: 192.168.1.100
```

### Method 2: Router DHCP List
Check your router's connected devices for hostname: `wake-up-light`

### Method 3: mDNS
Use the hostname directly:
```bash
ping wake-up-light.local
```

## Troubleshooting

### WiFi Connection Issues

**Problem**: Device won't connect to WiFi
- Check WiFi SSID and password are correct
- Ensure 2.4 GHz band is enabled (ESP32 doesn't support 5 GHz)
- Check serial output for connection attempts

**Problem**: Frequent reconnections
- Check WiFi signal strength
- Try moving closer to router
- Check for interference from other devices

### Brightness Issues

**Problem**: Lights don't turn on
- Check LED power supply (12V for most strips)
- Verify GPIO pins 18/19 are connected correctly
- Check PWM output with multimeter (should see 0-5V PWM signal)

**Problem**: Jittery brightness
- The 10-bit PWM resolution should provide smooth fades
- If still jittery, try adjusting `PWM_FREQ` or `PWM_RESOLUTION`
- Check power supply is stable and adequately filtered

**Problem**: Colors don't look right
- Adjust warm/cool ratio in API calls
- Check LED strip color temperature ratings
- Verify gamma correction isn't over-correcting (currently set to 1.0 for linear response)

### API Issues

**Problem**: CORS errors in web frontend
- All endpoints support CORS with `Access-Control-Allow-Origin: *`
- Ensure browser is making valid HTTP requests (not HTTPS to HTTP)
- Check for typos in endpoint URLs

**Problem**: API requests timeout
- Ensure ESP32 is connected to WiFi
- Check device IP is correct
- Try pinging the device first: `ping <IP>`

### OTA Update Issues

**Problem**: OTA upload fails
- Device must be connected to WiFi
- Use correct IP address: `pio run -t upload --upload-port <IP>`
- Check both serial output during upload for detailed error messages
- Device may need to be power-cycled to recover from failed OTA

## Code Architecture

### Main Components

**`setup()`** (lines 122-151)
- Initializes all subsystems in order
- Loads persistent settings from flash

**`loop()`** (lines 154-164)
- Handles OTA requests
- Processes HTTP requests
- Updates fade animations
- Checks sunrise/auto-off timers

**State Management** (lines 43-67)
- Single `alarmState` struct holds all device state
- Includes alarm settings, brightness levels, fade animations
- Persisted to flash storage via `Preferences`

### Key Functions

- `setupWiFi()` - Connect to WiFi with retries
- `setupNTP()` - Synchronize system time
- `setupWebServer()` - Register HTTP endpoints
- `setupOTA()` - Configure over-the-air updates
- `updateSunrise()` - Handle alarm fade-up logic
- `updateManualFade()` - Handle manual brightness transitions
- `updateAutoOff()` - Check auto-off timer

### HTTP Handlers

Each endpoint has a handler function:
- `handleSetAlarm()`, `handleGetAlarm()`, `handleToggleAlarm()`
- `handleManualOn()`, `handleManualOff()`, `handleSetBrightness()`
- `handleSetAutoOff()`, `handleGetAutoOff()`
- `handleStatus()`, `handleNotFound()`

### Brightness Control

- `setBrightness(warm, cool)` - Sets LED brightness with gamma correction
- `applyGamma(value, gamma)` - Applies gamma curve for perceptual linearity
- Linear fade uses `easeInOutSine()` for smooth transitions

### Storage

- Uses ESP32 `Preferences` library (NVS flash storage)
- Persists: alarm time, enabled status, auto-off settings
- Automatically loaded on startup

## Performance Notes

- **Loop Rate**: 20ms delay for 50 Hz update rate
- **OTA Handler**: Checks on every loop iteration for update requests
- **Memory Usage**: Minimal - struct-based state, no dynamic allocations
- **PWM Frequency**: 5 kHz chosen for imperceptible flicker and low audible noise

## Building & Dependencies

### PlatformIO Configuration
See `platformio.ini`:
- Platform: espressif32
- Board: esp32doit-devkit-v1
- Framework: Arduino
- Libraries: ESPAsyncWebServer, AsyncTCP (available but not currently used)

### Core Libraries
- `<Arduino.h>` - Arduino framework
- `<WiFi.h>` - WiFi connectivity
- `<WebServer.h>` - HTTP server
- `<ArduinoOTA.h>` - OTA updates
- `<Preferences.h>` - Flash storage
- `<time.h>` - Time functions
- `<cmath>` - Math functions

## Future Enhancements

### Possible Features
- Web dashboard for real-time control
- Scene/preset support (different brightness curves)
- Temperature sensor for auto-adjustment
- Mobile app integration
- Snooze functionality
- Multiple alarm support
- Sunset simulation for sleep preparation
- Gesture/button control integration
- Integration with home automation systems (Home Assistant, etc.)

### Known Limitations
- Single WiFi network only (no multi-AP support)
- No built-in security (consider adding authentication for production use)
- JSON parsing is basic (no external JSON library)
- No sleep/low-power modes
- Manual fade duration is fixed (not adjustable via API)

## License

[Add license information]

## Contributing

[Add contribution guidelines]

## Support

For issues or questions:
1. Check the Troubleshooting section above
2. Monitor serial output with `pio device monitor`
3. Verify WiFi connectivity and device IP
4. Check API endpoint documentation for correct request format

---

**Last Updated**: November 2024
**Firmware Version**: Latest (check `git log` for version history)
