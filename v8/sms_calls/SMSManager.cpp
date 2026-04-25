#include "SMSManager.h"

bool SMSManager::begin() {
  return true;
}

bool SMSManager::sendMessage(const String &number, const String &message) {
  (void)number;
  (void)message;
  return true;
}
