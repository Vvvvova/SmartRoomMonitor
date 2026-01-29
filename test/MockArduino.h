#pragma once

#ifdef UNIT_TEST

#include <string>
#include <iostream>
#include <cmath>
#include <vector>
#include <stdarg.h>
#include <stdio.h>
#include <cstdint>
// Remove conflicting time_t typedef - use system one
// timestamp is usually uint32_t in logic, but standard time_t is int64 on 64bit.
// Code casts to (uint32_t) for storage in Record correctly.

typedef uint32_t TickType_t;
#define pdMS_TO_TICKS(x) (x)
#define pdTRUE 1
// Remove NULL define (system has it)

// Mock millis
extern unsigned long _mock_millis;
inline unsigned long millis() { return _mock_millis; }
inline void delay(unsigned long ms) { _mock_millis += ms; }

// Mock String
class String {
public:
    std::string s;
    String() : s("") {}
    String(const char* str) : s(str) {}
    String(std::string str) : s(str) {}
    String(int val) : s(std::to_string(val)) {}
    String(float val, int prec=1) { 
        char buf[32]; 
        sprintf(buf, "%.*f", prec, val); 
        s = buf; 
    }
    const char* c_str() const { return s.c_str(); }
    bool operator==(const String& other) const { return s == other.s; }
    bool operator==(const char* other) const { return s == other; }
    String& operator=(const char* other) { s = other; return *this; }
};

// Mock Serial
class MockSerial {
public:
    void println(const char* s) { printf("%s\n", s); }
    void println(String s) { printf("%s\n", s.c_str()); }
    void print(const char* s) { printf("%s", s); }
    void print(String s) { printf("%s", s.c_str()); }
    void printf(const char* format, ...) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
};
extern MockSerial Serial;

// Mock DHT
// Undef first to avoid redefinition warnings if Settings.h included later or earlier
#undef DHTPIN
#define DHTPIN 4
#undef DHTTYPE
#define DHTTYPE 11   
#undef DHT22
#define DHT22 22 

class DHT {
public:
    DHT(uint8_t pin, uint8_t type) {}
    void begin() {}
    float readTemperature() { return _temp; }
    float readHumidity() { return _hum; }
    
    // Test control
    static float _temp;
    static float _hum;
};

// Mock FreeRTOS
typedef int SemaphoreHandle_t;
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return 1; }
inline int xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t block_time) { return 1; }
inline void xSemaphoreGive(SemaphoreHandle_t mutex) {}
inline void vTaskDelayUntil(TickType_t *pxPreviousWakeTime, TickType_t xTimeIncrement) { _mock_millis += xTimeIncrement; }
inline TickType_t xTaskGetTickCount() { return _mock_millis; }
inline void xTaskCreatePinnedToCore(void (*task)(void*), const char* name, uint32_t stack, void* param, int prio, void* handle, int core) {
    // No-op for mock
}

// Helper math
inline float constrain(float input, float low, float high) {
    if (input < low) return low;
    if (input > high) return high;
    return input;
}
inline float max(float a, float b) { return (a > b) ? a : b; }

// Use standard abs/isnan
using std::abs;
using std::isnan;

#undef NAN
#define NAN std::nanf("")

// Snprintf wrapper
inline int snprintf(char * s, size_t n, const char * format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(s, n, format, args);
    va_end(args);
    return ret;
}

// Mock ClimateMath
class ClimateMath {
public:
    static float calculateAbsHumidity(float t, float h) {
        if(std::isnan(t) || std::isnan(h)) return NAN;
        return (t * 0.2f) + (h * 0.1f); 
    }
    static float calculateDewPoint(float t, float h) {
        return t - ((100.0f - h)/5.0f);
    }
};

// Mock WeatherManager
class WeatherManager {
public:
    bool isDataValid() { return false; }
    float getOutdoorTemp() { return 20.0; }
    float getOutdoorAbsHum() { return 10.0; } 
    float getOutdoorHum() { return 50.0; }
    String getStatusString() { return "Mock"; }
};

#endif
