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
    String html = R"rawliteral(
<html><head><meta charset="utf-8"><title>TTGO T-Call v8 Docs</title>
<style>
body{font-family:Arial,sans-serif;line-height:1.5;max-width:900px;margin:20px auto;padding:0 16px;color:#111}
code,pre{background:#f4f4f4;padding:2px 4px;border-radius:4px}
pre{padding:12px;overflow:auto}
h1,h2{margin-top:24px}
.ok{color:#0a7a0a}.warn{color:#b26a00}.box{border:1px solid #ddd;border-radius:8px;padding:12px;margin:12px 0;background:#fafafa}
</style></head><body>
<h1>TTGO T-Call v8</h1>
<div class="box">
<p><b>Mode:</b> )rawliteral" + wifiManager->modeName() + R"rawliteral(</p>
<p><b>Firebase:</b> )rawliteral" + String(firebaseManager->isReady() ? "<span class='ok'>ready</span>" : "<span class='warn'>not ready</span>") + R"rawliteral(</p>
<p><b>Docs URL:</b> this page</p>
</div>

<h2>How this ESP32 connects to Firebase</h2>
<ul>
<li>The ESP32 connects to WiFi in <b>STA</b> mode first. STA means it joins your router as a client.</li>
<li>If STA fails, the board can fall back to AP mode and host its own hotspot.</li>
<li>The device uses <b>Firebase Authentication</b> to get an ID token, then uses the Realtime Database REST API with that token.</li>
<li><b>Service account JSON is server-side only</b>. It is not used on the ESP32.</li>
</ul>

<h2>Firebase setup</h2>
<ol>
<li>Enable Realtime Database.</li>
<li>Enable Authentication.</li>
<li>Enable Anonymous sign-in or set device email/password in <code>secrets.h</code>.</li>
<li>Use the exact RTDB URL from the same Firebase project as the API key.</li>
<li>Allow the device to read/write the <code>ttgo_tcall</code> subtree during development.</li>
</ol>

<h2>Realtime Database path layout</h2>
<ul>
<li><code>/ttgo_tcall/commands/pending</code> - incoming commands for the ESP32</li>
<li><code>/ttgo_tcall/commands/history</code> - processed command archive</li>
<li><code>/ttgo_tcall/counters</code> - daily/weekly/monthly SMS counters</li>
<li><code>/ttgo_tcall/status</code> - device status snapshot</li>
<li><code>/ttgo_tcall/telemetry</code> - temperature/humidity and counters with timestamp</li>
</ul>

<h2>Pending SMS format for the server team</h2>
<pre>{
  "type": "sms",
  "number": "+923001234567",
  "message": "hello",
  "status": "pending",
  "createdAtMs": 1712345678000
}</pre>

<h2>Recommended dev rules</h2>
<pre>{
  "rules": {
    ".read": "auth != null",
    ".write": "auth != null"
  }
}</pre>

<h2>On boot</h2>
<ul>
<li>The device first restores counters from Firebase.</li>
<li>It waits 60 seconds before polling pending SMS.</li>
<li>Telemetry includes temperature, humidity, counts, and timestamp.</li>
</ul>
</body></html>
)rawliteral";
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
