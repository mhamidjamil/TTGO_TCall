#ifndef V8_CONFIG_MANAGER_H
#define V8_CONFIG_MANAGER_H

struct V8Config {
  bool wifiEnabled;
  bool apFallbackEnabled;
  char wifiSsid[64];
  char wifiPass[64];
  char apSsid[32];
  char apPass[32];
  int webServerPort;
  int pollingIntervalSeconds;
  int dailySmsLimit;
  int weeklySmsLimit;
  int monthlySmsLimit;
};

class ConfigManager {
public:
  void begin();
  const V8Config &get() const;
private:
  V8Config config;
};

#endif
