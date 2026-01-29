#pragma once

/**
 * @file AdviceEngine.h
 * @brief Stateless advice generator - Pure function pattern
 * 
 * Takes climate data input, outputs human-readable advice.
 * Zero side effects, easy to test, easy to modify.
 */

#include <Arduino.h>
#include "CoreState.h"

// =============================================================================
// INPUT STRUCTURE
// =============================================================================
struct AdviceInput {
    // Indoor climate
    float temp        = NAN;
    float hum         = NAN;
    float absHum      = NAN;
    float dewPoint    = NAN;
    
    // Outdoor weather
    float outTemp     = NAN;
    float outAbsHum   = NAN;
    bool weatherValid = false;
    
    // State machine
    ClimateState state = ClimateState::STABLE;
    unsigned long stateEnterTime = 0;
    unsigned long now = 0;
    
    // Drying indicator (passed in for display)
    String dryingInd = "-";
};

// =============================================================================
// OUTPUT STRUCTURE
// =============================================================================
struct AdviceOutput {
    String text = "Загрузка...";
    int code = 0;  // 0=Normal, 1=Vent/Yellow, 2=Critical/Red, 3=Optimal/Green
};

// =============================================================================
// PURE FUNCTION ENGINE
// =============================================================================
class AdviceEngine {
public:
    /**
     * @brief Generate human-readable advice based on climate conditions.
     * 
     * This is a PURE FUNCTION - no side effects, no state.
     * Given the same input, always returns the same output.
     * 
     * @param in Climate data input
     * @return AdviceOutput containing text and color code
     */
    static AdviceOutput compute(const AdviceInput& in);
};
