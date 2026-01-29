#include "NetworkManager.h"

NetworkManager::NetworkManager() 
    : lastConnCheck(0), initialSyncDone(false) {}

void NetworkManager::begin() {
    // 1. WiFi Init
    g_state.lock();
    g_state.advice = "CONNECTING...";
    g_state.ipAddress = "Init WiFi";
    g_state.unlock();
    
    // Zero Flash Writes: Disable NVS persistence
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    connectWiFi();

    // 2. Time Sync (Blocking on first boot for valid timestamps)
    if (isConnected()) {
        syncTime();
    } else {
        g_state.lock();
        g_state.advice = "WIFI FAIL";
        g_state.ipAddress = "Offline Mode";
        g_state.unlock();
    }
}

void NetworkManager::update() {
    unsigned long now = millis();
    
    // Check every 30s
    if (now - lastConnCheck > 30000) {
        lastConnCheck = now;
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[NET] Reconnecting...");
            WiFi.reconnect();
        } else {
            // Update IP in state just in case
            String ip = WiFi.localIP().toString();
            g_state.lock();
            if (g_state.ipAddress != ip) {
                g_state.ipAddress = ip;
            }
            g_state.unlock();
        }
    }
}

void NetworkManager::connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
        delay(500);
        retries++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[NET] WiFi Connected");
    } else {
        Serial.println("[NET] WiFi Failed");
    }
}

void NetworkManager::syncTime() {
    g_state.lock();
    g_state.advice = "SYNC TIME...";
    g_state.ipAddress = WiFi.localIP().toString();
    g_state.unlock();

    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    
    struct tm timeinfo;
    int retry = 0;
    while (!getLocalTime(&timeinfo) && retry < 20) {
        delay(500);
        retry++;
    }
    
    if (retry < 20) {
        initialSyncDone = true;
        Serial.println("[NET] Time Synced");
    } else {
        Serial.println("[NET] Time Sync Failed");
    }
}

bool NetworkManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

String NetworkManager::getLocalIP() const {
    return WiFi.localIP().toString();
}

String NetworkManager::getResetReason() {
  esp_reset_reason_t reason = esp_reset_reason();
  switch (reason) {
  case ESP_RST_POWERON: return "Power On";
  case ESP_RST_SW:      return "Software Reset";
  case ESP_RST_PANIC:   return "Crash/Panic (Watchdog?)";
  case ESP_RST_INT_WDT: return "Interrupt Watchdog";
  case ESP_RST_TASK_WDT:return "Task Watchdog";
  case ESP_RST_WDT:     return "Other Watchdog";
  case ESP_RST_DEEPSLEEP:return "Deep Sleep Wake";
  case ESP_RST_BROWNOUT:return "Brownout (Low Voltage)";
  case ESP_RST_SDIO:    return "SDIO Reset";
  default:              return "Unknown (" + String(reason) + ")";
  }
}
