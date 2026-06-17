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

String readAtResponse(unsigned long timeoutMs) {
  String buffer;
  unsigned long started = millis();
  while (millis() - started < timeoutMs) {
    while (Serial1.available()) {
      buffer += (char)Serial1.read();
      if (buffer.indexOf("\r\nOK\r\n") != -1 || buffer.indexOf("\nOK\r\n") != -1 ||
          buffer.indexOf("\r\nERROR\r\n") != -1 || buffer.indexOf("\nERROR\r\n") != -1) {
        return buffer;
      }
    }
    delay(10);
  }
  return buffer;
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
         sendAtCommand("AT+CNMI=2,1,0,0,0", "OK", 1500);
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

String SMSManager::listMessages() {
  while (Serial1.available()) {
    Serial1.read();
  }
  Serial1.println("AT+CMGL=\"ALL\"");
  return readAtResponse(8000);
}

String SMSManager::readMessage(int index) {
  while (Serial1.available()) {
    Serial1.read();
  }
  Serial1.print("AT+CMGR=");
  Serial1.println(index);
  return readAtResponse(5000);
}

bool SMSManager::deleteMessage(int index) {
  if (index <= 0) {
    return false;
  }
  return sendAtCommand(String("AT+CMGD=") + String(index), "OK", 5000);
}

bool SMSManager::deleteAllMessages() {
  return sendAtCommand("AT+CMGD=1,4", "OK", 10000);
}
