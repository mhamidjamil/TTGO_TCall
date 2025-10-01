#include "SMSManager.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

SMSManager::SMSManager(ConfigManager &cfgMgr) : cfgMgr(cfgMgr) {}

void SMSManager::begin() {
  // Serial1 should already be configured in main sketch with modem TX/RX pins and baud
  Serial.println("SMSManager initialized");
}

void SMSManager::loop() {
  // Could parse incoming modem responses if required
}

bool SMSManager::sendSms(const String &to, const String &message, String &err) {
  auto cfg = cfgMgr.get();
  if (!cfg.allowSms) { err = "Sending SMS disabled"; return false; }

  Serial.printf("[SMSManager] Sending SMS to %s: %s\n", to.c_str(), message.c_str());

  // Set text mode
  Serial1.println("AT+CMGF=1");
  delay(200);

  // Start send
  Serial1.print("AT+CMGS=\"");
  Serial1.print(to);
  Serial1.println("\"");
  delay(200);

  Serial1.print(message);
  Serial1.write((char)26); // Ctrl+Z

  // read response
  unsigned long start = millis();
  String resp;
  while (millis() - start < 15000) {
    while (Serial1.available()) resp += (char)Serial1.read();
    if (resp.indexOf("OK") >= 0) break;
    if (resp.indexOf("ERROR") >= 0) break;
    delay(100);
  }

  if (resp.indexOf("OK") >= 0) {
    forwardEvent("sent_sms", to, message);
    return true;
  }
  err = resp;
  return false;
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
