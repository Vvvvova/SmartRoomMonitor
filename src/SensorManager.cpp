#ifdef UNIT_TEST
#include "MockArduino.h"
#include "SensorManager.h"
#else
#include "SensorManager.h"
#include "ClimateMath.h"
#include "WeatherManager.h"
#endif

#ifdef UNIT_TEST
MockSerial Serial;
float DHT::_temp = NAN;
float DHT::_hum = NAN;
unsigned long _mock_millis = 0;
#endif

SensorManager::SensorManager()
    : dht(DHTPIN, DHTTYPE), currentTemp(NAN), currentHum(NAN), currentDP(NAN),
      currentAbsHum(NAN), avg24h(NAN), lastValidTemp(NAN),
      lastTempForWindowCheck(NAN), windowOpen(false),
      state(ClimateState::STABLE), stateEnterTime(0), weather(nullptr),
      historyHead(0), historyCount(0), lastLogTime(0),
      cachedAdvice("Загрузка..."), cachedCode(0), lastAdviceUpdate(0),
      // FIX: Initialize all physics tracking variables to NAN
      lastAbsHumForWindowCheck(NAN), stateEnterAbsHum(NAN), lastAbsHum(NAN),
      stateEnterHum(NAN), lastHumForWindowCheck(NAN),
      // Plateau v2.0 initialization
      slopeWindowHead(0), slopeWindowCount(0), plateauConfirmCounter(0),
      baselineUpdateCounter(0),
      // Improved Rebound Detection
      reboundStartTime(0), reboundStartTemp(NAN), reboundDetected(false),
      // Smart Triggers v3.3
      triggerConfirmCount(0), prevAbsHumForTrigger(NAN) {
  dataMutex = xSemaphoreCreateMutex();
  // Initialize slope window to NAN
  for (size_t i = 0; i < SLOPE_WINDOW_SIZE; i++) {
    slopeWindow[i] = NAN;
  }
}

void SensorManager::lock() {
  // Use timeout to prevent infinite deadlock (WebServer locking during slow
  // network)
  if (dataMutex)
    xSemaphoreTake(dataMutex, pdMS_TO_TICKS(500));
}

void SensorManager::unlock() {
  if (dataMutex)
    xSemaphoreGive(dataMutex);
}

// -------------------------------------------------------------------------
// OPTIMIZATION 4: Async Sensor Task
// -------------------------------------------------------------------------
void SensorManager::sensorTask(void *parameter) {
  SensorManager *self = (SensorManager *)parameter;
  const TickType_t intervalTicks = pdMS_TO_TICKS(6000);
  TickType_t lastWakeTime = xTaskGetTickCount();

  for (;;) {
    // Read DHT (blocking, but isolated in this task)
    float t = self->dht.readTemperature();
    float h = self->dht.readHumidity();

    // Only process when both values are valid
    if (!isnan(t) && !isnan(h)) {
      // Acquire mutex just for the processing step – keep critical section
      // short
      if (xSemaphoreTake(self->dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        self->processReading(t, h);
        xSemaphoreGive(self->dataMutex);
      }
    }

// Feed watchdog if available (prevents resets during long loops)
#if defined(ESP32) && defined(CONFIG_ESP32_WDT)
    esp_task_wdt_reset();
#endif

    // Delay until next 6‑second slot (more accurate than vTaskDelay)
    vTaskDelayUntil(&lastWakeTime, intervalTicks);
  }
}

void SensorManager::begin() {
  dht.begin();
  // Launch the background task on Core 1 (App Core) to avoid stalling Core 0
  // (WiFi/Radio) NOTE: Standard DHT library blocks interrupts, which causes
  // crashes on Core 0. Moving to Core 1 makes the UI slightly stutter every 6s,
  // but prevents random reboots. CRITICAL: 8KB stack to prevent overflow during
  // complex state machine operations Allocate a larger stack (10KB) to avoid
  // overflow during complex state handling. Use core 1 to keep Wi‑Fi/Radio
  // responsive.
  xTaskCreatePinnedToCore(SensorManager::sensorTask, // Function
                          "DHT_Task",                // Name
                          10 * 1024,                 // Stack size (10KB)
                          this,                      // Param
                          2,                         // Priority (Middle-High)
                          NULL,                      // Handle
                          1                          // Core 1
  );
}

void SensorManager::update() {
  unsigned long now = millis();

  // 1. LOGGING LOGIC (Adaptive)
  unsigned long logInterval = 180000; // 3 Minutes
  if (state == ClimateState::VENTILATING) {
    logInterval = 30000; // 30 Seconds
  }

  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10))) {
    // Check Trigger
    long el = now - lastLogTime;
    if (el >= logInterval) {
      if (!isnan(currentTemp)) {
        addHistoryPoint(currentTemp, currentHum);
        lastLogTime = now;

        // Update slopeWindow for Plateau v2.0 (synchronized with history
        // logging) Only during ventilation - 6 points × 30 sec = 3 min window
        if (state == ClimateState::VENTILATING ||
            state == ClimateState::TARGET_MET ||
            state == ClimateState::INEFFICIENT) {
          slopeWindow[slopeWindowHead] = currentAbsHum;
          slopeWindowHead = (slopeWindowHead + 1) % SLOPE_WINDOW_SIZE;
          if (slopeWindowCount < SLOPE_WINDOW_SIZE)
            slopeWindowCount++;
        }
      }
    }

    // 2. Advice Caching Logic (Update every 2s or if forced)
    if (now - lastAdviceUpdate > 2000) {
      updateAdvice();
      lastAdviceUpdate = now;
    }

    xSemaphoreGive(dataMutex);
  }
}

// -------------------------------------------------------------------------
// OPTIMIZATION 2: Caching
// -------------------------------------------------------------------------
void SensorManager::updateAdvice() {
  // Re-run the heavy string logic
  String s;
  int code = 0;

  if (isnan(currentHum)) {
    s = "Анализ...";
    code = 0;
  } else if (state == ClimateState::INEFFICIENT) {
    // FIX v3.4: Add timer to show how long window was open
    unsigned long durMs = millis() - stateEnterTime;
    int mins = (durMs + 59999) / 60000; // Round UP for better UX
    char buf[48];
    snprintf(buf, sizeof(buf), "Эффект упал (%d мин). Закрывай", mins);
    s = buf;
    code = 2; // Red
  } else if (state == ClimateState::TARGET_MET) {
    // FIX v3.4: Show total time it took to reach target
    unsigned long durMs = millis() - stateEnterTime;
    int mins = (durMs + 59999) / 60000; // Round UP for better UX
    char buf[48];
    snprintf(buf, sizeof(buf), "Готово (за %d мин) Закрывай", mins);
    s = buf;
    code = 3; // Green
  } else if (state == ClimateState::VENTILATING) {
    // FIX v3.4: Add timer and speedometer to drying advice
    unsigned long durMs = millis() - stateEnterTime;
    int mins = (durMs + 59999) / 60000; // Round UP for better UX
    String ind = getDryingIndicator();
    char buf[48];
    snprintf(buf, sizeof(buf), "Сушка (%d мин) %s", mins, ind.c_str());
    s = buf;
    code = 1; // Yellow
  } else {
    // STABLE
    float outTemp =
        (weather && weather->isDataValid()) ? weather->getOutdoorTemp() : 20.0;

    // Winter
    if (outTemp < 10.0) {
      float margin = currentTemp - currentDP;
      if (margin < 3.0) {
        s = "КРИТИЧНО! ГРЕТЬ/ОСУШАТЬ";
        code = 2;
      } else if (currentHum > 55.0) {
        s = "Влажно [ЗАЛП 5 мин]";
        code = 1;
      } else {
        s = "Зимняя Норма";
        code = 3;
      }
    }
    // Summer
    else if (outTemp > 18.0) {
      if (weather && weather->isDataValid()) {
        float inAbs =
            ClimateMath::calculateAbsHumidity(currentTemp, currentHum);
        float outAbs = weather->getOutdoorAbsHum();
        if (outAbs > inAbs) {
          s = "Влажно [НЕ ОТКРЫВАТЬ!]";
          code = 3;
        } // Blue/Green
        else if (currentHum > 60.0) {
          s = "Влажно [Проветрить]";
          code = 1;
        } // Yellow
        else {
          s = "Летняя Норма";
          code = 3;
        }
      } else {
        if (currentHum > 60.0) {
          s = "Влажно [Проветрить]";
          code = 1;
        } else {
          s = "Летняя Норма";
          code = 3;
        }
      }
    }
    // Transition
    else {
      if ((currentTemp - currentDP) < 2.5) {
        s = "КРИТИЧНО! Открыть окно";
        code = 2;
      } else if (currentHum > 60.0) {
        s = "Влажно [Реком. проветрить]";
        code = 1;
      } else if (currentHum < 35.0) {
        s = "Сухой воздух [Увлажнить]";
        code = 3;
      } else {
        s = "Норма (Стены сохнут)";
        code = 3;
      }
    }
  }

  cachedAdvice = s;
  cachedCode = code;
}

String SensorManager::getRecommendation() {
  // Return cached value (Thread safe read technically requires mutex but String
  // copy is atomic enough for display) For strict correctness we take mutex for
  // string copy
  String copy;
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10))) {
    copy = cachedAdvice;
    xSemaphoreGive(dataMutex);
  }
  return copy;
}

int SensorManager::getAdviceCode() {
  // Integer read is atomic on ESP32
  return cachedCode;
}

// -------------------------------------------------------------------------
// OPTIMIZATION 5: Ring Buffer Implementation
// -------------------------------------------------------------------------
void SensorManager::addHistoryPoint(float t, float h) {
  time_t now = time(NULL);
  if (now < 1600000000)
    return;

  // EMA for Daily Avg
  if (isnan(avg24h))
    avg24h = h;
  else
    avg24h = (avg24h * 0.99f) + (h * 0.01f);

  Record r = {(uint32_t)now, t, h};

  // Ring Buffer Write
  history[historyHead] = r;
  historyHead = (historyHead + 1) % HISTORY_SIZE;
  if (historyCount < HISTORY_SIZE)
    historyCount++;
}

size_t SensorManager::getHistoryCount() const { return historyCount; }

void SensorManager::getHistoryCopy(std::vector<Record> &target) {
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(200))) {
    target.clear();
    target.reserve(historyCount);

    // Reconstruct order: Oldest -> Newest
    // If not full: 0..count-1
    // If full: head..MAX-1, 0..head-1

    if (historyCount < HISTORY_SIZE) {
      for (size_t i = 0; i < historyCount; i++) {
        target.push_back(history[i]);
      }
    } else {
      // Ring buffer logic
      // 1. head to End
      for (size_t i = historyHead; i < HISTORY_SIZE; i++) {
        target.push_back(history[i]);
      }
      // 2. 0 to head
      for (size_t i = 0; i < historyHead; i++) {
        target.push_back(history[i]);
      }
    }
    xSemaphoreGive(dataMutex);
  }
}

size_t SensorManager::copyHistory(size_t offset, size_t count,
                                  Record *destination) {
  if (!destination || offset >= historyCount)
    return 0;

  size_t actualCopied = 0;

  if (xSemaphoreTake(dataMutex,
                     pdMS_TO_TICKS(100))) { // Short timeout for chunk access
    // Calculate safe count
    size_t available = historyCount - offset;
    size_t toCopy = (count < available) ? count : available;

    // The logic for "offset" implies logical index 0..historyCount-1
    // (Where 0 is Oldest)

    // Logical index 'i' maps to ring buffer index:
    // IF historyCount < HISTORY_SIZE:
    //    RealIndex = i
    // (If full) Oldest is at head.
    //    RealIndex = (head + i) % MAX

    size_t startRealIndex;
    if (historyCount < HISTORY_SIZE) {
      startRealIndex = offset;
    } else {
      startRealIndex = (historyHead + offset) % HISTORY_SIZE;
    }

    // We could do this in one or two memcpy blocks for speed,
    // but a loop is safer and easier to read for limited sizes (e.g. 50).
    for (size_t i = 0; i < toCopy; i++) {
      size_t ringIndex = (startRealIndex + i) % HISTORY_SIZE;
      destination[i] = history[ringIndex];
    }

    actualCopied = toCopy;
    xSemaphoreGive(dataMutex);
  }
  return actualCopied;
}

Record SensorManager::getHistoryPoint(size_t index) const {
  // Note: This method is inherently unsafe if called while writing happens
  // Prefer getHistoryCopy() for bulk access

  if (index >= historyCount)
    return {0, NAN, NAN};

  // If buffer is NOT full: head is at (count), so oldest is at 0.
  // If buffer IS full: head is typically Oldest.

  // Actually, head always points to the NEXT write slot, which means the slot
  // at 'head' is the Oldest (if full). If not full, 0 is the oldest.

  size_t actualIndex;
  if (historyCount < HISTORY_SIZE) {
    // Usage [0, 1, 2 ... count-1]
    actualIndex = index;
  } else {
    // Usage [head, head+1 ... MAX-1, 0 ... head-1]
    // Oldest is 'head'.
    actualIndex = (historyHead + index) % HISTORY_SIZE;
  }

  return history[actualIndex];
}

// =============================================================================
// STATE MACHINE v6.0 - PURE DECISION FUNCTION
// =============================================================================
// This function has NO side effects. It takes all inputs and returns all outputs.
// This makes the state machine fully testable without mocking Arduino.
//
SensorManager::StateOutput
SensorManager::computeTransition(const StateInput& in) {
  StateOutput out;
  
  // Initialize output with current state (no change by default)
  out.newState = in.currentState;
  out.newStateEnterTime = in.stateEnterTime;
  out.updateBaseline = false;
  out.newBaseTemp = in.baseTemp;
  out.newBaseHum = in.baseHum;
  out.newBaseAbsHum = in.baseAbsHum;
  out.newTriggerCount = in.triggerConfirmCount;
  out.newPlateauCount = in.plateauConfirmCount;
  out.newBaselineCounter = in.baselineUpdateCount;
  out.newReboundStartTemp = in.reboundStartTemp;
  out.newReboundStartTime = in.reboundStartTime;
  out.newStateEnterHum = in.stateEnterHum;
  out.newStateEnterAbsHum = in.stateEnterAbsHum;
  out.transitionReason = nullptr;

  unsigned long timeSinceStateEnter = in.now - in.stateEnterTime;

  // =========================================================================
  // 1. STABLE STATE — Monitoring for ventilation start
  // =========================================================================
  if (in.currentState == ClimateState::STABLE) {
    // Reset plateau tracking (will be applied by caller)
    out.newPlateauCount = 0;
    out.newReboundStartTemp = NAN;

    bool lockoutActive = (timeSinceStateEnter < STATE_LOCKOUT_MS);

    // Trigger conditions
    bool rapidHumDrop =
        (!isnan(in.baseHum) &&
         (in.baseHum - in.hum) > VENT_TRIGGER_HUM_DROP);
    bool rapidTempDrop =
        (!isnan(in.baseTemp) &&
         (in.baseTemp - in.temp) > VENT_TRIGGER_TEMP_DROP);
    bool rapidAbsHumDrop =
        (!isnan(in.prevAbsHum) &&
         (in.prevAbsHum - in.absHum) > VENT_TRIGGER_ABSHUM_DROP);

    bool anyTrigger = (rapidHumDrop || rapidTempDrop || rapidAbsHumDrop);

    if (anyTrigger && !lockoutActive) {
      out.newTriggerCount = in.triggerConfirmCount + 1;
      if (out.newTriggerCount >= VENT_TRIGGER_CONFIRM) {
        // TRANSITION: STABLE -> VENTILATING
        out.newState = ClimateState::VENTILATING;
        out.newStateEnterTime = in.now;
        out.newStateEnterHum = in.hum;
        out.newStateEnterAbsHum = in.absHum;
        out.updateBaseline = true;
        out.newBaseTemp = in.temp;
        out.newBaseHum = in.hum;
        out.newBaseAbsHum = in.absHum;
        out.newTriggerCount = 0;
        out.newPlateauCount = 0;
        out.newReboundStartTemp = NAN;
        out.transitionReason = "STABLE -> VENTILATING (Smart Trigger)";
      }
    } else {
      out.newTriggerCount = 0;
    }

    // Baseline update (every 50 readings)
    out.newBaselineCounter = in.baselineUpdateCount + 1;
    if (out.newBaselineCounter >= 50) {
      out.newBaselineCounter = 0;
      out.updateBaseline = true;
      out.newBaseTemp = in.temp;
      out.newBaseHum = in.hum;
      out.newBaseAbsHum = in.absHum;
    }
  }

  // =========================================================================
  // 2. VENTILATING STATE — Active drying
  // =========================================================================
  else if (in.currentState == ClimateState::VENTILATING) {
    unsigned long dur = timeSinceStateEnter;

    // A. SUCCESS CONDITION
    float targetHum = fmax(50.0f, in.stateEnterHum - 15.0f);
    if (in.hum <= targetHum) {
      out.newState = ClimateState::TARGET_MET;
      out.newStateEnterTime = in.now;
      out.updateBaseline = true;
      out.newBaseTemp = in.temp;
      out.newBaseHum = in.hum;
      out.newBaseAbsHum = in.absHum;
      out.newReboundStartTemp = NAN;
      out.transitionReason = "VENTILATING -> TARGET_MET (Target reached)";
    }
    // B. PLATEAU DETECTION
    else if (dur > 180000 && in.slopeCount >= 3) {
      float slope = in.slopeNewest - in.slopeOldest;
      
      // Adaptive threshold
      float adaptiveThreshold = PLATEAU_SLOPE_THRESHOLD;
      if (!isnan(in.stateEnterHum)) {
        float humFactor = fmin(fmax(in.stateEnterHum, 50.0f), 70.0f) - 50.0f;
        adaptiveThreshold = PLATEAU_SLOPE_THRESHOLD - (humFactor * 0.003f);
      }

      if (!isnan(slope) && slope > adaptiveThreshold) {
        out.newPlateauCount = in.plateauConfirmCount + 1;
        if (out.newPlateauCount >= PLATEAU_CONFIRM_COUNT) {
          out.newState = ClimateState::INEFFICIENT;
          out.newStateEnterTime = in.now;
          out.updateBaseline = true;
          out.newBaseTemp = in.temp;
          out.newBaseHum = in.hum;
          out.newBaseAbsHum = in.absHum;
          out.newReboundStartTemp = NAN;
          out.transitionReason = "VENTILATING -> INEFFICIENT (Plateau)";
        }
      } else {
        out.newPlateauCount = 0;
      }
    }

    // C. REBOUND DETECTION (Window Closed)
    if (out.newState == ClimateState::VENTILATING) { // Only if not already transitioning
      if (!isnan(in.reboundStartTemp)) {
        float tempRise = in.temp - in.reboundStartTemp;
        unsigned long reboundDur = in.now - in.reboundStartTime;
        
        if (tempRise > REBOUND_TEMP_RISE && reboundDur > REBOUND_TIME_MS) {
          out.newState = ClimateState::STABLE;
          out.newStateEnterTime = in.now;
          out.updateBaseline = true;
          out.newBaseTemp = in.temp;
          out.newBaseHum = in.hum;
          out.newBaseAbsHum = in.absHum;
          out.transitionReason = "VENTILATING -> STABLE (Temp rebound)";
        } else if (in.temp < in.reboundStartTemp) {
          out.newReboundStartTemp = NAN;
        }
      } else {
        if (in.temp > in.baseTemp + 0.05f) {
          out.newReboundStartTemp = in.baseTemp;
          out.newReboundStartTime = in.now;
        }
      }

      // Fallback: AbsHum rebound
      if (out.newState == ClimateState::VENTILATING &&
          (in.absHum - in.baseAbsHum) > REBOUND_ABSHUM_RISE) {
        out.newState = ClimateState::STABLE;
        out.newStateEnterTime = in.now;
        out.updateBaseline = true;
        out.newBaseTemp = in.temp;
        out.newBaseHum = in.hum;
        out.newBaseAbsHum = in.absHum;
        out.transitionReason = "VENTILATING -> STABLE (AbsHum rebound)";
      }
    }
  }

  // =========================================================================
  // 3. TARGET_MET — Success! Waiting for window close
  // =========================================================================
  else if (in.currentState == ClimateState::TARGET_MET) {
    // Rebound detection
    if (!isnan(in.reboundStartTemp)) {
      float tempRise = in.temp - in.reboundStartTemp;
      unsigned long reboundDur = in.now - in.reboundStartTime;
      
      if (tempRise > REBOUND_TEMP_RISE && reboundDur > REBOUND_TIME_MS) {
        out.newState = ClimateState::STABLE;
        out.newStateEnterTime = in.now;
        out.updateBaseline = true;
        out.newBaseTemp = in.temp;
        out.newBaseHum = in.hum;
        out.newBaseAbsHum = in.absHum;
        out.transitionReason = "TARGET_MET -> STABLE (Rebound)";
      } else if (in.temp < in.reboundStartTemp) {
        out.newReboundStartTemp = NAN;
      }
    } else {
      if (in.temp > in.baseTemp + 0.05f) {
        out.newReboundStartTemp = in.baseTemp;
        out.newReboundStartTime = in.now;
      }
    }

    // AbsHum rebound
    if (out.newState == ClimateState::TARGET_MET &&
        (in.absHum - in.baseAbsHum) > REBOUND_ABSHUM_RISE) {
      out.newState = ClimateState::STABLE;
      out.newStateEnterTime = in.now;
      out.updateBaseline = true;
      out.newBaseTemp = in.temp;
      out.newBaseHum = in.hum;
      out.newBaseAbsHum = in.absHum;
      out.transitionReason = "TARGET_MET -> STABLE (AbsHum rebound)";
    }

    // Timeout
    if (out.newState == ClimateState::TARGET_MET && timeSinceStateEnter > 3600000) {
      out.newState = ClimateState::STABLE;
      out.newStateEnterTime = in.now;
      out.updateBaseline = true;
      out.newBaseTemp = in.temp;
      out.newBaseHum = in.hum;
      out.newBaseAbsHum = in.absHum;
      out.transitionReason = "TARGET_MET -> STABLE (Timeout 1h)";
    }
  }

  // =========================================================================
  // 4. INEFFICIENT — Plateau reached, waiting for window close
  // =========================================================================
  else if (in.currentState == ClimateState::INEFFICIENT) {
    // Same rebound logic as TARGET_MET
    if (!isnan(in.reboundStartTemp)) {
      float tempRise = in.temp - in.reboundStartTemp;
      unsigned long reboundDur = in.now - in.reboundStartTime;
      
      if (tempRise > REBOUND_TEMP_RISE && reboundDur > REBOUND_TIME_MS) {
        out.newState = ClimateState::STABLE;
        out.newStateEnterTime = in.now;
        out.updateBaseline = true;
        out.newBaseTemp = in.temp;
        out.newBaseHum = in.hum;
        out.newBaseAbsHum = in.absHum;
        out.transitionReason = "INEFFICIENT -> STABLE (Rebound)";
      } else if (in.temp < in.reboundStartTemp) {
        out.newReboundStartTemp = NAN;
      }
    } else {
      if (in.temp > in.baseTemp + 0.05f) {
        out.newReboundStartTemp = in.baseTemp;
        out.newReboundStartTime = in.now;
      }
    }

    // AbsHum rebound
    if (out.newState == ClimateState::INEFFICIENT &&
        (in.absHum - in.baseAbsHum) > REBOUND_ABSHUM_RISE) {
      out.newState = ClimateState::STABLE;
      out.newStateEnterTime = in.now;
      out.updateBaseline = true;
      out.newBaseTemp = in.temp;
      out.newBaseHum = in.hum;
      out.newBaseAbsHum = in.absHum;
      out.transitionReason = "INEFFICIENT -> STABLE (AbsHum rebound)";
    }

    // Timeout
    if (out.newState == ClimateState::INEFFICIENT && timeSinceStateEnter > 3600000) {
      out.newState = ClimateState::STABLE;
      out.newStateEnterTime = in.now;
      out.updateBaseline = true;
      out.newBaseTemp = in.temp;
      out.newBaseHum = in.hum;
      out.newBaseAbsHum = in.absHum;
      out.transitionReason = "INEFFICIENT -> STABLE (Timeout 1h)";
    }
  }

  return out;
}

void SensorManager::processReading(float rawT, float rawH) {
  float t = rawT + TEMP_OFFSET;
  float h = constrain(rawH + HUM_OFFSET, 0.0f,
                      100.0f); // FIX: Prevent impossible humidity values

  // Filter
  if (!isnan(lastValidTemp) && abs(t - lastValidTemp) > MAX_TEMP_JUMP) {
    t = lastValidTemp;
  } else {
    if (!isnan(lastValidTemp))
      t = (lastValidTemp * 0.8f) + (t * 0.2f);
    else
      lastValidTemp = t;
  }
  lastValidTemp = t;

  currentTemp = t;
  currentHum = h;
  // PHYSICS ENGINE UPDATE: Absolute Humidity
  currentAbsHum = ClimateMath::calculateAbsHumidity(currentTemp, currentHum);
  currentDP = ClimateMath::calculateDewPoint(currentTemp, currentHum);

  // FIX v3.4: Initialize baseline on first valid reading
  // This enables triggers to work immediately after boot
  if (isnan(lastHumForWindowCheck)) {
    lastHumForWindowCheck = currentHum;
    lastTempForWindowCheck = currentTemp;
    lastAbsHumForWindowCheck = currentAbsHum;
    prevAbsHumForTrigger = currentAbsHum;
  }

  // --- STATE MACHINE v6.0 - Pure Function Pattern ---
  unsigned long now = millis();

  // Calculate slope window summary for plateau detection
  float slopeNewest = NAN, slopeOldest = NAN;
  if (slopeWindowCount >= 3) {
    size_t oldestIdx = (slopeWindowHead + SLOPE_WINDOW_SIZE - slopeWindowCount) % SLOPE_WINDOW_SIZE;
    slopeOldest = slopeWindow[oldestIdx];
    slopeNewest = slopeWindow[(slopeWindowHead + SLOPE_WINDOW_SIZE - 1) % SLOPE_WINDOW_SIZE];
  }

  // Pack all inputs for the pure decision function
  StateInput in;
  in.temp = currentTemp;
  in.hum = currentHum;
  in.absHum = currentAbsHum;
  in.baseTemp = lastTempForWindowCheck;
  in.baseHum = lastHumForWindowCheck;
  in.baseAbsHum = lastAbsHumForWindowCheck;
  in.prevAbsHum = prevAbsHumForTrigger;
  in.currentState = state;
  in.stateEnterTime = stateEnterTime;
  in.now = now;
  in.triggerConfirmCount = triggerConfirmCount;
  in.plateauConfirmCount = plateauConfirmCounter;
  in.baselineUpdateCount = baselineUpdateCounter;
  in.reboundStartTemp = reboundStartTemp;
  in.reboundStartTime = reboundStartTime;
  in.slopeNewest = slopeNewest;
  in.slopeOldest = slopeOldest;
  in.slopeCount = slopeWindowCount;
  in.stateEnterHum = stateEnterHum;
  in.stateEnterAbsHum = stateEnterAbsHum;

  // Call pure decision function
  StateOutput out = computeTransition(in);

  // Apply output to class state
  state = out.newState;
  stateEnterTime = out.newStateEnterTime;
  triggerConfirmCount = out.newTriggerCount;
  plateauConfirmCounter = out.newPlateauCount;
  baselineUpdateCounter = out.newBaselineCounter;
  reboundStartTemp = out.newReboundStartTemp;
  reboundStartTime = out.newReboundStartTime;
  stateEnterHum = out.newStateEnterHum;
  stateEnterAbsHum = out.newStateEnterAbsHum;

  if (out.updateBaseline) {
    lastTempForWindowCheck = out.newBaseTemp;
    lastHumForWindowCheck = out.newBaseHum;
    lastAbsHumForWindowCheck = out.newBaseAbsHum;
  }

  // Log state transitions for debugging
  if (out.transitionReason != nullptr) {
    Serial.printf("[STATE] %s\n", out.transitionReason);
  }

  // Reset plateau tracking when entering STABLE
  if (state == ClimateState::STABLE) {
    slopeWindowCount = 0;
    slopeWindowHead = 0;
    reboundDetected = false;
  }

  // Update previous AbsHum for next iteration
  prevAbsHumForTrigger = currentAbsHum;

  // Physics Tracking Update
  lastAbsHum = currentAbsHum;
}

// DEPRECATED: Physics logic moved inside processReading
// float SensorManager::calculateDropRate() const { ... }

// Getters
float SensorManager::getTemp() const { return currentTemp; }
float SensorManager::getHum() const { return currentHum; }
float SensorManager::getDewPoint() const { return currentDP; }
float SensorManager::getAvg24h() const { return avg24h; }
bool SensorManager::isRapidChange() const {
  return state != ClimateState::STABLE;
}
String SensorManager::getStateString() const {
  switch (state) {
  case ClimateState::STABLE:
    return "STABLE";
  case ClimateState::VENTILATING:
    return "VENT";
  case ClimateState::TARGET_MET:
    return "TARGET";
  case ClimateState::INEFFICIENT:
    return "INEFFICIENT";
  default:
    return "???";
  }
}

SensorManager::ClimateState SensorManager::getClimateState() const {
  return state;
}
int SensorManager::getStateCode() const { return (int)state; }
unsigned long SensorManager::getStateEnterTime() const {
  return stateEnterTime;
}

// --- DRYING SPEEDOMETER v3.3 ---
float SensorManager::getDryingRate() const {
  // Only calculate rate during active drying states
  if (state != ClimateState::VENTILATING && state != ClimateState::TARGET_MET) {
    return 0.0f;
  }

  unsigned long dur = millis() - stateEnterTime;
  if (dur < 60000 || isnan(stateEnterAbsHum)) {
    return 0.0f; // Need at least 1 min of data
  }

  // Rate in g/m³/min (negative = drying, we return positive for display)
  float absHumDrop = stateEnterAbsHum - currentAbsHum;
  float minutes = dur / 60000.0f;
  return absHumDrop / minutes;
}

String SensorManager::getDryingIndicator() const {
  float rate = getDryingRate();

  if (rate <= 0.0f) {
    return "-"; // Not drying or state not applicable
  }

  // Use thresholds from Settings.h
  if (rate >= RATE_EXCELLENT) {
    return "▲▲"; // Fast drying
  } else if (rate >= RATE_GOOD) {
    return "▲"; // Normal drying
  } else {
    return "▼"; // Slow drying
  }
}

void SensorManager::setWeatherManager(WeatherManager *wm) {
  this->weather = wm;
}

float SensorManager::getOutdoorTemp() const {
  return (weather && weather->isDataValid()) ? weather->getOutdoorTemp() : NAN;
}
float SensorManager::getOutdoorHum() const {
  return (weather && weather->isDataValid()) ? weather->getOutdoorHum() : NAN;
}
float SensorManager::getOutdoorAbsHum() const {
  return (weather && weather->isDataValid()) ? weather->getOutdoorAbsHum()
                                             : NAN;
}
float SensorManager::getIndoorAbsHum() const {
  return ClimateMath::calculateAbsHumidity(currentTemp, currentHum);
}
bool SensorManager::isWeatherValid() const {
  return (weather && weather->isDataValid());
}
String SensorManager::getWeatherStatus() const {
  return weather ? weather->getStatusString() : "No Manager";
}
