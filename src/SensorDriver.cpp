#include "SensorDriver.h"

SensorDriver::SensorDriver(uint8_t pin, uint8_t type) 
    : dht(pin, type), pin(pin), type(type) {
}

void SensorDriver::begin() {
    dht.begin();
}

RawSensorData SensorDriver::read() {
    RawSensorData data;
    data.success = false;
    data.temp = NAN;
    data.hum = NAN;

    // Read from DHT
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    // Check for valid readings
    if (!isnan(t) && !isnan(h)) {
        data.temp = t;
        data.hum = h;
        data.success = true;
    }

    return data;
}
