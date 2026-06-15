#include "CallManager.h"

#include <Arduino.h>

namespace {
bool waitForResponse(const String &expected, unsigned long timeoutMs) {
  String buffer;
  unsigned long started = millis();
  while (millis() - started < timeoutMs) {
    while (Serial1.available()) {
      char c = (char)Serial1.read();
      buffer += c;
      if (buffer.indexOf(expected) != -1) {
        return true;
      }
      if (buffer.indexOf("ERROR") != -1) {
        return false;
      }
    }
    delay(10);
  }
  return false;
}

bool sendAtCommand(const String &command, const String &expected, unsigned long timeoutMs) {
  Serial1.println(command);
  return waitForResponse(expected, timeoutMs);
}
}

bool CallManager::begin() {
  return sendAtCommand("AT+CLIP=1", "OK", 1500);
}

void CallManager::loop() {
}

bool CallManager::hangUp() {
  Serial1.println("AT+CHUP");
  return waitForResponse("OK", 2000);
}
