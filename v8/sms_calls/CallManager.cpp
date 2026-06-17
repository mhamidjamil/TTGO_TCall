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

bool CallManager::placeMissedCall(const String &number, unsigned long ringMs, bool &userPicked, int &durationSeconds) {
  userPicked = false;
  durationSeconds = 0;

  while (Serial1.available()) {
    Serial1.read();
  }

  Serial1.print("ATD");
  Serial1.print(number);
  Serial1.println(";");

  String buffer;
  unsigned long started = millis();
  bool dialAccepted = false;
  while (millis() - started < ringMs) {
    while (Serial1.available()) {
      char c = (char)Serial1.read();
      buffer += c;
      if (buffer.indexOf("OK") != -1) {
        dialAccepted = true;
      }
      if (buffer.indexOf("CONNECT") != -1) {
        userPicked = true;
        durationSeconds = (int)((millis() - started) / 1000UL);
        hangUp();
        return true;
      }
      if (buffer.indexOf("NO CARRIER") != -1 || buffer.indexOf("BUSY") != -1 ||
          buffer.indexOf("NO ANSWER") != -1 || buffer.indexOf("ERROR") != -1) {
        durationSeconds = (int)((millis() - started) / 1000UL);
        hangUp();
        return dialAccepted;
      }
    }
    delay(20);
  }

  durationSeconds = (int)(ringMs / 1000UL);
  hangUp();
  return dialAccepted || buffer.length() > 0;
}
