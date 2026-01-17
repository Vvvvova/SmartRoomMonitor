#ifndef WEATHER_MANAGER_H
#define WEATHER_MANAGER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

class WeatherManager {
public:
    WeatherManager();
    void update(); // Non-blocking update check (every 30 mins)
    
    float getOutdoorTemp() const;
    float getOutdoorHum() const;
    float getOutdoorAbsHum() const; // g/m3
    String getConditionString() const; // e.g. "Rainy", "Clear"
    String getStatusString() const; // Debug Info (Error or "OK")
    bool isDataValid() const;

private:
    float outTemp;
    float outHum;
    unsigned long lastUpdate;
    bool valid;
    String lastError;

    void fetchWeather();
};

#endif
