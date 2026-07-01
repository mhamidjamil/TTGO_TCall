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

// A queued outgoing job. The Firestore document id IS the phone number, so
// id == phoneNumber. enqueBy records which app/user created the job (for future
// reply linking); the firmware only preserves it.
struct FirestoreJob {
  String id;
  String phoneNumber;
  String message;
  String status;
  String error;
  String enqueBy;
  bool userPicked = false;
  int durationSeconds = 0;
};

// The four editable block lists stored on the sim_module/device document.
struct BlockLists {
  static const size_t kMax = 32;
  String incomingCallers[kMax];
  size_t incomingCallerCount = 0;
  String incomingSms[kMax];
  size_t incomingSmsCount = 0;
  String outgoingCallers[kMax];
  size_t outgoingCallerCount = 0;
  String outgoingSms[kMax];
  size_t outgoingSmsCount = 0;
};

struct FirebaseRuntimeSettings {
  uint32_t intervalOfDhtSeconds = 15;
  bool showFirebasePushLogs = true;
  bool showThingSpeakPushLogs = true;
  bool jobLogs = true;
  int dailySmsLimit = 200;
  int weeklySmsLimit = 950;
  int monthlySmsLimit = 4900;
  String ntfyUrl;
  String ntfyLogUrl;
  // Desired WiFi pairs, managed from the dashboard via the RTDB runtime node.
  // The device persists them to LittleFS on sync; they apply on next reboot.
  String wifiSsid1;
  String wifiPass1;
  String wifiSsid2;
  String wifiPass2;
  bool createdIntervalOfDht = false;
  bool createdShowFirebasePushLogs = false;
  bool createdShowThingSpeakPushLogs = false;
  bool createdJobLogs = false;
  bool createdDailySmsLimit = false;
  bool createdWeeklySmsLimit = false;
  bool createdMonthlySmsLimit = false;
  bool createdNtfyUrl = false;
  bool createdNtfyLogUrl = false;
};

// SIM package/subscription state, persisted at RTDB /ttgo_tcall/package and
// mirrored to LittleFS. Detection tokens and the safety margin live here too so
// they can be tuned from the cloud without reflashing (e.g. when the operator
// changes the SMS count or the message wording).
struct PackageState {
  bool known = false;              // have we ever recorded a subscription?
  unsigned long subscribedEpoch = 0;
  unsigned long expiryEpoch = 0;
  int validityDays = 0;
  long smsAllowance = 0;
  int safetyMarginDays = 1;        // subtract from validity to be safe (Jazz expires early)
  String matchTokens;              // comma-separated: ALL must appear to count as a subscription SMS
  String lastMessage;
  bool createdNode = false;        // set true when we had to create/heal the node
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

  // --- Gateway control plane (sim_module/device, /sms, /calls) ---
  bool bootstrapGateway(const String &deviceName,
                        int pollIntervalSeconds,
                        bool missedCallMode);
  // Fetch up to maxJobs PENDING sms jobs via a server-side query (status ==
  // pending), so finished jobs are never downloaded. Does NOT claim them.
  bool fetchPendingSmsJobs(FirestoreJob *outJobs, int maxJobs, int &outCount);
  // Mark an sms job in_progress (call right before sending).
  bool claimSmsJob(const FirestoreJob &job);
  // Fetch + claim the next pending call job (server-side query, limit 1).
  bool fetchNextCallJob(FirestoreJob &outJob);
  bool fetchGatewayActive(bool &outActive);
  bool updateSmsJobStatus(const FirestoreJob &job, const String &status, const String &errorReason = String());
  bool updateCallJobStatus(const FirestoreJob &job,
                           const String &status,
                           bool userPicked,
                           int durationSeconds,
                           const String &errorReason = String());
  // Per-number incoming archives (sim_module/sms/sms_received/{number} etc.).
  bool pushSmsReceived(const String &number,
                       const String &originalMessage,
                       const String &normalizedMessage,
                       bool wasDecoded,
                       bool notified,
                       bool blocked,
                       const String &pakistanTimestamp,
                       int simIndex = -1);
  bool pushCallReceived(const String &number,
                        bool notified,
                        bool blocked,
                        const String &pakistanTimestamp);
  // Atomically increment a counter integer field on sim_module/device.
  bool incrementDeviceCounter(const char *field);
  bool pushDeviceHeartbeat(const String &deviceName,
                           int batteryPercent,
                           int signalStrength,
                           const String &networkOperator,
                           unsigned long epochSeconds,
                           int pollIntervalSeconds,
                           bool missedCallMode);
  bool recoverStuckJobs(unsigned long cutoffEpochSeconds);
  bool fetchBlockLists(BlockLists &out);
  // Write connectedSsid to /ttgo_tcall/settings/runtime in RTDB.
  bool pushConnectedSsid(const String &ssid);
  // Package/subscription state at RTDB /ttgo_tcall/package. fetch heals defaults
  // (createdNode=true) when the node is missing; push persists the given state.
  bool fetchPackageState(PackageState &outState, const String &defaultTokens, int defaultSafetyMarginDays);
  bool pushPackageState(const PackageState &state);
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
  String firestoreDocumentName(const String &path) const;
  bool httpGetJson(const String &url, String &responseBody, int &statusCode);
  bool httpPatchJson(const String &url, const String &payload, String &responseBody, int &statusCode);
  bool httpGetBearer(const String &url, String &responseBody, int &statusCode);
  bool httpPostBearerJson(const String &url, const String &payload, String &responseBody, int &statusCode);
  bool httpPatchBearerJson(const String &url, const String &payload, String &responseBody, int &statusCode);
  // Server-side query for pending jobs under parentPath's collectionId.
  bool queryPendingJobs(const String &parentPath, const char *collectionId, int limit,
                        FirestoreJob *outJobs, int maxJobs, int &outCount);
  bool claimJob(const String &collectionPath, const FirestoreJob &job);
  bool ensureDeviceDocument();
  bool ensureFirestoreDocument(const String &documentPath);

  V8Config config{};
  bool ready = false;
  unsigned long tokenExpiresAtMs = 0;
  String idToken;
  String error;
};

#endif
