#include "ThingSpeakManager.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>

bool ThingSpeakManager::begin(const V8Config &incomingConfig) {
  config = incomingConfig;
  ready = false;
  error = String();

  if (config.thingSpeakChannelId == 0 || String(config.thingSpeakWriteApiKey).isEmpty()) {
    error = "ThingSpeak config missing";
    return false;
  }

  ready = true;
  return true;
}

bool ThingSpeakManager::isReady() const {
  return ready;
}

String ThingSpeakManager::lastError() const {
  return error;
}

bool ThingSpeakManager::update(float temperature, float humidity) {
  if (!ready) {
    error = "ThingSpeak not ready";
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    error = "wifi disconnected";
    return false;
  }

  String url = String("https://api.thingspeak.com/update?api_key=") + config.thingSpeakWriteApiKey +
               String("&field1=") + String(temperature, 1) +
               String("&field2=") + String(humidity, 1);

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, url)) {
    error = "ThingSpeak begin failed";
    return false;
  }

  int statusCode = http.GET();
  String responseBody = http.getString();
  http.end();

  if (statusCode < 200 || statusCode >= 300) {
    error = String("ThingSpeak update failed code=") + statusCode + String(" body=") + responseBody;
    return false;
  }

  responseBody.trim();
  long entryId = responseBody.toInt();
  if (entryId <= 0) {
    error = String("ThingSpeak rejected update body=") + responseBody;
    return false;
  }

  error = String();
  return true;
}
