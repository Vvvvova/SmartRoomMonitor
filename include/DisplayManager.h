#pragma once
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Settings.h"

class DisplayManager {
public:
    DisplayManager();
    void begin();
    void update();  // Reads from g_state directly

private:
    Adafruit_SSD1306 display;
    bool isNightMode();
};
