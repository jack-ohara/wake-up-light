# Wake-Up LED Strip Setup Guide

## Hardware Setup
- **Warm LED Channel**: GPIO 18
- **Cool LED Channel**: GPIO 19
- **Power Supply**: Appropriate PSU for your LED strip

## Software Configuration

### 1. WiFi Credentials
Edit `src/main.cpp` and update these lines (around line 10):
```cpp
const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASSWORD = "YOUR_PASSWORD";
```

### 2. Timezone Configuration
The code uses POSIX timezone strings that **automatically handle daylight saving time**. For London, it's already set correctly:
```cpp
const char* TZ_INFO = "GMT0BST,M3.5.0/1,M10.5.0";
```

This automatically switches between GMT (winter) and BST (summer) without manual updates.

**Common POSIX timezone strings:**
- **UTC**: `"UTC0"`
- **London (GMT/BST)**: `"GMT0BST,M3.5.0/1,M10.5.0"` ← Already configured
- **Central Europe (CET/CEST)**: `"CET-1CEST,M3.5.0,M10.5.0"`
- **US Eastern (EST/EDT)**: `"EST5EDT,M3.2.0,M11.1.0"`
- **US Pacific (PST/PDT)**: `"PST8PDT,M3.2.0,M11.1.0"`
- **India (IST, no DST)**: `"IST-5:30"`

If you need a different timezone, change the `TZ_INFO` variable in `src/main.cpp` (around line 14).

### 3. Sunrise Duration
By default, the lights fade up over 15 minutes. To change this, modify (around line 25):
```cpp
const int SUNRISE_DURATION_MINUTES = 15;  // Change to desired duration
```

## Uploading to ESP32

### Using PlatformIO CLI:
```bash
pio run -t upload
```

### Using PlatformIO IDE (VSCode):
1. Open the project folder in VSCode
2. Install PlatformIO extension
3. Click "Upload" button in the bottom toolbar

## Finding Your ESP32

Once uploaded and connected to WiFi:

1. **Find the ESP32's IP Address**:
   - Open Serial Monitor: `pio device monitor`
   - Look for "IP address: X.X.X.X"

## REST API Endpoints

All endpoints are available at `http://<ESP32_IP>:<PORT>` and can be called from any HTTP client (curl, Postman, Python, etc.).

### Set Alarm Time
```bash
curl -X POST http://<ESP32_IP>/set-alarm \
  -H "Content-Type: application/json" \
  -d '{"hour": 6, "minute": 30}'
```

### Get Alarm Time
```bash
curl http://<ESP32_IP>/get-alarm
```

### Manual Controls
```bash
# Turn on full brightness
curl -X POST http://<ESP32_IP>/manual-on

# Turn off
curl -X POST http://<ESP32_IP>/manual-off
```

### Get Status
```bash
curl http://<ESP32_IP>/status
```

Example response:
```json
{
  "currentTime": "06:30:15",
  "alarmTime": "6:30",
  "isAlarmSet": true,
  "isSunriseActive": false,
  "warmBrightness": 255,
  "coolBrightness": 0
}
```

## Sunrise Behavior

When the alarm time is reached:
1. Sunrise begins
2. Cool light (dawn-like) fades in over the duration
3. Warm light gradually transitions in
4. After the duration completes, only warm light remains at full brightness

The alarm is checked every minute, so it will trigger within 1 minute of the set time.

## Troubleshooting

### ESP32 Not Connecting to WiFi
- Verify SSID and password are correct
- Check if WiFi is 2.4GHz (ESP32 doesn't support 5GHz)
- Look at serial output for connection messages

### No Time Sync
- Ensure ESP32 has internet connection (WiFi must be working)
- Check if NTP_SERVER is accessible
- Verify timezone offset is correct

### LEDs Not Working
- Check GPIO pins 18 and 19 are not used for WiFi
- Verify power supply is sufficient for LED strip brightness
- Test with manual-on endpoint first

## Dependencies

The project uses:
- Arduino Core for ESP32
- WebServer.h (built-in)
- Preferences.h (built-in storage library)
- Time functions (built-in)

All dependencies are automatically handled by PlatformIO.
