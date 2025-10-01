#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

struct Config {
  bool useApiSecret = true;
  String apiSecret = "";
  String forwardUrl = "";
  String forwardApiKey = "";
  bool allowSms = true;
  bool allowCall = true;
  // URL to check for remote settings and current settings version
  String settingsUrl = "";
  String settingsVersion = "";
};

class ConfigManager {
public:
  ConfigManager();
  void begin();
  Config get();
  bool save(const Config &cfg);
  // If configured, fetch remote settings and apply them. Returns true if new settings applied.
  bool checkAndApplyRemoteSettings();

private:
  const char *path = "/config.json";
  Config cfg;
  void loadFromFS();
};

#endif
