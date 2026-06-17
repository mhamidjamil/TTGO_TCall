#include "WebDashboard.h"

#include <FS.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include "FirebaseManager.h"
#include "NtfyManager.h"
#include "WiFiManager.h"

namespace {
WebServer *server = nullptr;
}

bool WebDashboard::begin(const V8Config &incomingConfig, WiFiManager &incomingWiFiManager, FirebaseManager &incomingFirebaseManager, NtfyManager &incomingNtfyManager) {
  config = &incomingConfig;
  wifiManager = &incomingWiFiManager;
  firebaseManager = &incomingFirebaseManager;
  ntfyManager = &incomingNtfyManager;

  if (server != nullptr) {
    delete server;
    server = nullptr;
  }

  server = new WebServer(config->webServerPort);

  server->on("/", [this]() {
    server->sendHeader("Location", "/dashboard.html");
    server->send(302, "text/plain", "dashboard");
  });

  server->serveStatic("/dashboard.html", SPIFFS, "/dashboard.html");
  server->serveStatic("/dashboard.css", SPIFFS, "/dashboard.css");
  server->serveStatic("/dashboard.js", SPIFFS, "/dashboard.js");
  server->serveStatic("/version.txt", SPIFFS, "/version.txt");

  server->on("/api/status", [this]() {
    String payload = String("{\"mode\":\"") + wifiManager->modeName() +
                     String("\",\"ip\":\"") + wifiManager->localIp().toString() +
                     String("\",\"firebase\":") + (firebaseManager->isReady() ? "true" : "false") +
                     String("}");
    server->send(200, "application/json", payload);
  });

  server->on("/api/firebase-web-config", [this]() {
    String authDomain = String(config->firebaseProjectId) + ".firebaseapp.com";
    String payload = String("{\"projectId\":\"") + config->firebaseProjectId +
                     String("\",\"apiKey\":\"") + config->firebaseApiKey +
                     String("\",\"authDomain\":\"") + authDomain +
                     String("\",\"databaseURL\":\"") + config->firebaseDatabaseUrl +
                     String("\",\"deviceId\":\"") + config->deviceId +
                     String("\",\"useAnonymous\":") + (config->firebaseUseAnonymous ? "true" : "false") +
                     String("}");
    server->send(200, "application/json", payload);
  });

  server->on("/api/notify-test", HTTP_POST, [this]() {
    bool ok = ntfyManager != nullptr && ntfyManager->test();
    String message = ok ? String("notification sent") : (ntfyManager != nullptr ? ntfyManager->lastError() : String("ntfy manager missing"));
    message.replace("\"", "\\\"");
    String payload = String("{\"ok\":") + (ok ? "true" : "false") +
                     String(",\"message\":\"") + message +
                     String("\"}");
    server->send(ok ? 200 : 500, "application/json", payload);
  });

  server->on("/api/sync-runtime", HTTP_POST, [this]() {
    runtimeSyncRequested = true;
    server->send(202, "application/json", "{\"ok\":true,\"message\":\"runtime sync queued\"}");
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
<li><code>/ttgo_tcall/settings/runtime</code> - runtime flags such as <code>jobLogs</code>, push logs, DHT interval, and SMS limits</li>
<li><code>/ttgo_tcall/counters</code> - daily/weekly/monthly SMS counters</li>
<li><code>/ttgo_tcall/status</code> - device status snapshot</li>
<li><code>/ttgo_tcall/telemetry</code> - temperature/humidity and counters with timestamp</li>
</ul>

<h2>Firestore gateway path layout</h2>
<ul>
<li><code>sim_module/settings</code> - single-device gateway settings and heartbeat fields</li>
<li><code>sim_module/settings/allowed_numbers</code> - outgoing permission and per-number quotas</li>
<li><code>sim_module/settings/sms_jobs</code> - pending, processing, sent, failed, blocked, or quota_exceeded SMS work</li>
<li><code>sim_module/settings/call_jobs</code> - pending, processing, completed, failed, blocked, quota_exceeded, or user_picked call work</li>
<li><code>sim_module/settings/sms_logs</code> and <code>sim_module/settings/call_logs</code> - audit trail</li>
</ul>

<h2>SMS job format for Rails or another server</h2>
<pre>{
  "device_id": "device_001",
  "phone_number": "+923001234567",
  "message": "hello",
  "status": "pending",
  "created_at": "server timestamp",
  "processing_started_at": null,
  "completed_at": null,
  "error": null
}</pre>

<h2>Call job format for Rails or another server</h2>
<pre>{
  "device_id": "device_001",
  "phone_number": "+923001234567",
  "status": "pending",
  "created_at": "server timestamp",
  "processing_started_at": null,
  "completed_at": null,
  "user_picked": false,
  "duration_seconds": 0,
  "error": null
}</pre>

<h2>Outgoing policy</h2>
<ul>
<li>Before queueing a job, add the target number under <code>sim_module/settings/allowed_numbers/{safePhoneNumber}</code> with <code>enabled = true</code>.</li>
<li>If a job ends with <code>blocked</code> and <code>number_not_allowed</code>, allow the number and retry the job.</li>
<li>Daily SMS and call quotas are counted from Firestore logs for the current UTC day.</li>
<li>Set <code>ttgo_tcall/settings/runtime/jobLogs</code> to <code>true</code> to print serial <code>[JOB]</code> lines for claims, validation, quota checks, send/dial attempts, and final status.</li>
</ul>

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
<li>The dashboard can request a runtime settings sync with the Sync Device Settings button.</li>
<li>The main loop uses one polling cycle for SMS and call queues.</li>
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

bool WebDashboard::consumeRuntimeSyncRequest() {
  if (!runtimeSyncRequested) {
    return false;
  }
  runtimeSyncRequested = false;
  return true;
}

String WebDashboard::docsUrl() const {
  if (config == nullptr) {
    return String();
  }
  return String("http://") + wifiManager->localIp().toString() + ":" + String(config->webServerPort) + "/docs";
}
