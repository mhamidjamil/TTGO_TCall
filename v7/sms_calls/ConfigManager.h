#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

struct Config {
  // API Configuration
  bool useApiSecret = true;
  String apiSecret = "";
  String forwardUrl = "";
  String forwardApiKey = "";
  
  // Device Permissions
  bool allowSms = true;
  bool allowCall = true;
  
  // Remote Settings
  String settingsUrl = "";
  String settingsVersion = "";
  
  // NTFY Configuration
  bool ntfyEnabled = true;
  String ntfyServerUrl = "";
  String ntfyTopic = "";
  
  // Owner and Device Info
  String ownerName = "";
  String myNumber = "";
  String deviceName = "";
  
  // Gate ESP Configuration
  bool gateEspEnabled = false;
  String gateEspUrl = "";
  
  // Admin Numbers (comma-separated)
  String adminNumbers = "";
  
  // Auto-reply Configuration
  String autoReplyMessage = "";
  
  // Message Management
  bool logMessages = true;
  bool deleteAfterForward = true;
};

class ConfigManager {
public:
  ConfigManager();
  void begin();
  Config get();
  bool save(const Config &cfg);
  // If configured, fetch remote settings and apply them. Returns true if new settings applied.
  bool checkAndApplyRemoteSettings();
  void loadFromNVS();
  void saveToNVS();
  
  // SPIFFS methods
  bool loadFromSPIFFS();
  bool saveToSPIFFS();
  
  // Debug method
  void printConfig();
  
  // Reset to defaults method
  bool resetToDefaults();

private:
  const char *path = "/config.json";
  Config cfg;
  
  void loadDefaults();
  void loadFromFS();
  
  // Helper methods for JSON serialization (templates to handle any JsonDocument type)
  template<typename T> void configToJson(const Config &config, T &doc);
  template<typename T> void jsonToConfig(const T &doc, Config &config);
};

#endif
