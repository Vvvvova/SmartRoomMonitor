#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "Settings.h"
#include "CoreState.h"

/**
 * @class NetworkManager
 * @brief Handles WiFi connection, Reconnection, and NTP Time Sync.
 * 
 * Extracts "infrastructure" logic out of the main loop.
 */
class NetworkManager {
public:
    NetworkManager();

    // Init & Connect
    void begin();
    
    // Periodic Maintenance (Reconnection)
    void update();

    // Helper: Get Reset Reason String
    String getResetReason();

    // Helpers
    bool isConnected() const;
    String getLocalIP() const;

private:
    void connectWiFi();
    void syncTime();
    
    unsigned long lastConnCheck;
    bool initialSyncDone;
    unsigned long lastTimeSyncAttempt;

};
