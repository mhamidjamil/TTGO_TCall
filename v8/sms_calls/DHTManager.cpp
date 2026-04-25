#include "DHTManager.h"

#if __has_include("secrets.h")
#include "secrets.h"
#elif __has_include("secrets.example.h")
#include "secrets.example.h"
#endif

#include <DHT.h>

namespace {
#if DHT_TYPE_DEFAULT == 22
DHT dht(DHT_PIN_DEFAULT, DHT22);
#else
DHT dht(DHT_PIN_DEFAULT, DHT11);
#endif
}

bool DHTManager::begin() {
  dht.begin();
  return true;
}

float DHTManager::readTemperature() {
  float value = dht.readTemperature();
  if (isnan(value)) {
    return -1000.0f;
  }
  return value;
}

float DHTManager::readHumidity() {
  float value = dht.readHumidity();
  if (isnan(value)) {
    return -1.0f;
  }
  return value;
}
