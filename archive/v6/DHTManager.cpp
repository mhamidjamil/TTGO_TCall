#include "DHTManager.h"

DHTManager::DHTManager(uint8_t pin, uint8_t type) : dht(pin, type) {}

void DHTManager::initialize() {
    dht.begin();
}

float DHTManager::readTemperature() {
    return dht.readTemperature();
}

float DHTManager::readHumidity() {
    return dht.readHumidity();
}
