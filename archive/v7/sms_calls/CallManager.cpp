#include "CallManager.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

CallManager::CallManager(ConfigManager &cfgMgr) : cfgMgr(cfgMgr) {}

void CallManager::begin() { Serial.println("CallManager initialized"); }

// counters
static int _callsMade = 0;
static int _callsReceived = 0;

int CallManager::getCallsMade() { return _callsMade; }
int CallManager::getCallsReceived() { return _callsReceived; }

void CallManager::loop() {
  // Placeholder: Call handling is triggered by SMSManager when +CLIP URC is detected
}

void CallManager::hangup() {
  Serial.println("Hangup: sending AT+CHUP");
  // ensure we have Serial1 configured by main sketch
  Serial1.println("AT+CHUP");
  // small delay to allow the modem to process
  delay(200);
  // optionally read response
  String r=""; unsigned long s=millis();
  while (millis()-s < 1000) { while (Serial1.available()) r += (char)Serial1.read(); if (r.length()) break; delay(10); }
  Serial.println("Hangup response: " + r);
  _callsMade++;
}

void CallManager::handleIncomingCall(const String &from) {
  Serial.println("Incoming call from " + from);
  hangup();
  _callsReceived++;
  forwardEvent("incoming_call", from);
}

void CallManager::forwardEvent(const String &type, const String &number) {
  Config c = cfgMgr.get();
  if (c.forwardUrl.length() == 0) return;
  HTTPClient http;
  http.begin(c.forwardUrl);
  http.addHeader("Content-Type", "application/json");
  if (c.forwardApiKey.length()) http.addHeader("X-Api-Key", c.forwardApiKey);
  StaticJsonDocument<192> doc;
  doc["type"] = type;
  doc["number"] = number;
  if (c.useApiSecret) doc["secret"] = c.apiSecret;
  String b; serializeJson(doc,b);
  int code = http.POST(b);
  if (code>0) Serial.println("Call forwarded: " + http.getString());
  http.end();
}
