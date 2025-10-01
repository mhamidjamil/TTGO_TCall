#ifndef CALL_MANAGER_H
#define CALL_MANAGER_H

#include <Arduino.h>
#include "ConfigManager.h"

class CallManager {
public:
  CallManager(ConfigManager &cfgMgr);
  void begin();
  void loop();
  void hangup();

private:
  ConfigManager &cfgMgr;
  void handleIncomingCall(const String &from);
  void forwardEvent(const String &type, const String &number);
};

#endif
