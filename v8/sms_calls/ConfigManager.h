#ifndef V8_CONFIG_MANAGER_H
#define V8_CONFIG_MANAGER_H

#include <Arduino.h>

struct V8Config {
  bool wifiEnabled;
  bool apFallbackEnabled;
  bool logVerbose;
  char wifiSsid[64];
  char wifiPass[64];
  char apSsid[32];
  char apPass[32];
  int webServerPort;
  int pollingIntervalSeconds;
  int dailySmsLimit;
  int weeklySmsLimit;
  int monthlySmsLimit;
  char dashboardPassword[64];
  char deviceName[64];
  char ownerName[64];
  char myNumber[24];
  char defaultCountryCode[8];
  char adminNumbers[128];
  char authenticNumbers[128];
  char bypassKey[64];
  char firebaseProjectId[96];
  char firebaseDatabaseUrl[160];
  char firebaseApiKey[96];
  char firebaseUserEmail[96];
  char firebaseUserPassword[96];
  char firebaseCommandPath[96];
  char firebaseHistoryPath[96];
  char firebaseCounterPath[96];
  char firebaseStatusPath[96];
};

class ConfigManager {
public:
  void begin();
  bool save();
  const V8Config &get() const;

private:
  void loadDefaults();
  bool loadFromSPIFFS();
  bool saveToSPIFFS();
  void readJsonConfig(const String &jsonText);
  String writeJsonConfig() const;

  V8Config config;
};

#endif
