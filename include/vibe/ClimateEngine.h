#pragma once
#include "CoreState.h"
#include "Settings.h"
#include <Arduino.h>

#include "AdviceEngine.h"

/**
 * @class ClimateEngine
 * @brief Processes climate data to manage ventilation states.
 * 
 * Isolated logic layer. Takes raw readings and computes transitions,
 * plateaus, and drying rates without knowing about hardware.
 */
class ClimateEngine {
public:
    ClimateEngine();
    
    // Result Packet for Coordinator
    struct EngineResult {
        bool stateChanged;
        ClimateState state;
        unsigned long stateEnterTime;
        
        // Advice (computed internally)
        String adviceText;
        int adviceCode;
        
        // Metrics
        float dryingRate;
        String dryingInd;
        
        // Operational Flags
        bool shouldLogHistory;
        uint32_t suggestPollInterval;
    };

    // Window Detection State & Physics Tracking
    // Note: Moving structs to public for static access
    struct StateInput {
        float temp, hum, absHum;
        float baseTemp, baseHum, baseAbsHum;
        float prevAbsHum;
        ClimateState currentState;
        unsigned long stateEnterTime;
        unsigned long now;
        unsigned int triggerConfirmCount;
        unsigned int plateauConfirmCount;
        unsigned int baselineUpdateCount;
        float reboundStartTemp;
        float reboundMinAbsHum;
        unsigned long reboundStartTime;
        float slopeNewest, slopeOldest;
        size_t slopeCount;
        float stateEnterHum, stateEnterAbsHum;
        unsigned long lastBaselineUpdateTime;
    };

    struct StateOutput {
        ClimateState newState;
        unsigned long newStateEnterTime;
        bool updateBaseline;
        float newBaseTemp, newBaseHum, newBaseAbsHum;
        unsigned int newTriggerCount;
        unsigned int newPlateauCount;
        unsigned int newBaselineCounter;
        float newReboundStartTemp;
        float newReboundMinAbsHum;
        unsigned long newReboundStartTime;
        float newStateEnterHum, newStateEnterAbsHum;
        const char* transitionReason;
    };

    /**
     * @brief Process new sensor readings and update internal state machine.
     * @return EngineResult with all necessary updates
     */
    EngineResult process(const CoreSnapshot& snapshot, unsigned long now);

    // 4. Run Pure Decision Logic
    static StateOutput computeTransition(const StateInput& in);

    // 5. Timing Helper (Centralized Heartbeat)
    unsigned long getSuggestedTickInterval() const;

    // Getters for values computed during process
    ClimateState getState() const { return state; }
    unsigned long getStateEnterTime() const { return stateEnterTime; }
    float getDryingRate(unsigned long now) const;
    String getDryingIndicator(unsigned long now) const;

    // 6. Synchronization Helper
    // Responsible for copying EngineResult into CoreState safely
    static void updateCoreState(CoreState& state, const EngineResult& result);

private:
    float lastTempForWindowCheck;
    float lastHumForWindowCheck;
    float lastAbsHumForWindowCheck;
    float stateEnterAbsHum;
    float stateEnterHum;

    // Plateau Detection
    static const size_t SLOPE_WINDOW_SIZE = 6;
    float slopeWindow[SLOPE_WINDOW_SIZE];
    size_t slopeWindowHead;
    size_t slopeWindowCount;
    unsigned int plateauConfirmCounter;
    unsigned int baselineUpdateCounter;

    // Improved Rebound Detection
    unsigned long reboundStartTime;
    float reboundStartTemp;
    float reboundMinAbsHum;

    // Smart Triggers
    unsigned int triggerConfirmCount;
    float prevAbsHumForTrigger;

    // State Machine
    ClimateState state;
    unsigned long stateEnterTime;

    void updateSlope(float absHum);
    
    // Internal Timers
    unsigned long lastHistoryLogTime;
    unsigned long lastSlopeUpdateTime;
    unsigned long lastBaselineUpdateTime;
};
