#include "SensorManager.h"
#include "WeatherManager.h" 
#include "ClimateMath.h"

SensorManager::SensorManager() 
    : dht(DHTPIN, DHTTYPE), 
      currentTemp(NAN), currentHum(NAN), currentDP(NAN), currentAbsHum(NAN), avg24h(NAN),
      lastValidTemp(NAN), lastTempForWindowCheck(NAN), windowOpen(false), 
      state(ClimateState::STABLE), stateEnterTime(0), weather(nullptr),
      historyHead(0), historyCount(0), lastLogTime(0),
      cachedAdvice("Загрузка..."), cachedCode(0), lastAdviceUpdate(0),
      // FIX: Initialize all physics tracking variables to NAN
      lastAbsHumForWindowCheck(NAN), stateEnterAbsHum(NAN), lastAbsHum(NAN), stateEnterHum(NAN),
      // Plateau v2.0 initialization
      slopeWindowHead(0), slopeWindowCount(0), plateauConfirmCounter(0), baselineUpdateCounter(0),
      // Improved Rebound Detection
      reboundStartTime(0), reboundStartTemp(NAN), reboundDetected(false)
{
    dataMutex = xSemaphoreCreateMutex();
    // Initialize slope window to NAN
    for (size_t i = 0; i < SLOPE_WINDOW_SIZE; i++) {
        slopeWindow[i] = NAN;
    }
}

void SensorManager::lock() {
    // Use timeout to prevent infinite deadlock (WebServer locking during slow network)
    if(dataMutex) xSemaphoreTake(dataMutex, pdMS_TO_TICKS(500));
}

void SensorManager::unlock() {
    if(dataMutex) xSemaphoreGive(dataMutex);
}

// -------------------------------------------------------------------------
// OPTIMIZATION 4: Async Sensor Task
// -------------------------------------------------------------------------
void SensorManager::sensorTask(void* parameter) {
    SensorManager* self = (SensorManager*)parameter;
    const TickType_t intervalTicks = pdMS_TO_TICKS(6000);
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    for(;;) {
        // Read DHT (blocking, but isolated in this task)
        float t = self->dht.readTemperature();
        float h = self->dht.readHumidity();

        // Only process when both values are valid
        if (!isnan(t) && !isnan(h)) {
            // Acquire mutex just for the processing step – keep critical section short
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
    // Launch the background task on Core 1 (App Core) to avoid stalling Core 0 (WiFi/Radio)
    // NOTE: Standard DHT library blocks interrupts, which causes crashes on Core 0.
    // Moving to Core 1 makes the UI slightly stutter every 6s, but prevents random reboots.
    // CRITICAL: 8KB stack to prevent overflow during complex state machine operations
    // Allocate a larger stack (10KB) to avoid overflow during complex state handling.
    // Use core 1 to keep Wi‑Fi/Radio responsive.
    xTaskCreatePinnedToCore(
        SensorManager::sensorTask,   // Function
        "DHT_Task",                  // Name
        10 * 1024,                  // Stack size (10KB)
        this,                        // Param
        2,                           // Priority (Middle-High)
        NULL,                        // Handle
        1                            // Core 1
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
                
                // Update slopeWindow for Plateau v2.0 (synchronized with history logging)
                // Only during ventilation - 6 points × 30 sec = 3 min window
                if (state == ClimateState::VENTILATING || 
                    state == ClimateState::TARGET_MET || 
                    state == ClimateState::INEFFICIENT) {
                    slopeWindow[slopeWindowHead] = currentAbsHum;
                    slopeWindowHead = (slopeWindowHead + 1) % SLOPE_WINDOW_SIZE;
                    if (slopeWindowCount < SLOPE_WINDOW_SIZE) slopeWindowCount++;
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
    } 
    else if (state == ClimateState::INEFFICIENT) {
        s = "Эффективность упала. Закрыть.";
        code = 2; // Red
    }
    else if (state == ClimateState::TARGET_MET) {
        s = "Цель (50%) достигнута! Можно закрыть.";
        code = 3; // Green
    }
    else if (state == ClimateState::VENTILATING) {
        // Physics-based advice
        s = "Сушка (Идет активное проветривание)";
        code = 1; // Yellow
    }
    else {
        // STABLE
        float outTemp = (weather && weather->isDataValid()) ? weather->getOutdoorTemp() : 20.0;
        
        // Winter
        if (outTemp < 10.0) {
            float margin = currentTemp - currentDP;
            if (margin < 3.0) { s = "КРИТИЧНО! ГРЕТЬ/ОСУШАТЬ"; code = 2; }
            else if (currentHum > 55.0) { s = "Влажно [ЗАЛП 5 мин]"; code = 1; }
            else { s = "Зимняя Норма"; code = 3; }
        }
        // Summer
        else if (outTemp > 18.0) {
             if (weather && weather->isDataValid()) {
                float inAbs = ClimateMath::calculateAbsHumidity(currentTemp, currentHum);
                float outAbs = weather->getOutdoorAbsHum();
                if (outAbs > inAbs) { s = "Влажно [НЕ ОТКРЫВАТЬ!]"; code = 3; } // Blue/Green
                else if (currentHum > 60.0) { s = "Влажно [Проветрить]"; code = 1; } // Yellow
                else { s = "Летняя Норма"; code = 3; }
             } else {
                 if (currentHum > 60.0) { s = "Влажно [Проветрить]"; code = 1; }
                 else { s = "Летняя Норма"; code = 3; }
             }
        }
        // Transition
        else {
            if ((currentTemp - currentDP) < 2.5) { s = "КРИТИЧНО! Открыть окно"; code = 2; }
            else if (currentHum > 60.0) { s = "Влажно [Реком. проветрить]"; code = 1; }
            else if (currentHum < 35.0) { s = "Сухой воздух [Увлажнить]"; code = 3; }
            else { s = "Норма (Стены сохнут)"; code = 3; }
        }
    }
    
    cachedAdvice = s;
    cachedCode = code;
}

String SensorManager::getRecommendation() {
    // Return cached value (Thread safe read technically requires mutex but String copy is atomic enough for display)
    // For strict correctness we take mutex for string copy
    String copy;
    if(xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10))) {
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
    if (now < 1600000000) return; 

    // EMA for Daily Avg
    if(isnan(avg24h)) avg24h = h;
    else avg24h = (avg24h * 0.99f) + (h * 0.01f);

    Record r = {(uint32_t)now, t, h};
    
    // Ring Buffer Write
    history[historyHead] = r;
    historyHead = (historyHead + 1) % HISTORY_SIZE;
    if (historyCount < HISTORY_SIZE) historyCount++;
}

size_t SensorManager::getHistoryCount() const { return historyCount; }

void SensorManager::getHistoryCopy(std::vector<Record>& target) {
    if(xSemaphoreTake(dataMutex, pdMS_TO_TICKS(200))) {
        target.clear();
        target.reserve(historyCount);
        
        // Reconstruct order: Oldest -> Newest
        // If not full: 0..count-1
        // If full: head..MAX-1, 0..head-1
        
        if (historyCount < HISTORY_SIZE) {
            for(size_t i = 0; i < historyCount; i++) {
                target.push_back(history[i]);
            }
        } else {
             // Ring buffer logic
             // 1. head to End
             for(size_t i = historyHead; i < HISTORY_SIZE; i++) {
                 target.push_back(history[i]);
             }
             // 2. 0 to head
             for(size_t i = 0; i < historyHead; i++) {
                 target.push_back(history[i]);
             }
        }
        xSemaphoreGive(dataMutex);
    }
}

size_t SensorManager::copyHistory(size_t offset, size_t count, Record* destination) {
    if (!destination || offset >= historyCount) return 0;
    
    size_t actualCopied = 0;

    if(xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100))) { // Short timeout for chunk access
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
    
    if (index >= historyCount) return {0, NAN, NAN};

    // If buffer is NOT full: head is at (count), so oldest is at 0.
    // If buffer IS full: head is typically Oldest.
    
    // Actually, head always points to the NEXT write slot, which means the slot at 'head' is the Oldest (if full).
    // If not full, 0 is the oldest.
    
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

void SensorManager::processReading(float rawT, float rawH) {
    float t = rawT + TEMP_OFFSET;
    float h = constrain(rawH + HUM_OFFSET, 0.0f, 100.0f); // FIX: Prevent impossible humidity values

    // Filter
    if (!isnan(lastValidTemp) && abs(t - lastValidTemp) > MAX_TEMP_JUMP) {
         t = lastValidTemp; 
    } else {
         if (!isnan(lastValidTemp)) t = (lastValidTemp * 0.8f) + (t * 0.2f);
         else lastValidTemp = t;
    }
    lastValidTemp = t; 
    
    currentTemp = t;
    currentHum = h;
    // PHYSICS ENGINE UPDATE: Absolute Humidity
    currentAbsHum = ClimateMath::calculateAbsHumidity(currentTemp, currentHum);
    currentDP = ClimateMath::calculateDewPoint(currentTemp, currentHum);
    
    // --- SMART STATE MACHINE v5.2 ---
    unsigned long now = millis();
    
    // Helper: Update slope window (called only during VENTILATING logging)
    // We'll use this for Plateau v2.0 detection
    
    // =========================================================================
    // 1. STABLE STATE — Monitoring for ventilation start
    // =========================================================================
    if (state == ClimateState::STABLE) {
        // Reset plateau tracking
        plateauConfirmCounter = 0;
        slopeWindowCount = 0;
        slopeWindowHead = 0;
        reboundDetected = false;
        
        if (historyCount > 0) {
            Record last = getHistoryPoint(historyCount - 1); 
            
            // Detect Vent Start: Sudden Drop in RH or Temp
            // FIX: Add lockout period after returning from TARGET_MET/INEFFICIENT
            unsigned long timeSinceStable = now - stateEnterTime;
            bool lockoutActive = (timeSinceStable < 60000); // 60 sec lockout
            
            bool rapidHumDrop = (last.h - currentHum) > 3.0; // -3% Trigger
            bool rapidTempDrop = (!isnan(lastTempForWindowCheck) && (lastTempForWindowCheck - currentTemp) > 0.5);

            if ((rapidHumDrop || rapidTempDrop) && !lockoutActive) {
                state = ClimateState::VENTILATING;
                stateEnterTime = now;
                stateEnterAbsHum = currentAbsHum;
                stateEnterHum = currentHum; // Save for adaptive target
                lastTempForWindowCheck = currentTemp; 
                lastAbsHumForWindowCheck = currentAbsHum;
                
                // Reset plateau tracking for new ventilation session
                plateauConfirmCounter = 0;
                slopeWindowCount = 0;
                slopeWindowHead = 0;
                reboundDetected = false;
                reboundStartTemp = NAN;
                
                Serial.println("[STATE] STABLE -> VENTILATING");
            }
        }
        
        // Keep baseline updated (every 50 readings = ~5 minutes at 6s interval)
        // FIX: Using class member instead of static variable
        if (++baselineUpdateCounter >= 50) {
            baselineUpdateCounter = 0;
            lastTempForWindowCheck = currentTemp;
            lastAbsHumForWindowCheck = currentAbsHum;
        }
    }
    
    // =========================================================================
    // 2. VENTILATING STATE — Active drying, checking for success or plateau
    // =========================================================================
    else if (state == ClimateState::VENTILATING) {
        unsigned long dur = now - stateEnterTime;
        
        // NOTE: slopeWindow is now updated in update() every 30 sec (synchronized with history)
        // This gives us 6 points × 30 sec = 3 min window for plateau detection
        
        // --- A. SUCCESS CONDITION (Highest Priority) ---
        // Adaptive target: max(50%, startHum - 15%)
        float targetHum = max(50.0f, stateEnterHum - 15.0f);
        
        if (currentHum <= targetHum) {
            state = ClimateState::TARGET_MET;
            stateEnterTime = now;
            lastTempForWindowCheck = currentTemp;
            lastAbsHumForWindowCheck = currentAbsHum;
            reboundDetected = false;
            reboundStartTemp = NAN;
            Serial.printf("[STATE] VENTILATING -> TARGET_MET (%.1f%% <= %.1f%%)\n", currentHum, targetHum);
        }
        
        // --- B. PLATEAU DETECTION v2.0 (Only if NOT target met) ---
        // Conditions: After 3 min, with enough data, slope is flat for 90 seconds
        else if (dur > 180000 && slopeWindowCount >= 3) { // Check after 3 min with min 3 points
            
            // Calculate average slope from window
            // Simple approach: (newest - oldest) / count
            size_t oldestIdx = (slopeWindowHead + SLOPE_WINDOW_SIZE - slopeWindowCount) % SLOPE_WINDOW_SIZE;
            float oldestAbs = slopeWindow[oldestIdx];
            float newestAbs = slopeWindow[(slopeWindowHead + SLOPE_WINDOW_SIZE - 1) % SLOPE_WINDOW_SIZE];
            
            if (!isnan(oldestAbs) && !isnan(newestAbs)) {
                // slope in g/m³ over the window period (~3 min with 6 points at 30s intervals)
                float slope = newestAbs - oldestAbs;
                
                // If slope > -0.15 g/m³ over 3 min → very slow drying (< 0.05 g/m³/min)
                // This is our PLATEAU_THRESHOLD
                if (slope > -0.15f) {
                    plateauConfirmCounter++;
                    // Need 15 consecutive readings (~90 sec at 6s interval) to confirm
                    if (plateauConfirmCounter >= 15) {
                        // Additional check: Did we achieve at least 10% drop from start?
                        float dropPercent = ((stateEnterAbsHum - currentAbsHum) / stateEnterAbsHum) * 100.0f;
                        
                        state = ClimateState::INEFFICIENT;
                        stateEnterTime = now;
                        lastTempForWindowCheck = currentTemp;
                        lastAbsHumForWindowCheck = currentAbsHum;
                        reboundDetected = false;
                        reboundStartTemp = NAN;
                        Serial.printf("[STATE] VENTILATING -> INEFFICIENT (slope=%.3f, drop=%.1f%%)\n", slope, dropPercent);
                    }
                } else {
                    // Still drying effectively — reset confirmation counter
                    plateauConfirmCounter = 0;
                }
            }
        }
        
        // --- C. IMPROVED REBOUND DETECTION (Window Closed) ---
        // Use rate of temperature change, not absolute threshold
        if (!isnan(reboundStartTemp)) {
            float tempRise = currentTemp - reboundStartTemp;
            unsigned long reboundDur = now - reboundStartTime;
            
            // If temp rose by +0.15°C over 2 minutes → window is closed
            if (tempRise > 0.15f && reboundDur > 120000) {
                state = ClimateState::STABLE;
                stateEnterTime = now;
                lastTempForWindowCheck = currentTemp;
                lastAbsHumForWindowCheck = currentAbsHum;
                Serial.printf("[STATE] VENTILATING -> STABLE (Rebound: +%.2f°C in %lus)\n", tempRise, reboundDur/1000);
            }
            // If temp started falling again — reset rebound detection
            else if (currentTemp < reboundStartTemp) {
                reboundStartTemp = NAN;
                reboundDetected = false;
            }
        } else {
            // Start watching for rebound if temp starts rising
            if (!isnan(lastAbsHum) && currentTemp > lastTempForWindowCheck + 0.05f) {
                reboundStartTemp = lastTempForWindowCheck;
                reboundStartTime = now;
                reboundDetected = true;
            }
        }
        
        // Fallback: Old absolute threshold (faster for obvious window close)
        if (currentAbsHum - lastAbsHumForWindowCheck > 0.3f) {
            state = ClimateState::STABLE;
            stateEnterTime = now;
            lastTempForWindowCheck = currentTemp;
            lastAbsHumForWindowCheck = currentAbsHum;
            Serial.println("[STATE] VENTILATING -> STABLE (AbsHum rebound)");
        }
    }
    
    // =========================================================================
    // 3. TARGET_MET — Success! Waiting for window close confirmation
    // =========================================================================
    else if (state == ClimateState::TARGET_MET) {
        // Improved rebound detection (same logic)
        if (!isnan(reboundStartTemp)) {
            float tempRise = currentTemp - reboundStartTemp;
            unsigned long reboundDur = now - reboundStartTime;
            
            if (tempRise > 0.15f && reboundDur > 120000) {
                state = ClimateState::STABLE;
                stateEnterTime = now; // For lockout
                // FIX: Update baseline to prevent false re-detection
                lastTempForWindowCheck = currentTemp;
                lastAbsHumForWindowCheck = currentAbsHum;
                Serial.printf("[STATE] TARGET_MET -> STABLE (Rebound confirmed)\n");
            }
            else if (currentTemp < reboundStartTemp) {
                reboundStartTemp = NAN;
                reboundDetected = false;
            }
        } else {
            if (currentTemp > lastTempForWindowCheck + 0.05f) {
                reboundStartTemp = lastTempForWindowCheck;
                reboundStartTime = now;
            }
        }
        
        if (currentAbsHum - lastAbsHumForWindowCheck > 0.3f) {
            state = ClimateState::STABLE;
            stateEnterTime = now;
            lastTempForWindowCheck = currentTemp;
            lastAbsHumForWindowCheck = currentAbsHum;
            Serial.println("[STATE] TARGET_MET -> STABLE (AbsHum rebound)");
        }
        
        if (now - stateEnterTime > 3600000) {
            state = ClimateState::STABLE;
            stateEnterTime = now;
            lastTempForWindowCheck = currentTemp;
            lastAbsHumForWindowCheck = currentAbsHum;
            Serial.println("[STATE] TARGET_MET -> STABLE (Timeout 1h)");
        }
    }

    // =========================================================================
    // 4. INEFFICIENT — Plateau reached, waiting for window close
    // =========================================================================
    else if (state == ClimateState::INEFFICIENT) {
        // Same rebound detection as TARGET_MET
        if (!isnan(reboundStartTemp)) {
            float tempRise = currentTemp - reboundStartTemp;
            unsigned long reboundDur = now - reboundStartTime;
            
            if (tempRise > 0.15f && reboundDur > 120000) {
                state = ClimateState::STABLE;
                stateEnterTime = now;
                lastTempForWindowCheck = currentTemp;
                lastAbsHumForWindowCheck = currentAbsHum;
                Serial.printf("[STATE] INEFFICIENT -> STABLE (Rebound confirmed)\n");
            }
            else if (currentTemp < reboundStartTemp) {
                reboundStartTemp = NAN;
            }
        } else {
            if (currentTemp > lastTempForWindowCheck + 0.05f) {
                reboundStartTemp = lastTempForWindowCheck;
                reboundStartTime = now;
            }
        }
        
        if (currentAbsHum - lastAbsHumForWindowCheck > 0.3f) {
            state = ClimateState::STABLE;
            stateEnterTime = now;
            lastTempForWindowCheck = currentTemp;
            lastAbsHumForWindowCheck = currentAbsHum;
            Serial.println("[STATE] INEFFICIENT -> STABLE (AbsHum rebound)");
        }
        
        // Timeout fallback
        if (now - stateEnterTime > 3600000) {
            state = ClimateState::STABLE;
            stateEnterTime = now;
            lastTempForWindowCheck = currentTemp;
            lastAbsHumForWindowCheck = currentAbsHum;
            Serial.println("[STATE] INEFFICIENT -> STABLE (Timeout 1h)");
        }
    }
    
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
bool SensorManager::isRapidChange() const { return state != ClimateState::STABLE; } 
String SensorManager::getStateString() const {
    switch(state) {
        case ClimateState::STABLE: return "STABLE";
        case ClimateState::VENTILATING: return "VENT";
        case ClimateState::TARGET_MET: return "TARGET";
        case ClimateState::INEFFICIENT: return "INEFFICIENT";
        default: return "???";
    }
}

SensorManager::ClimateState SensorManager::getClimateState() const { return state; }
int SensorManager::getStateCode() const { return (int)state; }
unsigned long SensorManager::getStateEnterTime() const { return stateEnterTime; }

void SensorManager::setWeatherManager(WeatherManager* wm) {
    this->weather = wm;
}

float SensorManager::getOutdoorTemp() const { return (weather && weather->isDataValid()) ? weather->getOutdoorTemp() : NAN; }
float SensorManager::getOutdoorHum() const { return (weather && weather->isDataValid()) ? weather->getOutdoorHum() : NAN; }
float SensorManager::getOutdoorAbsHum() const { return (weather && weather->isDataValid()) ? weather->getOutdoorAbsHum() : NAN; }
float SensorManager::getIndoorAbsHum() const { return ClimateMath::calculateAbsHumidity(currentTemp, currentHum); }
bool SensorManager::isWeatherValid() const { return (weather && weather->isDataValid()); }
String SensorManager::getWeatherStatus() const { return weather ? weather->getStatusString() : "No Manager"; }
