#pragma once

// =========================================================================
// SETTINGS TEMPLATE
// =========================================================================
// Copy this file to "Settings.h" and fill in your real values.
// DO NOT commit Settings.h to Git — it contains your private credentials!
// =========================================================================

// -------------------------------------------------------------------------
// Hardware Configuration
// -------------------------------------------------------------------------
#define DHTPIN 14
#define DHTTYPE DHT22
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
#define I2C_SDA 21
#define I2C_SCL 22

// -------------------------------------------------------------------------
// Telegram Bot Settings
// -------------------------------------------------------------------------
// Get your bot token from @BotFather on Telegram
#define BOT_TOKEN "YOUR_BOT_TOKEN_HERE"
// Get your Chat ID by messaging @userinfobot on Telegram
#define OWNER_CHAT_ID "YOUR_CHAT_ID_HERE"

// -------------------------------------------------------------------------
// WiFi Connectivity
// -------------------------------------------------------------------------
static const char* WIFI_SSID = "YOUR_WIFI_SSID";
static const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
static const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = 3600;      // UTC+1 (adjust for your timezone)
const int DAYLIGHT_OFFSET_SEC = 3600;  // Daylight saving time offset

// -------------------------------------------------------------------------
// Sensor Logic & Thresholds
// -------------------------------------------------------------------------
const unsigned long SENSOR_INTERVAL_MS = 120000; // 2 Minutes Default
const size_t HISTORY_SIZE = 500;                 // Ring buffer size (~18-25 hours)

// Sensor Calibration (adjust based on your hardware)
// These offsets compensate for chip self-heating in an enclosed case
const float TEMP_OFFSET = -2.0f;   // Temperature offset in °C
const float HUM_OFFSET = +10.9f;   // Humidity offset in %

// Anomaly Detection
const float MAX_TEMP_JUMP = 2.0f;  // Max allowed jump between readings
const float EMA_ALPHA = 0.2f;      // EMA smoothing factor (0.0 - 1.0)

// Advice Logic Thresholds
const float HUMIDITY_HIGH_THRESHOLD = 60.0f; // >60% -> Elevated risk
const float HUMIDITY_LOW_THRESHOLD = 40.0f;  // <40% -> Too dry
const float RAPID_DROP_THRESHOLD = 2.0f;     // % drop per interval -> Ventilating

// -------------------------------------------------------------------------
// UI Configuration
// -------------------------------------------------------------------------
const int NIGHT_MODE_START_HOUR = 22;  // 10 PM
const int NIGHT_MODE_END_HOUR = 9;     // 9 AM

// -------------------------------------------------------------------------
// Weather API (open-meteo.com — free, no API key required)
// -------------------------------------------------------------------------
// Update latitude & longitude for your location
// Example: Weiden in der Oberpfalz, Germany
static const char* WEATHER_API_URL = "https://api.open-meteo.com/v1/forecast?latitude=49.67&longitude=12.16&current=temperature_2m,relative_humidity_2m&timezone=Europe%2FBerlin";
