#include "DisplayManager.h"

bool DisplayManager::begin() {
  return true;
}

void DisplayManager::update(float temperature, float humidity, const char *networkMode, const char *cloudMode) {
  (void)temperature;
  (void)humidity;
  (void)networkMode;
  (void)cloudMode;
}
