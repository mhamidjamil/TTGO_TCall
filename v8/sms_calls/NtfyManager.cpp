#include "NtfyManager.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

bool NtfyManager::begin(const String &url) {
  setUrl(url);
  return topicUrl.length() > 0;
}

void NtfyManager::setUrl(const String &url) {
  topicUrl = url;
  topicUrl.trim();
  error = String();
}

bool NtfyManager::notify(const String &title, const String &message) {
  if (topicUrl.length() == 0) {
    error = "ntfy url missing";
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    error = "wifi disconnected";
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, topicUrl)) {
    error = "ntfy begin failed";
    return false;
  }

  http.addHeader("Content-Type", "text/plain");
  http.addHeader("Title", title);
  int statusCode = http.POST(message);
  String body = http.getString();
  http.end();

  if (statusCode < 200 || statusCode >= 300) {
    error = String("ntfy failed code=") + statusCode + String(" body=") + body;
    return false;
  }

  error = String();
  return true;
}

bool NtfyManager::test() {
  return notify("TTGO ntfy test", "Test notification from TTGO T-Call v8");
}

String NtfyManager::currentUrl() const {
  return topicUrl;
}

String NtfyManager::lastError() const {
  return error;
}
