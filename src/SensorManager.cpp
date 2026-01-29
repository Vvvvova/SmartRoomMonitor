#include "SensorManager.h"

SensorManager::SensorManager()
    : driver(DHTPIN, DHTTYPE), lastValidTemp(NAN),
      lastLogTime(0) {
  dataMutex = xSemaphoreCreateMutex();
}

void SensorManager::begin() {
  driver.begin();
  
  // Launch Task on Core 1
  xTaskCreatePinnedToCore(SensorManager::sensorTask, "DHT_Task", 4096, this, 2, NULL, 1);
}

void SensorManager::lock() {
  if (dataMutex) xSemaphoreTake(dataMutex, pdMS_TO_TICKS(500));
}

void SensorManager::unlock() {
  if (dataMutex) xSemaphoreGive(dataMutex);
}

void SensorManager::sensorTask(void *parameter) {
  SensorManager *self = (SensorManager *)parameter;
  
  // Initial delay to let sensor stabilize
  vTaskDelay(pdMS_TO_TICKS(2000));

  for (;;) {
    // 1. Determine Sleep Time from CoreState (Heartbeat)
    uint32_t sleepMs = g_state.recommendedPollInterval;
    if (sleepMs < 2000) sleepMs = 2000; // Hard limit for DHT22 (0.5Hz max)
    
    TickType_t sleepTicks = pdMS_TO_TICKS(sleepMs);
    
    // 2. Read Sensor
    float rawT = self->driver.readTemperature();
    float rawH = self->driver.readHumidity();

    // 3. Process
    if (!isnan(rawT) && !isnan(rawH)) {
      if (xSemaphoreTake(self->dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        self->processReading(rawT, rawH);
        xSemaphoreGive(self->dataMutex);
      }
    } else {
        Serial.println("DHT Read Failed!");
    }

    #if defined(ESP32) && defined(CONFIG_ESP32_WDT)
    esp_task_wdt_reset();
    #endif
    
    // 4. Sleep
    vTaskDelay(sleepTicks);
  }
}

void SensorManager::processReading(float rawT, float rawH) {
  float t = rawT + TEMP_OFFSET;
  float h = ClimateMath::constrainHumidity(rawH + HUM_OFFSET);

  // 1. Math Layer: Filter & Validation
  if (!ClimateMath::isJumpValid(t, lastValidTemp, MAX_TEMP_JUMP)) {
      t = lastValidTemp;
  } else {
      t = ClimateMath::lowPassFilter(t, lastValidTemp, EMA_ALPHA);
  }
  lastValidTemp = t;

  float currentTemp = t;
  float currentHum = h;
  float currentAbsHum = ClimateMath::calculateAbsHumidity(currentTemp, currentHum);
  float currentDP = ClimateMath::calculateDewPoint(currentTemp, currentHum);
  
  // 2. Publish to CoreState
  g_state.lock();
  g_state.temp = currentTemp;
  g_state.hum = currentHum;
  g_state.dewPoint = currentDP;
  g_state.absHum = currentAbsHum;
  
  // Update timestamp to signal new data
  g_state.lastDataTimestamp = millis();
  
  g_state.unlock();
  
  // Signal Coordinator (Reactive Wakeup)
  g_state.notifyCoordinator();
  
  // Track local time for consistency if needed
  lastLogTime = millis();
}

void SensorManager::update() {
   // Legacy method required by main.cpp interface, but mostly empty now.
   // Could be used for watchdog or status checks if needed.
}
