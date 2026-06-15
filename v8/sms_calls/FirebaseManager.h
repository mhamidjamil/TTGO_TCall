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

struct FirebaseRuntimeSettings {
  uint32_t intervalOfDhtSeconds = 15;
  bool showFirebasePushLogs = true;
  bool showThingSpeakPushLogs = true;
  int dailySmsLimit = 200;
  int weeklySmsLimit = 950;
  int monthlySmsLimit = 4900;
  String ntfyUrl;
  bool createdIntervalOfDht = false;
  bool createdShowFirebasePushLogs = false;
  bool createdShowThingSpeakPushLogs = false;
  bool createdDailySmsLimit = false;
  bool createdWeeklySmsLimit = false;
  bool createdMonthlySmsLimit = false;
  bool createdNtfyUrl = false;
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
  bool pushTelemetry(float temperature, float humidity, unsigned long epochSeconds);
  bool pushStartupStatus(const String &bootTime, const String &wifiMode, const String &ipAddress, bool firebaseReady);
  bool pushLandingSnapshot(float temperature,
                           float humidity,
                           int sentToday,
                           int sentWeek,
                           int sentMonth,
                           const String &wifiMode,
                           const String &ipAddress,
                           bool firebaseReady,
                           bool telemetryPushOk,
                           const String &telemetryMessage,
                           unsigned long epochSeconds);
  bool fetchRuntimeSettings(FirebaseRuntimeSettings &outSettings,
                            uint32_t defaultIntervalOfDhtSeconds,
                            bool defaultShowFirebasePushLogs,
                            bool defaultShowThingSpeakPushLogs = true,
                            const String &defaultNtfyUrl = String());
  bool pushSimModuleEvent(const String &type,
                          const String &number,
                          const String &message,
                          bool blocked,
                          const String &pakistanTimestamp,
                          int simIndex = -1);
  bool fetchSimBlockLists(String *blockedCallers,
                          size_t maxBlockedCallers,
                          size_t &blockedCallerCount,
                          String *blockedSmsSenders,
                          size_t maxBlockedSmsSenders,
                          size_t &blockedSmsSenderCount);
  bool bootstrapSimModulePaths();
  bool cleanupLegacySimModulePaths();
  String lastError() const;

private:
  bool ensureAuthenticated();
  bool authenticate();
  bool bootstrapPaths();
  String rootPathFromConfig() const;
  bool tryUpdateDatabaseUrlFromBody(const String &responseBody);
  String rebuildUrlWithCurrentBase(const String &originalUrl) const;
  String buildPathUrl(const String &path) const;
  String buildFirestoreUrl(const String &path) const;
  bool httpGetJson(const String &url, String &responseBody, int &statusCode);
  bool httpPatchJson(const String &url, const String &payload, String &responseBody, int &statusCode);
  bool httpGetBearer(const String &url, String &responseBody, int &statusCode);
  bool httpPostBearerJson(const String &url, const String &payload, String &responseBody, int &statusCode);
  bool httpPatchBearerJson(const String &url, const String &payload, String &responseBody, int &statusCode);
  bool httpDeleteBearer(const String &url, String &responseBody, int &statusCode);
  bool fetchFirestoreSettingsList(const String &fieldName, String *numbers, size_t maxNumbers, size_t &numberCount);
  bool appendCsvNumbers(const String &csvText, String *numbers, size_t maxNumbers, size_t &numberCount);
  bool ensureSimSettingsDocument();
  bool deleteFirestoreCollectionDocuments(const String &collectionPath);
  bool deleteFirestoreDocument(const String &documentPath);
  bool ensureFirestoreDocument(const String &documentPath);

  V8Config config{};
  bool ready = false;
  unsigned long tokenExpiresAtMs = 0;
  String idToken;
  String error;
};

#endif
