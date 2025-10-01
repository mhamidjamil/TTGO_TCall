#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

struct Config {
  bool useApiSecret = true;
  String apiSecret = "";
  String forwardUrl = "";
  String forwardApiKey = "";
  bool allowSms = true;
  bool allowCall = true;
};

class ConfigManager {
public:
  ConfigManager();
  void begin();
  Config get();
  bool save(const Config &cfg);

private:
  const char *path = "/config.json";
  Config cfg;
  void loadFromFS();
};

#endif
