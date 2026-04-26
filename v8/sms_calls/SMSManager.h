#ifndef V8_SMS_MANAGER_H
#define V8_SMS_MANAGER_H

#include <Arduino.h>

class SMSManager {
public:
  bool begin();
  bool sendMessage(const String &number, const String &message);
};

#endif
