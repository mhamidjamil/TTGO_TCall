#ifndef V8_WIFI_MANAGER_H
#define V8_WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "ConfigManager.h"

class WiFiManager {
public:
  bool begin(const ConfigManager &configManager);
  // STA-only retry when in AP/OFFLINE mode. Restores AP on failure.
  bool tryReconnect(const ConfigManager &configManager);
  bool isStationConnected() const;
  bool isAccessPointActive() const;
  String modeName() const;
  IPAddress localIp() const;
  // SSID of the currently connected station network, or empty string.
  String getConnectedSsid() const;

private:
  bool connectStation(const V8Config &config, const ConfigManager &cm);
  void startAccessPoint(const V8Config &config);

  bool stationConnected = false;
  bool accessPointActive = false;
  String connectedSsid;
};

#endif
