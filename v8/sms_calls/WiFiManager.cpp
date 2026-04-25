#include "WiFiManager.h"

#include <WiFi.h>

bool WiFiManager::begin(const V8Config &config) {
  stationConnected = false;
  accessPointActive = false;

  if (config.wifiEnabled) {
    stationConnected = connectStation(config);
  }

  if (!stationConnected && config.apFallbackEnabled) {
    startAccessPoint(config);
  }

  return stationConnected || accessPointActive;
}

bool WiFiManager::connectStation(const V8Config &config) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.wifiSsid, config.wifiPass);

  const unsigned long timeoutMs = 15000;
  unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - started) < timeoutMs) {
    delay(250);
  }

  return WiFi.status() == WL_CONNECTED;
}

void WiFiManager::startAccessPoint(const V8Config &config) {
  WiFi.mode(WIFI_AP);
  accessPointActive = WiFi.softAP(config.apSsid, config.apPass);
}

bool WiFiManager::isStationConnected() const {
  return stationConnected;
}

bool WiFiManager::isAccessPointActive() const {
  return accessPointActive;
}

String WiFiManager::modeName() const {
  if (stationConnected) {
    return "STA";
  }
  if (accessPointActive) {
    return "AP";
  }
  return "OFFLINE";
}

IPAddress WiFiManager::localIp() const {
  if (stationConnected) {
    return WiFi.localIP();
  }
  if (accessPointActive) {
    return WiFi.softAPIP();
  }
  return IPAddress(0, 0, 0, 0);
}
