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

// (SMSManager uses Serial1 to communicate with the modem)

private:
  ConfigManager &cfgMgr;
  void handleIncomingSms(const String &from, const String &body);
  void forwardEvent(const String &type, const String &number, const String &body);
  // Read SMS from modem storage by index and forward; delete on success
  void readAndForwardSms(int index);
  // Forward event and return true if server returned HTTP 200
  bool forwardEventWithResult(const String &type, const String &number, const String &body);
};

#endif
