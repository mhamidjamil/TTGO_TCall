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

  // list all SMS messages stored on the modem as a JSON array string
  String listAllMessages();
  // delete all SMS messages from modem storage
  bool deleteAllMessages();

  // Counters for display / telemetry
  int getMessagesSent();
  int getMessagesReceived();

private:
  // provide accessors to the static counters in implementation
  friend int get_sms_sent_count();
  friend int get_sms_received_count();

// (SMSManager uses Serial1 to communicate with the modem)

private:
  ConfigManager &cfgMgr;
  unsigned long lastPingTime;
  void handleIncomingSms(const String &from, const String &body);
  void forwardEvent(const String &type, const String &number, const String &body);
  // Read SMS from modem storage by index and forward; delete on success
  void readAndForwardSms(int index);
  // Forward event and return true if server returned HTTP 200
  bool forwardEventWithResult(const String &type, const String &number, const String &body);
  // Read response from Serial1 with timeout
  String readResponseFromSerial1(unsigned long timeoutMs);
  // Ping the bridge server to validate connectivity
  void pingBridge();
};

#endif
