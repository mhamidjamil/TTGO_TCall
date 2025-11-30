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

  int getCallsMade();
  int getCallsReceived();

  // Allow external modules (e.g., SMS manager) to notify of an incoming call
  void handleIncomingCall(const String &from);

private:
  ConfigManager &cfgMgr;
  void forwardEvent(const String &type, const String &number);
};

#endif
