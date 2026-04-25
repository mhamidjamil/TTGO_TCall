#include "SMSManager.h"

bool SMSManager::begin() {
  return true;
}

bool SMSManager::sendMessage(const char *number, const char *message) {
  (void)number;
  (void)message;
  return false;
}
