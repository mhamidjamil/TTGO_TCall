#include "SMSManager.h"

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
  while (Serial1.available()) {
    Serial1.read();
  }
  Serial1.println(command);
  return waitForResponse(expected, timeoutMs);
}
}

bool SMSManager::begin() {
  return sendAtCommand("AT", "OK", 1500) &&
         sendAtCommand("AT+CMGF=1", "OK", 1500) &&
         sendAtCommand("AT+CNMI=2,2,0,0,0", "OK", 1500);
}

bool SMSManager::sendMessage(const String &number, const String &message) {
  if (!sendAtCommand("AT+CMGF=1", "OK", 1500)) {
    return false;
  }

  while (Serial1.available()) {
    Serial1.read();
  }

  Serial1.print("AT+CMGS=\"");
  Serial1.print(number);
  Serial1.println("\"");
  if (!waitForResponse(">", 5000)) {
    return false;
  }

  Serial1.print(message);
  Serial1.write((char)26);
  return waitForResponse("OK", 15000);
}
