/**
 * @file main.cpp
 * @brief Autonomous Smart Climate Monitor Firmware (Modular & Reactive)
 *        Version: v4.3 (Reactive Vibe)
 */

#include "CoreState.h"       // Data Hub
#include "DisplayManager.h"
#include "SensorManager.h"
#include "Settings.h"
#include "TelegramManager.h"
#include "WeatherManager.h"
#include "WebManager.h"
#include "NetworkManager.h" // [NEW] Network Logic
#include "vibe/ClimateEngine.h"
#include "AdviceEngine.h"
#include <Arduino.h>

// Global State Hub
CoreState g_state;

// Modules
SensorManager sensorManager;
DisplayManager displayManager;
WebManager webManager;
WeatherManager weatherManager;
TelegramManager telegramManager;
NetworkManager networkManager; // [NEW]

// Logic Engine (Brain)
ClimateEngine engine;

void setup() {
  setCpuFrequencyMhz(80);
  Serial.begin(115200);

  // 1. Init Data Hub
  g_state.init();
  
  // Register Main Task for Notifications (Reactivity)
  g_state.mainTaskHandle = xTaskGetCurrentTaskHandle();

  // 2. Init UI
  displayManager.begin();
  
  // 3. Init Network (WiFi + Time + Reconnect)
  // This blocks until WiFi/Time is ready or times out
  networkManager.begin();
  
  // 4. Initial Weather Fetch
  g_state.advice = "GET WEATHER...";
  g_state.ipAddress = "Weiden (DE)";
  displayManager.update();
  
  weatherManager.update();
  int wRetry = 0;
  while (!weatherManager.isDataValid() && wRetry < 20) {
      weatherManager.update(); 
      delay(500);
      wRetry++;
  }

  // 5. Start Services
  g_state.advice = "STARTING...";
  g_state.ipAddress = "Sensors Init";
  displayManager.update();

  sensorManager.begin();
  webManager.begin();
  telegramManager.begin();
  
  // 6. Broadcast Startup
  String reason = networkManager.getResetReason();
  String startupMsg = "🟢 **Система Запущена**\n";
  startupMsg += "Версия: v4.3 (Reactive Vibe)\n";
  startupMsg += "Причина: " + reason + "\n";
  startupMsg += "Heap: " + String(ESP.getFreeHeap() / 1024) + " KB";
  telegramManager.broadcastAlert(startupMsg, 1);

  // Initial Update
  g_state.lock();
  g_state.ipAddress = networkManager.getLocalIP();
  g_state.unlock();
  displayManager.update();
}

void loop() {
  // ---------------------------------------------------------------------------
  // REACTIVE LOOP (Sleep until notified)
  // ---------------------------------------------------------------------------
  
  // Wait for notification from SensorManager (or timeout 1s)
  // pdTRUE = Request to clear the notification value on exit
  // pdMS_TO_TICKS(1000) = Wake up every 1s anyway (for maint tasks)
  uint32_t notificationValue = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
  
  unsigned long now = millis();

  if (notificationValue > 0) {
      // EVENT: New Sensor Data Available!
      CoreSnapshot sn = g_state.getSnapshot();
      
      if (!isnan(sn.temp)) {
          // A. Process Climate Logic
          ClimateEngine::EngineResult result = engine.process(sn, now);
          
          // B. Update Data Hub
          ClimateEngine::updateCoreState(g_state, result);
          
          // C. Reactivity (Update UI immediately)
          displayManager.update();
          
          // D. Telegram Alerts (Target Met, etc)
          telegramManager.update(); 
      }
  }

  // ---------------------------------------------------------------------------
  // MAINTENANCE TASKS (Every 1s timeout)
  // ---------------------------------------------------------------------------
  
  // 1. Network Maintainer (Reconnects if needed)
  networkManager.update();
  
  // 2. Weather Update (Internal timer handles frequency)
  weatherManager.update();
  
  // 3. Telegram Polling (Incoming messages)
  telegramManager.update();
}
