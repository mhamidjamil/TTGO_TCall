#ifndef V8_RATE_LIMIT_MANAGER_H
#define V8_RATE_LIMIT_MANAGER_H

#include <Arduino.h>
#include "ConfigManager.h"

class RateLimitManager {
public:
  void begin(const V8Config &config);
  void setLimits(int dailySmsLimit, int weeklySmsLimit, int monthlySmsLimit);
  bool canSend(String &reason) const;
  // Reset whichever windows the calendar has moved past. Returns true when at
  // least one counter was reset, so the caller can push the fresh numbers.
  bool rollover(unsigned long epochSeconds);
  void recordSend(unsigned long epochSeconds);
  // Adopt the cloud snapshot, once, on first contact. A count stored against an
  // older window key restores as 0 instead of being carried into the new window,
  // and sends made before the snapshot arrived are added on top so they survive.
  void loadSnapshot(int dailyCount,
                    int weeklyCount,
                    int monthlyCount,
                    const String &storedDayKey,
                    const String &storedWeekKey,
                    const String &storedMonthKey,
                    unsigned long epochSeconds);
  // False until the cloud snapshot has been read. While false the device must
  // not write counters back, or a boot-time zero overwrites the real totals.
  bool isRestored() const;
  int dailyCount() const;
  int weeklyCount() const;
  int monthlyCount() const;
  String dayKey() const;
  String weekKey() const;
  String monthKey() const;

private:
  void refreshWindowKeys(unsigned long epochSeconds);

  int dailyLimit = 0;
  int weeklyLimit = 0;
  int monthlyLimit = 0;
  int daily = 0;
  int weekly = 0;
  int monthly = 0;
  String currentDayKey;
  String currentWeekKey;
  String currentMonthKey;
  bool restored = false;
};

#endif
