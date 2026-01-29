#pragma once

#include "Settings.h"

#ifdef UNIT_TEST
#include "MockArduino.h"
#else
#include <Arduino.h>
#include <DHT.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#endif

#include <vector>

class WeatherManager; // Forward Declaration

// Optimized Record (12 bytes)
struct Record {
  uint32_t ts;
  float t;
  float h;
};

class SensorManager {
public:
  SensorManager();
  void begin();
  void update(); // Main Loop processing (State Machine etc)

  // Task wrapper
  static void sensorTask(void *parameter);

  unsigned long getStateEnterTime() const;
  void setWeatherManager(WeatherManager *wm);

  // Getters (Thread Safe-ish)
  float getTemp() const;
  float getHum() const;
  float getDewPoint() const;

  // Array Access for Web Stream
  // Returns number of available points
  size_t getHistoryCount() const;
  // Thread-Safe Snapshot: Copies active history to target vector
  void getHistoryCopy(std::vector<Record> &target);

  // Thread-Safe Chunk Access: Copies up to 'count' items starting at 'offset'
  // Returns number of items actually copied
  size_t copyHistory(size_t offset, size_t count, Record *destination);

  // Returns reading at index (0 = Oldest, count-1 = Newest) for Graphing
  // convenience
  Record getHistoryPoint(size_t index) const;

  // Analysis
  String getRecommendation(); // Improved with Caching
  int getAdviceCode();        // Improved with Caching
  float getAvg24h() const;

  // Debug / Weather Data
  float getOutdoorTemp() const;
  float getOutdoorHum() const;
  float getOutdoorAbsHum() const;
  float getIndoorAbsHum() const;
  bool isWeatherValid() const;
  String getWeatherStatus() const;
  // Physics & Smart State Machine (v5.0)
  // New Enum States (Must be defined before usage)
  enum class ClimateState { STABLE, VENTILATING, TARGET_MET, INEFFICIENT };

  // =========================================================================
  // STATE MACHINE v6.0 - Pure Function Architecture
  // =========================================================================

  // All inputs needed for state transition decision (immutable)
  struct StateInput {
    // Current sensor readings
    float temp, hum, absHum;
    
    // Baseline values (for trigger detection)
    float baseTemp, baseHum, baseAbsHum;
    float prevAbsHum;  // Previous reading for delta trigger
    
    // Current state context
    ClimateState currentState;
    unsigned long stateEnterTime;
    unsigned long now;
    
    // Counters
    unsigned int triggerConfirmCount;
    unsigned int plateauConfirmCount;
    unsigned int baselineUpdateCount;
    
    // Rebound tracking
    float reboundStartTemp;
    unsigned long reboundStartTime;
    
    // Plateau detection (slope window summary)
    float slopeNewest, slopeOldest;
    size_t slopeCount;
    
    // Initial values for adaptive thresholds
    float stateEnterHum, stateEnterAbsHum;
  };

  // All mutations from state transition decision
  struct StateOutput {
    ClimateState newState;
    unsigned long newStateEnterTime;
    
    // Baseline updates
    bool updateBaseline;
    float newBaseTemp, newBaseHum, newBaseAbsHum;
    
    // Counter updates
    unsigned int newTriggerCount;
    unsigned int newPlateauCount;
    unsigned int newBaselineCounter;
    
    // Rebound tracking updates
    float newReboundStartTemp;
    unsigned long newReboundStartTime;
    
    // State entry values (set on state change)
    float newStateEnterHum, newStateEnterAbsHum;
    
    // Debug
    const char* transitionReason;  // nullptr if no state change
  };

  // Pure decision function (no side effects)
  static StateOutput computeTransition(const StateInput& in);

  ClimateState getClimateState() const;

  // Legacy/Helper Support
  bool isRapidChange() const;
  String getStateString() const;
  int getStateCode() const;

  // Drying Speedometer v3.3
  float getDryingRate() const;       // Returns g/m³/min during ventilation
  String getDryingIndicator() const; // Returns ▲▲, ▲, ▼, or - based on rate

  // Thread Safety
  void lock();
  void unlock();

private:
  DHT dht;
  WeatherManager *weather;

  // Optimization 5: Ring Buffer
  Record history[HISTORY_SIZE];
  size_t historyHead;  // Points to the NEXT write position
  size_t historyCount; // Total items stored

  // Thread Safety
  SemaphoreHandle_t dataMutex;

  float currentTemp;
  float currentHum;
  float currentDP;
  float currentAbsHum; // New Physics: Absolute Humidity
  float avg24h;

  // Caching (Optimization 2)
  String cachedAdvice;
  int cachedCode;
  unsigned long lastAdviceUpdate;

  // Smoothing State
  float lastValidTemp;

  // Window Detection State & Physics Tracking
  float lastTempForWindowCheck;
  float lastHumForWindowCheck;    // RH% baseline for trigger detection
  float lastAbsHumForWindowCheck; // Rebound Detection
  float stateEnterAbsHum;         // Efficiency Tracking
  float lastAbsHum;               // Filter/Smooth
  float stateEnterHum;            // Starting RH% for adaptive target

  bool windowOpen;

  // Plateau Detection v2.0
  static const size_t SLOPE_WINDOW_SIZE = 6; // 6 points * 30s = 3 min window
  float slopeWindow[SLOPE_WINDOW_SIZE];      // Ring buffer of AbsHum values
  size_t slopeWindowHead;
  size_t slopeWindowCount;
  unsigned int plateauConfirmCounter; // Counts consecutive plateau readings
  unsigned int baselineUpdateCounter; // Moved from static variable

  // Improved Rebound Detection
  unsigned long reboundStartTime; // When temp started rising
  float reboundStartTemp;         // Temp when rise started
  bool reboundDetected;           // Flag for rising temp trend

  // Smart Triggers v3.3
  unsigned int
      triggerConfirmCount; // Counts consecutive trigger readings (hysteresis)
  float prevAbsHumForTrigger; // Previous AbsHum for summer trigger delta

  ClimateState state;
  unsigned long stateEnterTime;

  unsigned long lastLogTime;

  // Helper Functions
  // float calculateDropRate() const; // DEPRECATED: Physics-based logic used
  // instead
  void addHistoryPoint(float t, float h);
  void updateAdvice(); // Updates the cached string

#ifdef UNIT_TEST
public:
#endif
  void processReading(float t, float h);
};
