#ifndef V8_WIFI_MANAGER_H
#define V8_WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "ConfigManager.h"

class WiFiManager {
public:
  bool begin(const V8Config &config);
  bool isStationConnected() const;
  bool isAccessPointActive() const;
  String modeName() const;
  IPAddress localIp() const;

private:
  bool connectStation(const V8Config &config);
  void startAccessPoint(const V8Config &config);

  bool stationConnected = false;
  bool accessPointActive = false;
};

#endif
