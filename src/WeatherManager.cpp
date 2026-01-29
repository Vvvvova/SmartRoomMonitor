#include "WeatherManager.h"
#include "Settings.h"
#include "ClimateMath.h"
#include "CoreState.h"

const unsigned long UPDATE_INTERVAL = 10 * 60 * 1000; // 10 mins

WeatherManager::WeatherManager() : outTemp(NAN), outHum(NAN), lastUpdate(0), valid(false) {}

void WeatherManager::update() {
    if (WiFi.status() != WL_CONNECTED) return;

    // Retry sooner if not valid (e.g. 1 min instead of 10)
    unsigned long interval = valid ? UPDATE_INTERVAL : 60000; 

    if (millis() - lastUpdate > interval || lastUpdate == 0) {
        fetchWeather();
        // Only update timer if successful or if we want to enforce the wait even on fail
        // But here we want to retry. 
        // Let's set lastUpdate. If it fails, valid is false, so next time interval is 60s.
        lastUpdate = millis();
    }
}

// Getter for debug
String WeatherManager::getStatusString() const {
    if(valid) return "OK (Updated)";
    if(lastError.length() > 0) return lastError;
    return "Waiting...";
}

void WeatherManager::fetchWeather() {
    HTTPClient http;
    http.begin(WEATHER_API_URL);
    http.setTimeout(2000); // 2s timeout to prevent loop freeze
    
    int httpCode = http.GET();
    if (httpCode == 200) {
        String payload = http.getString();
        
        StaticJsonDocument<96> filter;
        filter["current"]["temperature_2m"] = true;
        filter["current"]["relative_humidity_2m"] = true;

        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

        if (!error) {
            outTemp = doc["current"]["temperature_2m"];
            outHum = doc["current"]["relative_humidity_2m"];
            valid = true;
            lastError = ""; 
            Serial.printf("Weather Updated: %.1fC, %.1f%%\n", outTemp, outHum);
        } else {
            lastError = "JSON Err";
            Serial.println(error.c_str());
        }
    } else {
        valid = false; 
        if(httpCode > 0) lastError = "HTTP " + String(httpCode);
        else lastError = "Conn Err";
        Serial.printf("Weather Error: %s\n", lastError.c_str());
    }
    http.end();

    // PUBLISH TO GLOBAL STATE
    g_state.lock();
    g_state.outTemp = outTemp;
    g_state.outHum = outHum;
    g_state.outAbsHum = getOutdoorAbsHum();
    g_state.weatherValid = valid;
    if (valid) {
        g_state.weatherStatus = "OK";
    } else {
        g_state.weatherStatus = lastError.length() > 0 ? lastError : "FAIL";
    }
    g_state.unlock();
}

float WeatherManager::getOutdoorTemp() const { return outTemp; }
float WeatherManager::getOutdoorHum() const { return outHum; }
float WeatherManager::getOutdoorAbsHum() const { return ClimateMath::calculateAbsHumidity(outTemp, outHum); }
bool WeatherManager::isDataValid() const { return valid; }

String WeatherManager::getConditionString() const {
    if(!valid) return "Нет данных";
    if(outTemp < 0) return "Мороз";
    if(outHum > 85) return "Влажно (Улица)";
    return "Норма (Улица)";
}
