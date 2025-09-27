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

<h2>Why auth failed with code 400</h2>
<p>The error body <code>ADMIN_ONLY_OPERATION</code> usually means anonymous sign-in is disabled or blocked for the project.</p>
<ul>
<li>If you want the current firmware default to work, enable <b>Anonymous</b> auth in Firebase Authentication.</li>
<li>If anonymous auth is not allowed, create a dedicated Firebase Auth user and set email/password in the local <code>secrets.h</code>.</li>
<li>Use the exact RTDB URL for the project.</li>
</ul>

<h2>Realtime Database path layout</h2>
<ul>
<li><code>/ttgo_tcall/commands/pending</code> - incoming commands for the ESP32</li>
<li><code>/ttgo_tcall/commands/history</code> - processed command archive</li>
<li><code>/ttgo_tcall/counters</code> - daily/weekly/monthly SMS counters</li>
<li><code>/ttgo_tcall/status</code> - device status snapshot</li>
<li><code>/ttgo_tcall/telemetry</code> - temperature/humidity and counters with timestamp</li>
</ul>

<h2>What a pending SMS command should look like</h2>
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

<h2>Checklist to make Firebase work</h2>
<ol>
<li>Enable Realtime Database.</li>
<li>Enable Firebase Authentication.</li>
<li>Enable Anonymous sign-in or create a device user.</li>
<li>Use the project API key and RTDB URL from the same project.</li>
<li>Make sure the device can reach the internet from WiFi STA mode.</li>
<li>Check the serial log for the full auth error body if it still fails.</li>
</ol>

<h2>Local status</h2>
<p>This device reports temperature, humidity, sent-today, week, and month counts on the OLED and uploads telemetry periodically when Firebase is ready.</p>
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
