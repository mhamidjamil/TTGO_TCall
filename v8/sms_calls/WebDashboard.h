#ifndef V8_WEB_DASHBOARD_H
#define V8_WEB_DASHBOARD_H

#include <Arduino.h>
#include "ConfigManager.h"

class WiFiManager;
class FirebaseManager;
class NtfyManager;

// Serves the dashboard + device API from its OWN FreeRTOS task (core 0), so
// pages load instantly even while the main loop is blocked for tens of seconds
// in TLS calls or modem AT waits. begin() spawns the task; there is no loop().
class WebDashboard {
public:
  bool begin(const V8Config &config, WiFiManager &wifiManager, FirebaseManager &firebaseManager, NtfyManager &ntfyManager, ConfigManager &configManager);
  bool consumeRuntimeSyncRequest();
  String docsUrl() const;

private:
  static void serverTask(void *param);

  const V8Config *config = nullptr;
  WiFiManager *wifiManager = nullptr;
  FirebaseManager *firebaseManager = nullptr;
  NtfyManager *ntfyManager = nullptr;
  ConfigManager *configManager = nullptr;
  // Set from the web task, consumed by the main loop — volatile is sufficient
  // for a single-writer/single-reader bool flag.
  volatile bool runtimeSyncRequested = false;
  void *taskHandle = nullptr;
};

#endif
