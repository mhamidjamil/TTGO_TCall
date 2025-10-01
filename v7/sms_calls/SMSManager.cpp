#include "SMSManager.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

SMSManager::SMSManager(ConfigManager &cfgMgr) : cfgMgr(cfgMgr) {}

void SMSManager::begin() {
  // Setup modem serial if needed - placeholder
  Serial.println("SMSManager initialized");
}

void SMSManager::loop() {
  // Placeholder: in a real project, read from modem serial for incoming SMS and call handleIncomingSms
}

bool SMSManager::sendSms(const String &to, const String &message, String &err) {
  auto cfg = cfgMgr.get();
  if (!cfg.allowSms) { err = "Sending SMS disabled"; return false; }
  // Here send AT commands to modem. For now we simulate success.
  Serial.println("Pretend sending SMS to " + to + ": " + message);
  // After sending, optionally forward event
  forwardEvent("sent_sms", to, message);
  return true;
}

void SMSManager::handleIncomingSms(const String &from, const String &body) {
  Serial.println("Incoming SMS from " + from + ": " + body);
  forwardEvent("incoming_sms", from, body);
}

void SMSManager::forwardEvent(const String &type, const String &number, const String &body) {
  Config c = cfgMgr.get();
  if (c.forwardUrl.length() == 0) return;
  if (c.useApiSecret && c.apiSecret.length() == 0) return;

  HTTPClient http;
  http.begin(c.forwardUrl);
  http.addHeader("Content-Type", "application/json");
  if (c.forwardApiKey.length()) http.addHeader("X-Api-Key", c.forwardApiKey);

  StaticJsonDocument<256> doc;
  doc["type"] = type;
  doc["number"] = number;
  doc["body"] = body;
  if (c.useApiSecret) doc["secret"] = c.apiSecret;

  String b;
  serializeJson(doc, b);
  int code = http.POST(b);
  if (code > 0) {
    Serial.println("Forwarded event, response: " + http.getString());
  } else {
    Serial.println("Failed to forward event: " + String(code));
  }
  http.end();
}
