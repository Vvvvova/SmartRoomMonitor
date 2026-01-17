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

}
