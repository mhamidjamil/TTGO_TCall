#include "RateLimitManager.h"

#include <time.h>

namespace {
// Quota windows are calendar windows in Pakistan local time (UTC+5), matching
// the timestamps written elsewhere. "sentToday" therefore means the operator's
// today; the old millis() windows measured 24 h/7 d/30 d from boot, so a device
// that rebooted daily never rolled over at all.
constexpr long kLocalOffsetSeconds = 5L * 60L * 60L;
constexpr long kSecondsPerDay = 86400L;

String formatDayKey(long localDays) {
  time_t raw = (time_t)localDays * kSecondsPerDay;
  struct tm timeInfo;
  if (!gmtime_r(&raw, &timeInfo)) {
    return String();
  }
  char buffer[11];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d", &timeInfo);
  return String(buffer);
}
}

void RateLimitManager::begin(const V8Config &config) {
  dailyLimit = config.dailySmsLimit;
  weeklyLimit = config.weeklySmsLimit;
  monthlyLimit = config.monthlySmsLimit;
  daily = 0;
  weekly = 0;
  monthly = 0;
  currentDayKey = String();
  currentWeekKey = String();
  currentMonthKey = String();
  restored = false;
}

void RateLimitManager::setLimits(int dailySmsLimit, int weeklySmsLimit, int monthlySmsLimit) {
  dailyLimit = dailySmsLimit;
  weeklyLimit = weeklySmsLimit;
  monthlyLimit = monthlySmsLimit;
}

bool RateLimitManager::canSend(String &reason) const {
  if (dailyLimit > 0 && daily >= dailyLimit) {
    reason = "daily_limit_reached";
    return false;
  }
  if (weeklyLimit > 0 && weekly >= weeklyLimit) {
    reason = "weekly_limit_reached";
    return false;
  }
  if (monthlyLimit > 0 && monthly >= monthlyLimit) {
    reason = "monthly_limit_reached";
    return false;
  }
  reason = String();
  return true;
}

void RateLimitManager::refreshWindowKeys(unsigned long epochSeconds) {
  if (epochSeconds <= 1000UL) {
    return;  // no NTP yet — keep the keys we have rather than guess from uptime
  }
  long localDays = (long)((epochSeconds + (unsigned long)kLocalOffsetSeconds) / (unsigned long)kSecondsPerDay);
  currentDayKey = formatDayKey(localDays);
  // 1970-01-01 was a Thursday; the +3 shift puts the week boundary on Monday.
  currentWeekKey = formatDayKey(localDays - ((localDays + 3) % 7));
  currentMonthKey = currentDayKey.substring(0, 7);
}

bool RateLimitManager::rollover(unsigned long epochSeconds) {
  String previousDayKey = currentDayKey;
  String previousWeekKey = currentWeekKey;
  String previousMonthKey = currentMonthKey;
  refreshWindowKeys(epochSeconds);
  if (previousDayKey.length() == 0) {
    return false;  // first keys after NTP came up — nothing to compare against
  }

  bool didReset = false;
  if (currentDayKey != previousDayKey) {
    daily = 0;
    didReset = true;
  }
  if (currentWeekKey != previousWeekKey) {
    weekly = 0;
    didReset = true;
  }
  if (currentMonthKey != previousMonthKey) {
    monthly = 0;
    didReset = true;
  }
  return didReset;
}

void RateLimitManager::recordSend(unsigned long epochSeconds) {
  rollover(epochSeconds);
  daily++;
  weekly++;
  monthly++;
}

void RateLimitManager::loadSnapshot(int dailyCount,
                                    int weeklyCount,
                                    int monthlyCount,
                                    const String &storedDayKey,
                                    const String &storedWeekKey,
                                    const String &storedMonthKey,
                                    unsigned long epochSeconds) {
  if (restored) {
    return;  // already authoritative; re-adopting would drop sends made since
  }
  refreshWindowKeys(epochSeconds);

  // Whatever is counted right now was sent between boot and this first cloud
  // read, so it belongs on top of the stored totals.
  int sentDaily = daily;
  int sentWeekly = weekly;
  int sentMonthly = monthly;
  daily = (storedDayKey == currentDayKey && dailyCount > 0 ? dailyCount : 0) + sentDaily;
  weekly = (storedWeekKey == currentWeekKey && weeklyCount > 0 ? weeklyCount : 0) + sentWeekly;
  monthly = (storedMonthKey == currentMonthKey && monthlyCount > 0 ? monthlyCount : 0) + sentMonthly;
  restored = true;
}

bool RateLimitManager::isRestored() const {
  return restored;
}

int RateLimitManager::dailyCount() const {
  return daily;
}

int RateLimitManager::weeklyCount() const {
  return weekly;
}

int RateLimitManager::monthlyCount() const {
  return monthly;
}

String RateLimitManager::dayKey() const {
  return currentDayKey;
}

String RateLimitManager::weekKey() const {
  return currentWeekKey;
}

String RateLimitManager::monthKey() const {
  return currentMonthKey;
}
