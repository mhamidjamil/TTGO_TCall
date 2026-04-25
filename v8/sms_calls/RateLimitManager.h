#ifndef V8_RATE_LIMIT_MANAGER_H
#define V8_RATE_LIMIT_MANAGER_H

#include <Arduino.h>
#include "ConfigManager.h"

class RateLimitManager {
public:
  void begin(const V8Config &config);
  bool canSend(String &reason) const;
  void recordSend();
  void sync();
  int dailyCount() const;
  int weeklyCount() const;
  int monthlyCount() const;

private:
  void rolloverIfNeeded();

  int dailyLimit = 0;
  int weeklyLimit = 0;
  int monthlyLimit = 0;
  int daily = 0;
  int weekly = 0;
  int monthly = 0;
  unsigned long dayWindowStartMs = 0;
  unsigned long weekWindowStartMs = 0;
  unsigned long monthWindowStartMs = 0;
};

#endif
