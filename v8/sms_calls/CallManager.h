#ifndef V8_CALL_MANAGER_H
#define V8_CALL_MANAGER_H

#include <Arduino.h>

class CallManager {
public:
  bool begin();
  void loop();
  bool hangUp();
  bool placeMissedCall(const String &number, unsigned long ringMs, bool &userPicked, int &durationSeconds);
};

#endif
