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
  auto attempt = [&](const char *ssid, const char *pass, const char *origin) -> bool {
    if (ssid == nullptr || strlen(ssid) == 0) {
      return false;
    }
    Serial.print("[WIFI] trying ");
    Serial.print(origin);
    Serial.print(" ssid=");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    const unsigned long timeoutMs = 15000;
    unsigned long started = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - started) < timeoutMs) {
      delay(250);
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("[WIFI] connected via ");
      Serial.print(origin);
      Serial.print(" ssid=");
      Serial.println(ssid);
      return true;
    }

    WiFi.disconnect(true);
    delay(200);
    return false;
  };

  // LittleFS user-entered pairs first, then the secrets.h networks, then AP.
  if (attempt(config.userWifiSsid1, config.userWifiPass1, "saved pair 1")) {
    return true;
  }
  if (attempt(config.userWifiSsid2, config.userWifiPass2, "saved pair 2")) {
    return true;
  }
  if (attempt(config.wifiSsid, config.wifiPass, "secrets primary")) {
    return true;
  }
  if (attempt(config.wifiSsidBackup, config.wifiPassBackup, "secrets backup")) {
    return true;
  }

  Serial.println("[WIFI] all station networks failed");
  return false;
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
