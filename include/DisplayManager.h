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
    void update(float t, float h, float dp, bool win, String advice, int adviceCode, int rawState, String ip);

private:
    Adafruit_SSD1306 display;
    bool isNightMode();
};
