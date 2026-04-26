#ifndef V8_THINGSPEAK_MANAGER_H
#define V8_THINGSPEAK_MANAGER_H

#include <Arduino.h>
#include "ConfigManager.h"

class ThingSpeakManager {
public:
  bool begin(const V8Config &config);
  bool isReady() const;
  bool update(float temperature, float humidity);
  String lastError() const;

private:
  V8Config config{};
  bool ready = false;
  String error;
};

#endif
