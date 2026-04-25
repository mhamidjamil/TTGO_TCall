#include "WebDashboard.h"

#include <WebServer.h>
#include "FirebaseManager.h"
#include "WiFiManager.h"

namespace {
WebServer *server = nullptr;
}

bool WebDashboard::begin(const V8Config &incomingConfig, WiFiManager &incomingWiFiManager, FirebaseManager &incomingFirebaseManager) {
  config = &incomingConfig;
  wifiManager = &incomingWiFiManager;
  firebaseManager = &incomingFirebaseManager;

  if (server != nullptr) {
    delete server;
    server = nullptr;
  }

  server = new WebServer(config->webServerPort);

  server->on("/", [this]() {
    server->send(200, "text/plain", "TTGO T-Call v8 running");
  });

  server->on("/api/status", [this]() {
    String payload = String("{\"mode\":\"") + wifiManager->modeName() +
                     String("\",\"ip\":\"") + wifiManager->localIp().toString() +
                     String("\",\"firebase\":") + (firebaseManager->isReady() ? "true" : "false") +
                     String("}");
    server->send(200, "application/json", payload);
  });

  server->on("/docs", [this]() {
    String html = String("<html><body><h2>TTGO T-Call v8</h2><p>API docs will expand here.</p><p>Mode: ") +
                  wifiManager->modeName() + String("</p><p>Firebase: ") +
                  (firebaseManager->isReady() ? "ready" : "not ready") + String("</p></body></html>");
    server->send(200, "text/html", html);
  });

  server->begin();
  return true;
}

void WebDashboard::loop() {
  if (server != nullptr) {
    server->handleClient();
  }
}

String WebDashboard::docsUrl() const {
  if (config == nullptr) {
    return String();
  }
  return String("http://") + wifiManager->localIp().toString() + ":" + String(config->webServerPort) + "/docs";
}
