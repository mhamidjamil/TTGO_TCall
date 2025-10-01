#ifndef SMS_MANAGER_H
#define SMS_MANAGER_H

#include <Arduino.h>
#include "ConfigManager.h"

class SMSManager {
public:
  SMSManager(ConfigManager &cfgMgr);
  void begin();
  void loop();
  // send SMS through modem - simple interface
  bool sendSms(const String &to, const String &message, String &err);

private:
  ConfigManager &cfgMgr;
  void handleIncomingSms(const String &from, const String &body);
  void forwardEvent(const String &type, const String &number, const String &body);
};

#endif
