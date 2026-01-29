#pragma once

/**
 * @file CoreState.h
 * @brief Central Data Hub - Single Source of Truth for all application state.
 * 
 * All managers WRITE here after processing.
 * All consumers READ from here.
 * This decouples components and enables "vibecoding" - adding features without touching unrelated code.
 */

#include <Arduino.h>
#include <freertos/semphr.h>
#include <vector>
#include "Settings.h" 

// =============================================================================
// SHARED DATA STRUCTURES
// =============================================================================

// Optimized Record (12 bytes)
struct Record {
  uint32_t ts;
  float t;
  float h;
};

// =============================================================================
// ENUMS (Shared across modules)
// =============================================================================

enum class ClimateState { 
    STABLE,       // Normal operation, monitoring
    VENTILATING,  // Window open, drying in progress
    TARGET_MET,   // Success! Humidity target reached
    INEFFICIENT   // Plateau detected, diminishing returns
};

// =============================================================================
// CORE SNAPSHOT (POJO for UI/Consumers)
// =============================================================================

struct CoreSnapshot {
    float temp;
    float hum;
    float dewPoint;
    float absHum;
    float avg24h;
    ClimateState state;
    unsigned long stateEnterTime;
    String advice;
    int adviceCode;
    float dryingRate;
    String dryingInd;
    float outTemp;
    float outHum;
    float outAbsHum;
    bool weatherValid;
    String weatherStatus;
    String ipAddress;
    uint32_t heapFree;
    uint32_t heapMin;
};

// =============================================================================
// CORE STATE STRUCTURE
// =============================================================================

struct CoreState {
    // -------------------------------------------------------------------------
    // Indoor Climate Data (Written by SensorManager)
    // -------------------------------------------------------------------------
    float temp        = NAN;   // Current temperature (°C)
    float hum         = NAN;   // Current relative humidity (%)
    float dewPoint    = NAN;   // Dew point (°C)
    float absHum      = NAN;   // Absolute humidity (g/m³)
    float avg24h      = NAN;   // 24h rolling average humidity (%)
    
    // -------------------------------------------------------------------------
    // State Machine (Written by SensorManager)
    // -------------------------------------------------------------------------
    ClimateState state = ClimateState::STABLE;
    unsigned long stateEnterTime = 0;
    
    // -------------------------------------------------------------------------
    // Advice Engine Output (Written by SensorManager)
    // -------------------------------------------------------------------------
    String advice     = "Загрузка...";
    int adviceCode    = 0;  // 0=Normal, 1=Vent, 2=Critical, 3=Optimal
    
    // -------------------------------------------------------------------------
    // Drying Statistics (Written by SensorManager)
    // -------------------------------------------------------------------------
    float dryingRate  = 0.0f;   // g/m³/min
    String dryingInd  = "-";    // ▲▲, ▲, ▼, or -
    
    // -------------------------------------------------------------------------
    // Outdoor Weather (Written by WeatherManager)
    // -------------------------------------------------------------------------
    float outTemp       = NAN;
    float outHum        = NAN;
    float outAbsHum     = NAN;
    bool weatherValid   = false;
    String weatherStatus = "N/A";
    
    // -------------------------------------------------------------------------
    // System Info (Written by main.cpp or various managers)
    // -------------------------------------------------------------------------
    String ipAddress    = "0.0.0.0";
    uint32_t heapFree   = 0;
    uint32_t heapMin    = 0;
    
    // Heartbeat & Coordination
    uint32_t recommendedPollInterval = 6000; // Default 6s
    uint32_t lastDataTimestamp       = 0;    // When (millis) did last data arrive?

    // -------------------------------------------------------------------------
    // History Data (Centralized Storage)
    // -------------------------------------------------------------------------
    Record history[HISTORY_SIZE];
    size_t historyHead  = 0;
    size_t historyCount = 0;

    void addHistoryPoint(float t, float h) {
        // NOTE: Caller must hold lock()!
        time_t now = time(NULL);
        if (now < 1600000000) return; // Skip invalid time

        Record r = {(uint32_t)now, t, h};
        history[historyHead] = r;
        historyHead = (historyHead + 1) % HISTORY_SIZE;
        if (historyCount < HISTORY_SIZE) historyCount++;
    }

    // Thread-Safe Snapshot for UI
    CoreSnapshot getSnapshot() {
        CoreSnapshot sn;
        lock();
        sn.temp = temp;
        sn.hum = hum;
        sn.dewPoint = dewPoint;
        sn.absHum = absHum;
        sn.avg24h = avg24h;
        sn.state = state;
        sn.stateEnterTime = stateEnterTime;
        sn.advice = advice;
        sn.adviceCode = adviceCode;
        sn.dryingRate = dryingRate;
        sn.dryingInd = dryingInd;
        sn.outTemp = outTemp;
        sn.outHum = outHum;
        sn.outAbsHum = outAbsHum;
        sn.weatherValid = weatherValid;
        sn.weatherStatus = weatherStatus;
        sn.ipAddress = ipAddress;
        sn.heapFree = heapFree;
        sn.heapMin = heapMin;
        unlock();
        return sn;
    }

    // Thread-Safe Chunk Access for WebManager
    size_t copyHistoryChunk(size_t offset, size_t count, Record *destination) {
        size_t actualCopied = 0;
        lock();
        if (offset < historyCount && destination) {
            size_t available = historyCount - offset;
            size_t toCopy = (count < available) ? count : available;
            
            size_t startRealIndex;
            if (historyCount < HISTORY_SIZE) {
                startRealIndex = offset;
            } else {
                startRealIndex = (historyHead + offset) % HISTORY_SIZE;
            }

            for (size_t i = 0; i < toCopy; i++) {
                size_t ringIndex = (startRealIndex + i) % HISTORY_SIZE;
                destination[i] = history[ringIndex];
            }
            actualCopied = toCopy;
        }
        unlock();
        return actualCopied;
    }
    
    // -------------------------------------------------------------------------
    // Thread Safety
    // -------------------------------------------------------------------------
    SemaphoreHandle_t mutex = 0;
    
    void init() {
        mutex = xSemaphoreCreateMutex();
    }
    
    void lock() { 
        if (mutex) xSemaphoreTake(mutex, pdMS_TO_TICKS(50)); 
    }
    
    void unlock() { 
        if (mutex) xSemaphoreGive(mutex); 
    }
    
    // -------------------------------------------------------------------------
    // Event System (Reactive)
    // -------------------------------------------------------------------------
    TaskHandle_t mainTaskHandle = NULL;

    void notifyCoordinator() {
        if (mainTaskHandle) {
            xTaskNotifyGive(mainTaskHandle);
        }
    }
    
    // -------------------------------------------------------------------------
    // Helper: Get state as string (for debugging)
    // -------------------------------------------------------------------------
    String getStateString() const {
        switch (state) {
            case ClimateState::STABLE:      return "STABLE";
            case ClimateState::VENTILATING: return "VENT";
            case ClimateState::TARGET_MET:  return "TARGET";
            case ClimateState::INEFFICIENT: return "INEFFICIENT";
            default:                        return "???";
        }
    }
    
    int getStateCode() const {
        return static_cast<int>(state);
    }
};

// =============================================================================
// GLOBAL INSTANCE (declared in CoreState.cpp or main.cpp)
// =============================================================================
extern CoreState g_state;
