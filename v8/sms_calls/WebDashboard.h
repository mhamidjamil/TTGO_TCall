#ifndef V8_WEB_DASHBOARD_H
#define V8_WEB_DASHBOARD_H

#include <Arduino.h>
#include "ConfigManager.h"

class WiFiManager;
class FirebaseManager;
class NtfyManager;

class WebDashboard {
public:
  bool begin(const V8Config &config, WiFiManager &wifiManager, FirebaseManager &firebaseManager, NtfyManager &ntfyManager);
  void loop();
  String docsUrl() const;

private:
  const V8Config *config = nullptr;
  WiFiManager *wifiManager = nullptr;
  FirebaseManager *firebaseManager = nullptr;
  NtfyManager *ntfyManager = nullptr;
};

#endif
