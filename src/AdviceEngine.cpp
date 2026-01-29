#include "AdviceEngine.h"
#include "ClimateMath.h"

AdviceOutput AdviceEngine::compute(const AdviceInput& in) {
    AdviceOutput out;
    out.text = "Анализ...";
    out.code = 0;

    if (isnan(in.hum)) {
        return out;
    }

    // --- PRIORITY 1: ACTIVE STATE OVERRIDES ---
    
    if (in.state == ClimateState::INEFFICIENT) {
        // Plateau detected
        unsigned long durMs = in.now - in.stateEnterTime;
        int mins = (durMs + 59999) / 60000; // Round UP
        char buf[64];
        snprintf(buf, sizeof(buf), "Эффект упал (%d мин). Закрывай", mins);
        out.text = buf;
        out.code = 2; // Red - Stop wasting heat
        return out;
    }
    
    if (in.state == ClimateState::TARGET_MET) {
        // Success
        unsigned long durMs = in.now - in.stateEnterTime;
        int mins = (durMs + 59999) / 60000;
        char buf[64];
        snprintf(buf, sizeof(buf), "Готово (за %d мин) Закрывай", mins);
        out.text = buf;
        out.code = 3; // Green - Win
        return out;
    }
    
    if (in.state == ClimateState::VENTILATING) {
        // Active drying
        unsigned long durMs = in.now - in.stateEnterTime;
        int mins = (durMs + 59999) / 60000;
        char buf[64];
        snprintf(buf, sizeof(buf), "Сушка (%d мин) %s", mins, in.dryingInd.c_str());
        out.text = buf;
        out.code = 1; // Yellow - Action in progress
        return out;
    }

    // --- PRIORITY 2: PASSIVE MONITORING (STABLE STATE) ---
    
    // Default fallback outdoor temp if invalid
    float outT = in.weatherValid ? in.outTemp : 20.0f;

    // A. Winter Mode (< 10°C)
    if (outT < 10.0f) {
        float margin = in.temp - in.dewPoint;
        if (margin < 3.0f) {
            out.text = "КРИТИЧНО! ГРЕТЬ/ОСУШАТЬ";
            out.code = 2;
        } else if (in.hum > 55.0f) {
            out.text = "Влажно [ЗАЛП 5 мин]";
            out.code = 1;
        } else {
            out.text = "Зимняя Норма";
            out.code = 3;
        }
    }
    // B. Summer Mode (> 18°C)
    else if (outT > 18.0f) {
        if (in.weatherValid) {
            // Smart Comparison (Absolute Humidity)
            // Indoor Abs vs Outdoor Abs
            if (in.outAbsHum > in.absHum) {
                out.text = "Влажно [НЕ ОТКРЫВАТЬ!]";
                out.code = 3; // Stay closed (Blue/Green)
            } 
            else if (in.hum > 60.0f) {
                out.text = "Влажно [Проветрить]";
                out.code = 1;
            } 
            else {
                out.text = "Летняя Норма";
                out.code = 3;
            }
        } else {
            // Simplified fallback
            if (in.hum > 60.0f) {
                out.text = "Влажно [Проветрить]";
                out.code = 1;
            } else {
                out.text = "Летняя Норма";
                out.code = 3;
            }
        }
    }
    // C. Transition Mode (10-18°C)
    else {
        if ((in.temp - in.dewPoint) < 2.5f) {
            out.text = "КРИТИЧНО! Открыть окно";
            out.code = 2;
        } else if (in.hum > 60.0f) {
            out.text = "Влажно [Реком. проветрить]";
            out.code = 1;
        } else if (in.hum < 35.0f) {
            out.text = "Сухой воздух [Увлажнить]";
            out.code = 3;
        } else {
            out.text = "Норма (Стены сохнут)";
            out.code = 3;
        }
    }

    return out;
}
