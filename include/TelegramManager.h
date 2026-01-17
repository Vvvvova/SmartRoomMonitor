#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <vector>
#include "Settings.h"
#include "SensorManager.h"

// Struct for Subscriber
struct Subscriber {
    String chatId;
    bool isMuted;
    String name;
    unsigned long lastAlertTime;
};

class TelegramManager {
public:
    TelegramManager(SensorManager* sm);
    void begin();
    void update(); // Main loop handler
    
    void broadcastAlert(const String& msg, int level); // level: 1=Info/Green, 2=Warn/Red
    
private:
    SensorManager* sensorManager;
    WiFiClientSecure client;
    UniversalTelegramBot* bot;
    
    unsigned long lastPollTime;
    int lastAdviceCode; // To track changes
    SensorManager::ClimateState lastClimateState;
    bool moldAlertSent;
    bool timeoutAlertSent;
    
    std::vector<Subscriber> subscribers;
    
    void handleNewMessages(int numNewMessages);
    void sendMainMenu(const String& chatId, const String& welcomeMsg = "");
    void sendStatus(const String& chatId);
    void subscribe(const String& chatId, const String& firstName);
    void toggleMute(const String& chatId);
    bool isAuthorized(const String& chatId); // Simple check if needed
};
