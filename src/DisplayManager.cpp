#include "DisplayManager.h"
#include <WiFi.h>

DisplayManager::DisplayManager() 
    : display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET) {}

void DisplayManager::begin() {
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setTimeOut(200); // Prevent infinite hang if display disconnects
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
    }
    display.clearDisplay();
    display.display();
}

bool DisplayManager::isNightMode() {
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)) return false; 
    int hr = timeinfo.tm_hour;
    if (hr >= NIGHT_MODE_START_HOUR || hr < NIGHT_MODE_END_HOUR) return true;
    return false;
}

void DisplayManager::update(float t, float h, float dp, bool win, String advice, int adviceCode, int rawState, String ip) {
    if(isNightMode()) {
        display.clearDisplay();
        display.display();
        return;
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    
    // Header
    display.setTextSize(1);
    display.setCursor(0,0);
    display.print(F("ACM-1 "));
    
    // Advice Code Logic
    // 0=Safe/Gray, 1=Vent/Yellow, 2=Crit/Red, 3=Safe/Green(Close/Dry)
    String lcdAdvice = "SAFE";
    switch(adviceCode) {
        case 2: lcdAdvice = "CRITICAL"; break;
        case 1: lcdAdvice = "VENTILATE"; break;
        case 3: lcdAdvice = "OPTIMAL"; break; // Close/Dry
        case 0: lcdAdvice = "NORMAL"; break;
    }
    
    // Overrides based on internal State Machine (rawState)
    // 0=STABLE, 1=VENTILATING, 2=PLATEAU
    if (rawState == 1) lcdAdvice = "DRYING...";
    if (rawState == 2) lcdAdvice = "PLATEAU";

    display.print(lcdAdvice);
    
    // Main Stats
    display.setTextSize(2);
    display.setCursor(0, 16);
    if(isnan(t)) display.print(F("--.-"));
    else display.print(t, 1);
    display.print(F("C"));

    display.setCursor(70, 16);
    if(isnan(h)) display.print(F("--"));
    else display.print(h, 0);
    display.print(F("%"));

    // Footer Info
    display.setTextSize(1);
    display.setCursor(0, 44);
    if(win) {
         display.print(F("[WIN OPEN] "));
    }
    display.print(F("DP:"));
    if(!isnan(dp)) display.print(dp, 1);
    
    display.setCursor(0, 56);
    display.print(ip);

    display.display();
}
