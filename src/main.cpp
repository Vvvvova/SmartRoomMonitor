/**
 * @file main.cpp
 * @brief Autonomous Smart Climate Monitor Firmware (Modular)
 */

#include "DisplayManager.h"
#include "SensorManager.h"
#include "Settings.h"
#include "TelegramManager.h" // [NEW]
#include "WeatherManager.h"  // NEW
#include "WebManager.h"
#include <Arduino.h>
#include <WiFi.h>


// Modules
SensorManager sensorManager;
DisplayManager displayManager;
WebManager webManager(&sensorManager);
WeatherManager weatherManager;                   // NEW
TelegramManager telegramManager(&sensorManager); // [NEW]

// Timer
unsigned long lastSensorRead = 0;

// Helper: Get Reset Reason
String getResetReason() {
  esp_reset_reason_t reason = esp_reset_reason();
  switch (reason) {
  case ESP_RST_POWERON:
    return "Power On";
  case ESP_RST_SW:
    return "Software Reset";
  case ESP_RST_PANIC:
    return "Crash/Panic (Watchdog?)";
  case ESP_RST_INT_WDT:
    return "Interrupt Watchdog";
  case ESP_RST_TASK_WDT:
    return "Task Watchdog";
  case ESP_RST_WDT:
    return "Other Watchdog";
  case ESP_RST_DEEPSLEEP:
    return "Deep Sleep Wake";
  case ESP_RST_BROWNOUT:
    return "Brownout (Low Voltage)";
  case ESP_RST_SDIO:
    return "SDIO Reset";
  default:
    return "Unknown (" + String(reason) + ")";
  }
}

void setup() {
  setCpuFrequencyMhz(80);
  Serial.begin(115200);

  // Init Modules
  displayManager.begin();
  displayManager.update(NAN, NAN, NAN, false, "CONNECTING...", 0, 0,
                        "Init WiFi");

  // Connect WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    displayManager.update(NAN, NAN, NAN, false, "SYNC TIME...", 0, 0,
                          WiFi.localIP().toString());

    // Time sync
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    struct tm timeinfo;
    int timeRetry = 0;
    // Wait for time (blocking up to 20s)
    while (!getLocalTime(&timeinfo) && timeRetry < 20) {
      delay(500);
      timeRetry++;
    }

    displayManager.update(NAN, NAN, NAN, false, "GET WEATHER...", 0, 0,
                          "Weiden (DE)");

    // Weather fetch (blocking up to 10s)
    weatherManager.update();
    int wRetry = 0;
    while (!weatherManager.isDataValid() && wRetry < 20) {
      weatherManager.update(); // Keep trying
      delay(500);
      wRetry++;
    }

  } else {
    displayManager.update(NAN, NAN, NAN, false, "WIFI FAIL", 0, 0,
                          "Offline Mode");
  }

  // NOW start sensors (timestamps will be correct)
  displayManager.update(NAN, NAN, NAN, false, "STARTING...", 0, 0,
                        "Sensors Init");

  // 4. Managers Init — ALWAYS set weather manager (even if offline for status
  // reporting)
  sensorManager.setWeatherManager(&weatherManager);
  weatherManager.update(); // Will fail gracefully if no WiFi
  sensorManager.begin();
  webManager.begin();

  // Telegram Init
  telegramManager.begin();
  String reason = getResetReason();
  String startupMsg = "🟢 **Система Запущена**\n";
  startupMsg += "Версия: v3.3 (Smart Triggers)\n";
  startupMsg += "Причина: " + reason + "\n";
  startupMsg += "Heap: " + String(ESP.getFreeHeap() / 1024) + " KB";
  telegramManager.broadcastAlert(startupMsg, 1);

  lastSensorRead = millis(); // Reset timer

  // First read
  sensorManager.update();

  displayManager.update(sensorManager.getTemp(), sensorManager.getHum(),
                        sensorManager.getDewPoint(), false,
                        sensorManager.getRecommendation(),
                        sensorManager.getAdviceCode(), // [NEW] Code
                        sensorManager.getStateCode(),  // [NEW] State
                        WiFi.localIP().toString());
}

void loop() {
  unsigned long now = millis();

  // 0. Periodic Connectivity Check (every 30s)
  static unsigned long lastConnCheck = 0;
  if (now - lastConnCheck > 30000) {
    lastConnCheck = now;
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WIFI] Reconnecting...");
      WiFi.reconnect();
    }
    // NTP re-sync check (if time looks wrong)
    if (time(NULL) < 1600000000) {
      Serial.println("[NTP] Time invalid, re-syncing...");
      configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    }
  }

  // 1. Core Updates (Polling)
  telegramManager.update(); // Handles incoming messages (non-blocking)

  // 2. Periodic Sensor & Logic Update (Adaptive)
  unsigned long dynamicInterval = SENSOR_INTERVAL_MS; // Default 2 mins
  if (sensorManager.isRapidChange()) {
    dynamicInterval = 10000; // 10 seconds (Rapid Mode)
  }

  if (now - lastSensorRead >= dynamicInterval) {
    lastSensorRead = now;

    weatherManager.update(); // Checks if 10m passed
    sensorManager.update();  // Actual sensor read

    // Update OLED immediately after new data
    displayManager.update(
        sensorManager.getTemp(), sensorManager.getHum(),
        sensorManager.getDewPoint(), false, sensorManager.getRecommendation(),
        sensorManager.getAdviceCode(), sensorManager.getStateCode(),
        WiFi.localIP().toString());
  }

  // 3. Yield to system tasks (CRITICAL for WiFi stability)
  delay(1);
}