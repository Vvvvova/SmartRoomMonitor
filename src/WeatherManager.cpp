#include "WeatherManager.h"
#include "Settings.h"
#include "ClimateMath.h"

const unsigned long UPDATE_INTERVAL = 10 * 60 * 1000; // 10 mins

WeatherManager::WeatherManager() : outTemp(NAN), outHum(NAN), lastUpdate(0), valid(false) {}

void WeatherManager::update() {
    if (WiFi.status() != WL_CONNECTED) return;

    if (millis() - lastUpdate > UPDATE_INTERVAL || lastUpdate == 0) {
        fetchWeather();
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
            lastError = ""; // Clear error
            Serial.printf("Weather Updated: %.1fC, %.1f%%\n", outTemp, outHum);
        } else {
            lastError = "JSON Error: " + String(error.c_str());
            Serial.println(lastError);
        }
    } else {
        valid = false; // Invalidate data on fetch fail? optional, but safer
        if(httpCode > 0) lastError = "HTTP " + String(httpCode);
        else lastError = "Timeout/Conn Error";
        Serial.printf("Weather Error: %s\n", lastError.c_str());
    }
    http.end();
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
