#pragma once

#include <Arduino.h>
#include <DHT.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "CoreState.h"
#include "ClimateMath.h"
#include "Settings.h"
#include <freertos/semphr.h>

/**
 * @class SensorManager
 * @brief Hardware Service for environmental sensors.
 * 
 * Handles periodic polling of DHT22 via FreeRTOS task, 
 * performs low-pass filtering, and pushes results to CoreState.
 * Delegates state machine logic to ClimateEngine.
 */
class SensorManager {
public:
  SensorManager();
  void begin();
  
  // Updates sensor readings and writes to CoreState (Hardware Wrapper)
  void update(); 

  // Locking (Internal use primarily, but exposed if needed)
  void lock();
  void unlock();

  // Accessor for Coordinator
  unsigned long getLastReadTime() const { return lastLogTime; } // Reusing lastLogTime as lastReadTime

private:
  DHT driver; // Changed from SensorDriver
  SemaphoreHandle_t dataMutex;

  // Current Raw/Filtered Values - REMOVED (Direct write to CoreState)
  float lastValidTemp;
  
  unsigned long lastLogTime;

  // Task for FreeRTOS
  static void sensorTask(void *parameter);
  
  // Internal helpers
  void processReading(float rawT, float rawH);
};
