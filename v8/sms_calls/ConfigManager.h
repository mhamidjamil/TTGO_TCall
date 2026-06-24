#ifndef V8_CONFIG_MANAGER_H
#define V8_CONFIG_MANAGER_H

#include <Arduino.h>

struct V8Config {
  bool wifiEnabled;
  bool apFallbackEnabled;
  bool logVerbose;
  bool firebaseUseAnonymous;
  char wifiSsid[64];
  char wifiPass[64];
  char wifiSsidBackup[64];
  char wifiPassBackup[64];
  // User-editable WiFi pairs saved in LittleFS from the dashboard. Empty by
  // default. Tried BEFORE the secrets.h networks above so a new workplace can be
  // configured without reflashing; if both fail the device falls back to the
  // secrets networks and then AP mode.
  char userWifiSsid1[64];
  char userWifiPass1[64];
  char userWifiSsid2[64];
  char userWifiPass2[64];
  char apSsid[32];
  char apPass[32];
  int webServerPort;
  int pollingIntervalSeconds;
  int dailySmsLimit;
  int weeklySmsLimit;
  int monthlySmsLimit;
  char dashboardPassword[64];
  char deviceId[48];
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
  char firebaseTelemetryPath[96];
  char ntfyUrl[160];
  unsigned long thingSpeakChannelId;
  char thingSpeakWriteApiKey[64];
};

class ConfigManager {
public:
  void begin();
  bool save();
  const V8Config &get() const;
  // Persist the dashboard-entered WiFi pairs to LittleFS. Applied on next reboot.
  bool updateUserWifi(const String &ssid1, const String &pass1,
                      const String &ssid2, const String &pass2);

private:
  void loadDefaults();
  bool loadFromLittleFS();
  bool saveToLittleFS();
  void readJsonConfig(const String &jsonText);
  String writeJsonConfig() const;

  V8Config config;
};

#endif
