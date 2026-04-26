#include "RateLimitManager.h"

namespace {
constexpr unsigned long kDayWindowMs = 24UL * 60UL * 60UL * 1000UL;
constexpr unsigned long kWeekWindowMs = 7UL * kDayWindowMs;
constexpr unsigned long kMonthWindowMs = 30UL * kDayWindowMs;
}

void RateLimitManager::begin(const V8Config &config) {
  dailyLimit = config.dailySmsLimit;
  weeklyLimit = config.weeklySmsLimit;
  monthlyLimit = config.monthlySmsLimit;
  daily = 0;
  weekly = 0;
  monthly = 0;
  dayWindowStartMs = millis();
  weekWindowStartMs = millis();
  monthWindowStartMs = millis();
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

void RateLimitManager::rolloverIfNeeded() {
  unsigned long now = millis();
  if (now - dayWindowStartMs >= kDayWindowMs) {
    daily = 0;
    dayWindowStartMs = now;
  }
  if (now - weekWindowStartMs >= kWeekWindowMs) {
    weekly = 0;
    weekWindowStartMs = now;
  }
  if (now - monthWindowStartMs >= kMonthWindowMs) {
    monthly = 0;
    monthWindowStartMs = now;
  }
}

void RateLimitManager::recordSend() {
  rolloverIfNeeded();
  daily++;
  weekly++;
  monthly++;
}

void RateLimitManager::sync() {
  rolloverIfNeeded();
}

void RateLimitManager::loadSnapshot(int dailyCount, int weeklyCount, int monthlyCount) {
  daily = dailyCount < 0 ? 0 : dailyCount;
  weekly = weeklyCount < 0 ? 0 : weeklyCount;
  monthly = monthlyCount < 0 ? 0 : monthlyCount;
  dayWindowStartMs = millis();
  weekWindowStartMs = millis();
  monthWindowStartMs = millis();
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
