#ifndef DHTMANAGER_H
#define DHTMANAGER_H

#include <DHT.h>

class DHTManager {
public:
    DHTManager(uint8_t pin, uint8_t type);
    void initialize();
    float readTemperature();
    float readHumidity();

private:
    DHT dht;
};

#endif // DHTMANAGER_H
