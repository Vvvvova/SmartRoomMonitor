#pragma once
#include <Arduino.h>
#include <DHT.h>

/**
 * @brief Hardware Abstraction Layer for Environmental Sensors
 * 
 * Responsibilities:
 * - Initialize hardware (DHT22/11/etc)
 * - Read raw values
 * - Handle hardware-specific quirks (e.g. 2s delay)
 * - Return raw data structure (no logic, no filtering)
 */
struct RawSensorData {
    float temp;
    float hum;
    bool success;
};

class SensorDriver {
private:
    DHT dht;
    uint8_t pin;
    uint8_t type;

public:
    // Defaults matching your current project (PIN 4, DHT22)
    SensorDriver(uint8_t pin = 4, uint8_t type = DHT22);

    void begin();
    
    /**
     * @brief Read data from physical sensor
     * Blocking call (DHT takes ~250ms typically)
     * @return RawSensorData struct containing t, h, and success flag
     */
    RawSensorData read();
};
