#ifndef V8_WIFI_MANAGER_H
#define V8_WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
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
  void setConnectedSsid(const String &ssid);

  bool stationConnected = false;
  bool accessPointActive = false;
  // connectedSsid is a heap String written during (re)connects on the main loop
  // and read by the web-server task — guard it so a mid-reallocation read from
  // the other core can't dereference a freed buffer.
  String connectedSsid;
  SemaphoreHandle_t ssidLock = nullptr;
};

#endif
