#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

class WebManager {
public:
    WebManager();
    void begin();

private:
    AsyncWebServer server;
};
