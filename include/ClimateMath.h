#pragma once
#include <Arduino.h>

// Shared Math Functions for Climate Analysis
namespace ClimateMath {

    inline float calculateAbsHumidity(float t, float h) {
        // Approx formula for Absolute Humidity in g/m3
        if(isnan(t) || isnan(h)) return NAN;
        
        float exponent = (17.67f * t) / (t + 243.5f);
        float saturationPressure = 6.112f * exp(exponent);
        // 2.1674 is a constant derived from the Molecular weight of water vapor and Gas constant
        float absoluteHumidity = (saturationPressure * h * 2.1674f) / (273.15f + t);
        
        return absoluteHumidity;
    }

    inline float calculateDewPoint(float t, float h) {
        if (isnan(t) || isnan(h)) return NAN;
        float a = 17.27f;
        float b = 237.7f;
        float alpha = ((a * t) / (b + t)) + log(h / 100.0f);
        return (b * alpha) / (a - alpha);
    }

    // -------------------------------------------------------------------------
    // Signal Processing (Filters & Validators)
    // -------------------------------------------------------------------------

    /**
     * @brief Apply Exponential Moving Average (Low Pass Filter)
     * @param current The new raw reading
     * @param previous The previous filtered value
     * @param factor Smoothing factor (0.0 to 1.0). Lower = smoother but slower.
     *               Typically 0.2 for sensor data.
     */
    inline float lowPassFilter(float current, float previous, float factor = 0.2f) {
        if (isnan(current)) return previous;
        if (isnan(previous)) return current; // First reading
        return (previous * (1.0f - factor)) + (current * factor);
    }

    /**
     * @brief Check if a value jump is valid (Physical Sanity Check)
     * @param current New reading
     * @param previous Last valid reading
     * @param maxJump Maximum allowed change per cycle
     * @return true if valid, false if outlier/glitch
     */
    inline bool isJumpValid(float current, float previous, float maxJump) {
        if (isnan(current)) return false;
        if (isnan(previous)) return true; // Always accept first
        return abs(current - previous) <= maxJump;
    }

    /**
     * @brief Clamp humidity to physically possible values (0-100%)
     */
    inline float constrainHumidity(float h) {
        if (isnan(h)) return NAN;
        if (h < 0.0f) return 0.0f;
        if (h > 100.0f) return 100.0f;
        return h;
    }

}
