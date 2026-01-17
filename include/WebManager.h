#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "SensorManager.h"

class WebManager {
public:
    WebManager(SensorManager* sm);
    void begin();

private:
    AsyncWebServer server;
    SensorManager* sensorManager;
};
