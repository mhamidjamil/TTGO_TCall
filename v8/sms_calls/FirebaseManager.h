#ifndef V8_FIREBASE_MANAGER_H
#define V8_FIREBASE_MANAGER_H

#include <Arduino.h>
#include "ConfigManager.h"

struct FirebaseCommand {
  String id;
  String type;
  String number;
  String message;
  String status;
  String errorReason;
};

class FirebaseManager {
public:
  bool begin(const V8Config &config);
  bool isReady() const;
  bool pollCommands();
  bool fetchNextCommand(FirebaseCommand &outCommand);
  bool updateCommandStatus(const FirebaseCommand &command, const String &status, const String &errorReason = String());
  bool updateCounterSnapshot(int dailyCount, int weeklyCount, int monthlyCount);
  bool fetchCounterSnapshot(int &dailyCount, int &weeklyCount, int &monthlyCount);
  bool pushTelemetry(float temperature, float humidity, int sentToday, int sentWeek, int sentMonth, unsigned long epochSeconds);
  bool pushStartupStatus(const String &bootTime, const String &wifiMode, const String &ipAddress, bool firebaseReady);
  bool pushLandingSnapshot(float temperature,
                           float humidity,
                           int sentToday,
                           int sentWeek,
                           int sentMonth,
                           int dailyLimit,
                           int weeklyLimit,
                           int monthlyLimit,
                           const String &wifiMode,
                           const String &ipAddress,
                           bool firebaseReady,
                           bool telemetryPushOk,
                           const String &telemetryMessage,
                           unsigned long epochSeconds);
  String lastError() const;

private:
  bool ensureAuthenticated();
  bool authenticate();
  bool bootstrapPaths();
  String rootPathFromConfig() const;
  String buildPathUrl(const String &path) const;
  bool httpGetJson(const String &url, String &responseBody, int &statusCode);
  bool httpPatchJson(const String &url, const String &payload, String &responseBody, int &statusCode);

  V8Config config{};
  bool ready = false;
  unsigned long tokenExpiresAtMs = 0;
  String idToken;
  String error;
};

#endif
