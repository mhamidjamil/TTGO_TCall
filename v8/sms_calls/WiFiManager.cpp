#include "WiFiManager.h"

#include <WiFi.h>

bool WiFiManager::begin(const ConfigManager &configManager) {
  const V8Config &config = configManager.get();
  stationConnected = false;
  accessPointActive = false;
  connectedSsid = String();

  if (config.wifiEnabled) {
    stationConnected = connectStation(config, configManager);
  }

  if (!stationConnected && config.apFallbackEnabled) {
    startAccessPoint(config);
  }

  return stationConnected || accessPointActive;
}

bool WiFiManager::connectStation(const V8Config &config, const ConfigManager &cm) {
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
      connectedSsid = String(ssid);
      return true;
    }

    WiFi.disconnect(true);
    delay(200);
    return false;
  };

  // Dynamic LittleFS networks first (managed via dashboard/SMS/serial).
  for (int i = 0; i < cm.getWifiNetworkCount(); i++) {
    WifiNetwork net = cm.getWifiNetwork(i);
    String label = String("saved[") + String(i) + String("]");
    if (attempt(net.ssid, net.pass, label.c_str())) {
      return true;
    }
  }

  // Legacy fixed slots (backward-compat with older dashboard WiFi pairs).
  if (attempt(config.userWifiSsid1, config.userWifiPass1, "legacy slot 1")) {
    return true;
  }
  if (attempt(config.userWifiSsid2, config.userWifiPass2, "legacy slot 2")) {
    return true;
  }

  // Compile-time secrets.h networks.
  if (attempt(config.wifiSsid, config.wifiPass, "secrets primary")) {
    return true;
  }
  if (attempt(config.wifiSsidBackup, config.wifiPassBackup, "secrets backup")) {
    return true;
  }

  Serial.println("[WIFI] all station networks failed");
  connectedSsid = String();
  return false;
}

void WiFiManager::startAccessPoint(const V8Config &config) {
  WiFi.mode(WIFI_AP);
  accessPointActive = WiFi.softAP(config.apSsid, config.apPass);
}

bool WiFiManager::tryReconnect(const ConfigManager &configManager) {
  if (stationConnected) {
    return true;
  }
  const V8Config &config = configManager.get();
  Serial.println("[WIFI] reconnect attempt (STA only)");
  if (accessPointActive) {
    WiFi.softAPdisconnect(true);
    accessPointActive = false;
    delay(200);
  }
  stationConnected = connectStation(config, configManager);
  if (stationConnected) {
    Serial.println("[WIFI] reconnected as STA");
  } else {
    if (config.apFallbackEnabled) {
      startAccessPoint(config);
    }
    Serial.println("[WIFI] reconnect failed; AP restored");
  }
  return stationConnected;
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

String WiFiManager::getConnectedSsid() const {
  if (!stationConnected) {
    return String();
  }
  return connectedSsid;
}
