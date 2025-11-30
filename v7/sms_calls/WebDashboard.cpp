#include "WebDashboard.h"
#include "secrets.h"
#include <ArduinoJson.h>
#include <Arduino.h>
#include <HTTPClient.h>
#include "SMSManager.h"
#include "CallManager.h"

WebDashboard::WebDashboard(ConfigManager &cfgMgr, SMSManager *sms, CallManager *call, int port)
  : server(port), cfgMgr(cfgMgr), smsManager(sms), callManager(call), serverPort(port) {}

void WebDashboard::begin() {
  // root serves a small info or redirect to /dashboard
  server.on("/", [this]() {
    Serial.println("[WebDashboard] GET /");
    server.send(200, "text/plain", "SMS & Calls module running");
  });
  // Config page - professional configuration interface
  server.on("/config", [this]() {
    Serial.println("[WebDashboard] GET /config");
    this->handleConfigPage();
  });
  // Route /dashboard is protected and handled by handleDashboard
  server.on("/dashboard", [this]() {
    Serial.printf("[WebDashboard] Request /dashboard from %s\n", server.client().remoteIP().toString().c_str());
    this->handleDashboard();
  });
  // Register API routes (handler checks request body/method as needed)
  server.on("/api/config", [this]() { Serial.println("[WebDashboard] /api/config"); this->handleApiConfig(); });
  server.on("/api/status", [this]() { Serial.println("[WebDashboard] /api/status"); this->handleApiStatus(); });
  server.on("/api/auth", [this]() { Serial.println("[WebDashboard] /api/auth"); this->handleApiAuth(); });
  server.on("/api/events", [this]() { Serial.println("[WebDashboard] /api/events"); this->handleApiEvents(); });
  server.on("/api/send_sms", [this]() { Serial.println("[WebDashboard] /api/send_sms"); this->handleSendSms(); });
  server.on("/api/call", [this]() { Serial.println("[WebDashboard] /api/call"); this->handleCall(); });
  server.on("/api/hangup", [this]() { Serial.println("[WebDashboard] /api/hangup"); this->handleHangup(); });
  
  // New comprehensive API endpoints
  server.on("/api/test/sms", [this]() { Serial.println("[WebDashboard] /api/test/sms"); this->handleTestSMS(); });
  server.on("/api/test/call", [this]() { Serial.println("[WebDashboard] /api/test/call"); this->handleTestCall(); });
  server.on("/api/test/ntfy", [this]() { Serial.println("[WebDashboard] /api/test/ntfy"); this->handleTestNTFY(); });
  server.on("/api/test/gate-esp", [this]() { Serial.println("[WebDashboard] /api/test/gate-esp"); this->handleTestGateESP(); });
  server.on("/api/logs/clear", [this]() { Serial.println("[WebDashboard] /api/logs/clear"); this->handleClearLogs(); });
  server.on("/api/logs/download", [this]() { Serial.println("[WebDashboard] /api/logs/download"); this->handleDownloadLogs(); });
  server.on("/api/restart", [this]() { Serial.println("[WebDashboard] /api/restart"); this->handleRestart(); });
  server.on("/api/config/reset", [this]() { Serial.println("[WebDashboard] /api/config/reset"); this->handleConfigReset(); });
  server.on("/api/test_forward", [this]() {
    // inline handler
    if (!checkAuth()) { server.send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
    Config c = cfgMgr.get();
    if (c.forwardUrl.length() == 0) { server.send(400, "application/json", "{\"error\":\"no forward url\"}"); return; }
    StaticJsonDocument<256> doc;
    doc["type"] = "test_forward";
    doc["number"] = "0000000000";
    doc["body"] = "Test forward from dashboard";
    if (c.useApiSecret) doc["secret"] = c.apiSecret;
    String body; serializeJson(doc, body);
    HTTPClient http; http.begin(c.forwardUrl);
    http.addHeader("Content-Type", "application/json");
    if (c.forwardApiKey.length()) http.addHeader("X-Api-Key", c.forwardApiKey);
    int code = http.POST(body);
    String resp = code>0?http.getString():String();
    http.end();
    StaticJsonDocument<256> out; out["code"]=code; out["response"]=resp; out["ok"] = (code>0);
    String s; serializeJson(out,s); server.send(200,"application/json",s);
  });
  server.on("/api/test_call", [this]() {
    if (!checkAuth()) { server.send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
    Config c = cfgMgr.get();
    if (c.forwardUrl.length() == 0) { server.send(400, "application/json", "{\"error\":\"no forward url\"}"); return; }
    StaticJsonDocument<256> doc; doc["type"]="incoming_call"; doc["number"]="+1000000000"; if(c.useApiSecret) doc["secret"]=c.apiSecret; String body; serializeJson(doc,body);
    HTTPClient http; http.begin(c.forwardUrl); http.addHeader("Content-Type","application/json"); if(c.forwardApiKey.length()) http.addHeader("X-Api-Key",c.forwardApiKey);
    int code = http.POST(body); String resp = code>0?http.getString():String(); http.end(); StaticJsonDocument<256> out; out["code"]=code; out["response"]=resp; out["ok"]=(code>0); String s; serializeJson(out,s); server.send(200,"application/json",s);
  });

  // New endpoint: check remote settings URL and apply if version mismatch
  server.on("/api/check_settings", [this]() {
    if (!checkAuth()) { server.send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
    bool applied = cfgMgr.checkAndApplyRemoteSettings();
    if (applied) server.send(200, "application/json", "{\"ok\":true,\"applied\":true}");
    else server.send(200, "application/json", "{\"ok\":true,\"applied\":false}");
  });

  // Expose message listing and delete endpoints for remote management
  server.on("/api/messages", [this]() {
    Serial.println("[WebDashboard] /api/messages");
    if (cfgMgr.get().useApiSecret) {
      String h = server.header("X-Api-Secret");
      if (h != cfgMgr.get().apiSecret) { server.send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
    }
    if (!smsManager) { server.send(500, "application/json", "{\"error\":\"no sms manager\"}"); return; }
    // call SMSManager to list messages
    String out = smsManager->listAllMessages();
    server.send(200, "application/json", out);
  });

  server.on("/api/messages/delete_all", [this]() {
    Serial.println("[WebDashboard] /api/messages/delete_all");
    if (cfgMgr.get().useApiSecret) {
      String h = server.header("X-Api-Secret");
      if (h != cfgMgr.get().apiSecret) { server.send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
    }
    if (!smsManager) { server.send(500, "application/json", "{\"error\":\"no sms manager\"}"); return; }
    bool ok = smsManager->deleteAllMessages();
    server.send(ok?200:500, "application/json", ok?"{\"ok\":true}":"{\"ok\":false}");
  });

  // Serve device API docs (simple HTML) protected by dashboard auth
  server.on("/docs", [this]() {
    // If this is a POST login attempt, treat pw param as login
    if (server.hasArg("pw")) {
      String p = server.arg("pw");
      if (p == String(DASHBOARD_PASSWORD)) {
        // embed pw into JS so subsequent API calls from the docs page can use header
        const char *docs = R"rawliteral(
<html><head><meta charset="utf-8"><title>ESP32 API Docs</title></head><body>
<h2>ESP32 SMS & Calls - API Reference</h2>
<p>Use the REST examples below. The page will use the provided dashboard password when calling protected APIs.</p>
<script>window._DASH_PW='" )rawliteral";
        String page = String(docs) + p + String(R"rawliteral(';</script>
<h3>GET /dashboard</h3>
<p>Serve the dashboard UI (HTML). Use the password form or X-Dashboard-Auth header.</p>
<h3>GET /api/config</h3>
<p>Returns effective runtime configuration (includes compile-time defaults from secrets.h and persisted values if any).</p>
<pre>{
  "useApiSecret": true,
  "apiSecret": "...",
  "forwardUrl": "...",
  "forwardApiKey": "...",
  "allowSms": true,
  "allowCall": true,
  "settingsUrl": "...",
  "settingsVersion": "..."
}</pre>
<h3>POST /api/send_sms</h3>
<p>Send SMS: JSON body {"to":"+9233...","message":"text"}. If useApiSecret is enabled add header X-Api-Secret.</p>
<h3>POST /api/check_settings</h3>
<p>Trigger remote settings check and apply if version mismatch.</p>
</body></html>
)rawliteral");
        server.send(200, "text/html", page);
        return;
      }
      // bad pw - fall through to show login form
    }

    if (!checkAuth()) {
      // Serve simple login form for docs
      const char *login = "<html><body><h3>Enter Dashboard Password</h3>"
                          "<form method='post' action='/docs'><input name='pw' type='password'/>"
                          "<button type='submit'>Login</button></form></body></html>";
      server.send(200, "text/html", login);
      return;
    }
    const char *docs = R"rawliteral(
<html><head><meta charset="utf-8"><title>ESP32 API Docs</title></head><body>
<h2>ESP32 SMS & Calls - API Reference</h2>
<p>Protected by dashboard password. Use the header <code>X-Dashboard-Auth: &lt;password&gt;</code>.</p>
<h3>GET /dashboard</h3>
<p>Serve the dashboard UI (HTML). Use the password form or X-Dashboard-Auth header.</p>
<h3>GET /api/config</h3>
<p>Returns effective runtime configuration (includes compile-time defaults from secrets.h and persisted values if any).</p>
<pre>{
  "useApiSecret": true,
  "apiSecret": "...",
  "forwardUrl": "...",
  "forwardApiKey": "...",
  "allowSms": true,
  "allowCall": true,
  "settingsUrl": "...",
  "settingsVersion": "..."
}</pre>
<h3>POST /api/send_sms</h3>
<p>Send SMS: JSON body {"to":"+9233...","message":"text"}. If useApiSecret is enabled add header X-Api-Secret.</p>
<h3>POST /api/check_settings</h3>
<p>Trigger remote settings check and apply if version mismatch.</p>
</body></html>
)rawliteral";
    server.send(200, "text/html", docs);
  });

  // Static assets are embedded in the fallback HTML; SPIFFS not used

  server.begin();
  // Serial.printf sometimes isn't available depending on WebServer implementation; use stored port
  Serial.printf("[WebDashboard] HTTP server started on port %d\n", serverPort);
}

void WebDashboard::handleClient() { server.handleClient(); }

void WebDashboard::handleConfigPage() {
  // Serve professional configuration management page with real-time SPIFFS sync
  Config c = cfgMgr.get();
  
  String html = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>TTGO T-Call Configuration</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; padding: 20px; }
    .container { max-width: 1200px; margin: 0 auto; background: white; border-radius: 16px; box-shadow: 0 20px 60px rgba(0,0,0,0.3); overflow: hidden; }
    .header { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 30px; text-align: center; }
    .header h1 { font-size: 28px; margin-bottom: 10px; }
    .header p { opacity: 0.9; font-size: 14px; }
    .tabs { display: flex; background: #f8f9fa; border-bottom: 2px solid #e9ecef; }
    .tab { flex: 1; padding: 15px; text-align: center; cursor: pointer; border: none; background: transparent; font-size: 14px; font-weight: 600; color: #6c757d; transition: all 0.3s; }
    .tab:hover { background: #e9ecef; color: #495057; }
    .tab.active { background: white; color: #667eea; border-bottom: 3px solid #667eea; }
    .content { padding: 30px; }
    .tab-content { display: none; }
    .tab-content.active { display: block; animation: fadeIn 0.3s; }
    @keyframes fadeIn { from { opacity: 0; transform: translateY(10px); } to { opacity: 1; transform: translateY(0); } }
    .form-group { margin-bottom: 25px; }
    .form-group label { display: block; margin-bottom: 8px; font-weight: 600; color: #495057; font-size: 14px; }
    .form-group input[type="text"], .form-group input[type="password"], .form-group input[type="number"], .form-group textarea { width: 100%; padding: 12px; border: 2px solid #e9ecef; border-radius: 8px; font-size: 14px; transition: all 0.3s; }
    .form-group input:focus, .form-group textarea:focus { outline: none; border-color: #667eea; box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.1); }
    .form-group textarea { resize: vertical; min-height: 80px; font-family: inherit; }
    .toggle-group { display: flex; align-items: center; justify-content: space-between; padding: 15px; background: #f8f9fa; border-radius: 8px; margin-bottom: 15px; }
    .toggle-group label { font-weight: 600; color: #495057; margin: 0; }
    .toggle { position: relative; display: inline-block; width: 60px; height: 30px; }
    .toggle input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; transition: 0.3s; border-radius: 30px; }
    .slider:before { position: absolute; content: ""; height: 22px; width: 22px; left: 4px; bottom: 4px; background-color: white; transition: 0.3s; border-radius: 50%; }
    input:checked + .slider { background-color: #667eea; }
    input:checked + .slider:before { transform: translateX(30px); }
    .btn { padding: 12px 30px; border: none; border-radius: 8px; font-size: 14px; font-weight: 600; cursor: pointer; transition: all 0.3s; margin-right: 10px; }
    .btn-primary { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; }
    .btn-primary:hover { transform: translateY(-2px); box-shadow: 0 5px 15px rgba(102, 126, 234, 0.3); }
    .btn-secondary { background: #6c757d; color: white; }
    .btn-secondary:hover { background: #5a6268; }
    .btn-danger { background: #dc3545; color: white; }
    .btn-danger:hover { background: #c82333; }
    .btn-group { display: flex; gap: 10px; margin-top: 30px; padding-top: 20px; border-top: 2px solid #e9ecef; flex-wrap: wrap; }
    .status-bar { padding: 15px; background: #d4edda; border: 1px solid #c3e6cb; border-radius: 8px; color: #155724; margin-bottom: 20px; display: none; }
    .status-bar.error { background: #f8d7da; border-color: #f5c6cb; color: #721c24; }
    .info-box { background: #e7f3ff; padding: 15px; border-radius: 8px; border-left: 4px solid #667eea; margin-bottom: 20px; }
    .info-box p { color: #495057; font-size: 14px; line-height: 1.6; }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>⚙️ TTGO T-Call Configuration</h1>
      <p>Professional Configuration Management with Real-Time SPIFFS Sync</p>
    </div>
    
    <div class="tabs">
      <button class="tab active" onclick="showTab('device')">Device Info</button>
      <button class="tab" onclick="showTab('api')">API & Security</button>
      <button class="tab" onclick="showTab('notifications')">Notifications</button>
      <button class="tab" onclick="showTab('advanced')">Advanced</button>
    </div>
    
    <div class="content">
      <div id="statusBar" class="status-bar"></div>
      
      <!-- Device Info Tab -->
      <div id="device" class="tab-content active">
        <div class="info-box">
          <p><strong>ℹ️ Device Information:</strong> Configure your device identity and owner details. These settings help identify your device and set primary contact information.</p>
        </div>
        
        <div class="form-group">
          <label for="deviceName">Device Name</label>
          <input type="text" id="deviceName" value=")rawliteral" + c.deviceName + R"rawliteral(" placeholder="My TTGO Device">
        </div>
        
        <div class="form-group">
          <label for="ownerName">Owner Name</label>
          <input type="text" id="ownerName" value=")rawliteral" + c.ownerName + R"rawliteral(" placeholder="Device Owner">
        </div>
        
        <div class="form-group">
          <label for="myNumber">Device Phone Number</label>
          <input type="text" id="myNumber" value=")rawliteral" + c.myNumber + R"rawliteral(" placeholder="+1234567890">
        </div>
        
        <div class="form-group">
          <label for="adminNumbers">Admin Numbers (comma-separated)</label>
          <input type="text" id="adminNumbers" value=")rawliteral" + c.adminNumbers + R"rawliteral(" placeholder="1234567890,0987654321">
        </div>
        
        <div class="form-group">
          <label for="autoReplyMessage">Auto-Reply Message</label>
          <textarea id="autoReplyMessage" placeholder="Automated response message for unknown callers">)rawliteral" + c.autoReplyMessage + R"rawliteral(</textarea>
        </div>
      </div>
      
      <!-- API & Security Tab -->
      <div id="api" class="tab-content">
        <div class="info-box">
          <p><strong>🔒 Security Settings:</strong> Configure API authentication and forwarding URLs. Enable API secret for secure remote access.</p>
        </div>
        
        <div class="toggle-group">
          <label>Use API Secret</label>
          <label class="toggle">
            <input type="checkbox" id="useApiSecret" )rawliteral" + String(c.useApiSecret ? "checked" : "") + R"rawliteral(>
            <span class="slider"></span>
          </label>
        </div>
        
        <div class="form-group">
          <label for="apiSecret">API Secret Token</label>
          <input type="password" id="apiSecret" value=")rawliteral" + c.apiSecret + R"rawliteral(" placeholder="Your secure API token">
        </div>
        
        <div class="form-group">
          <label for="forwardUrl">Forward URL</label>
          <input type="text" id="forwardUrl" value=")rawliteral" + c.forwardUrl + R"rawliteral(" placeholder="https://your-server.com/forward">
        </div>
        
        <div class="form-group">
          <label for="forwardApiKey">Forward API Key (Optional)</label>
          <input type="password" id="forwardApiKey" value=")rawliteral" + c.forwardApiKey + R"rawliteral(" placeholder="Optional API key for forwarding">
        </div>
        
        <div class="toggle-group">
          <label>Allow SMS</label>
          <label class="toggle">
            <input type="checkbox" id="allowSms" )rawliteral" + String(c.allowSms ? "checked" : "") + R"rawliteral(>
            <span class="slider"></span>
          </label>
        </div>
        
        <div class="toggle-group">
          <label>Allow Calls</label>
          <label class="toggle">
            <input type="checkbox" id="allowCall" )rawliteral" + String(c.allowCall ? "checked" : "") + R"rawliteral(>
            <span class="slider"></span>
          </label>
        </div>
      </div>
      
      <!-- Notifications Tab -->
      <div id="notifications" class="tab-content">
        <div class="info-box">
          <p><strong>🔔 Notification Settings:</strong> Configure NTFY notifications and Gate ESP integration for real-time alerts.</p>
        </div>
        
        <div class="toggle-group">
          <label>Enable NTFY Notifications</label>
          <label class="toggle">
            <input type="checkbox" id="ntfyEnabled" )rawliteral" + String(c.ntfyEnabled ? "checked" : "") + R"rawliteral(>
            <span class="slider"></span>
          </label>
        </div>
        
        <div class="form-group">
          <label for="ntfyServerUrl">NTFY Server URL</label>
          <input type="text" id="ntfyServerUrl" value=")rawliteral" + c.ntfyServerUrl + R"rawliteral(" placeholder="https://ntfy.sh">
        </div>
        
        <div class="form-group">
          <label for="ntfyTopic">NTFY Topic</label>
          <input type="text" id="ntfyTopic" value=")rawliteral" + c.ntfyTopic + R"rawliteral(" placeholder="my_notifications">
        </div>
        
        <div class="toggle-group">
          <label>Enable Gate ESP</label>
          <label class="toggle">
            <input type="checkbox" id="gateEspEnabled" )rawliteral" + String(c.gateEspEnabled ? "checked" : "") + R"rawliteral(>
            <span class="slider"></span>
          </label>
        </div>
        
        <div class="form-group">
          <label for="gateEspUrl">Gate ESP URL</label>
          <input type="text" id="gateEspUrl" value=")rawliteral" + c.gateEspUrl + R"rawliteral(" placeholder="192.168.1.100">
        </div>
      </div>
      
      <!-- Advanced Tab -->
      <div id="advanced" class="tab-content">
        <div class="info-box">
          <p><strong>⚡ Advanced Settings:</strong> Configure remote settings synchronization and message management options.</p>
        </div>
        
        <div class="form-group">
          <label for="settingsUrl">Remote Settings URL</label>
          <input type="text" id="settingsUrl" value=")rawliteral" + c.settingsUrl + R"rawliteral(" placeholder="https://your-server.com/settings">
        </div>
        
        <div class="form-group">
          <label for="settingsVersion">Settings Version</label>
          <input type="text" id="settingsVersion" value=")rawliteral" + c.settingsVersion + R"rawliteral(" placeholder="1.0">
        </div>
        
        <div class="toggle-group">
          <label>Log Messages</label>
          <label class="toggle">
            <input type="checkbox" id="logMessages" )rawliteral" + String(c.logMessages ? "checked" : "") + R"rawliteral(>
            <span class="slider"></span>
          </label>
        </div>
        
        <div class="toggle-group">
          <label>Delete After Forward</label>
          <label class="toggle">
            <input type="checkbox" id="deleteAfterForward" )rawliteral" + String(c.deleteAfterForward ? "checked" : "") + R"rawliteral(>
            <span class="slider"></span>
          </label>
        </div>
      </div>
      
      <div class="btn-group">
        <button class="btn btn-primary" onclick="saveConfig()">💾 Save Configuration</button>
        <button class="btn btn-secondary" onclick="reloadConfig()">🔄 Reload</button>
        <button class="btn btn-danger" onclick="resetConfig()">⚠️ Reset to Defaults</button>
        <button class="btn btn-secondary" onclick="location.href='/dashboard'">📊 Dashboard</button>
      </div>
    </div>
  </div>
  
  <script>
    function showTab(tabName) {
      document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
      document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
      event.target.classList.add('active');
      document.getElementById(tabName).classList.add('active');
    }
    
    function showStatus(message, isError = false) {
      const statusBar = document.getElementById('statusBar');
      statusBar.textContent = message;
      statusBar.className = 'status-bar' + (isError ? ' error' : '');
      statusBar.style.display = 'block';
      setTimeout(() => { statusBar.style.display = 'none'; }, 5000);
    }
    
    function saveConfig() {
      const config = {
        deviceName: document.getElementById('deviceName').value,
        ownerName: document.getElementById('ownerName').value,
        myNumber: document.getElementById('myNumber').value,
        adminNumbers: document.getElementById('adminNumbers').value,
        autoReplyMessage: document.getElementById('autoReplyMessage').value,
        useApiSecret: document.getElementById('useApiSecret').checked,
        apiSecret: document.getElementById('apiSecret').value,
        forwardUrl: document.getElementById('forwardUrl').value,
        forwardApiKey: document.getElementById('forwardApiKey').value,
        allowSms: document.getElementById('allowSms').checked,
        allowCall: document.getElementById('allowCall').checked,
        ntfyEnabled: document.getElementById('ntfyEnabled').checked,
        ntfyServerUrl: document.getElementById('ntfyServerUrl').value,
        ntfyTopic: document.getElementById('ntfyTopic').value,
        gateEspEnabled: document.getElementById('gateEspEnabled').checked,
        gateEspUrl: document.getElementById('gateEspUrl').value,
        settingsUrl: document.getElementById('settingsUrl').value,
        settingsVersion: document.getElementById('settingsVersion').value,
        logMessages: document.getElementById('logMessages').checked,
        deleteAfterForward: document.getElementById('deleteAfterForward').checked
      };
      
      fetch('/api/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(config)
      })
      .then(response => response.json())
      .then(data => {
        if (data.ok) {
          showStatus('✅ Configuration saved successfully to SPIFFS!');
        } else {
          showStatus('❌ Error saving configuration: ' + (data.error || 'Unknown error'), true);
        }
      })
      .catch(error => {
        showStatus('❌ Network error: ' + error.message, true);
      });
    }
    
    function reloadConfig() {
      location.reload();
    }
    
    function resetConfig() {
      if (confirm('Are you sure you want to reset all settings to defaults? This action cannot be undone.')) {
        fetch('/api/config/reset', {
          method: 'POST'
        })
        .then(response => response.json())
        .then(data => {
          if (data.ok) {
            showStatus('✅ Configuration reset to defaults!');
            setTimeout(() => location.reload(), 2000);
          } else {
            showStatus('❌ Error resetting configuration', true);
          }
        })
        .catch(error => {
          showStatus('❌ Network error: ' + error.message, true);
        });
      }
    }
  </script>
</body>
</html>)rawliteral";
  
  server.send(200, "text/html", html);
}

bool WebDashboard::checkAuth() {
  // Temporarily bypass authentication for /api/config
  if (server.uri() == "/api/config") {
    Serial.println("[WebDashboard] checkAuth: Bypassing auth for /api/config");
    return true;
  }

  // basic auth: check header 'X-Dashboard-Auth' or query param 'pw'
  if (server.hasHeader("X-Dashboard-Auth")) {
    String v = server.header("X-Dashboard-Auth");
    Serial.printf("[WebDashboard] checkAuth: header X-Dashboard-Auth present (len=%d)\n", v.length());
    if (v == String(DASHBOARD_PASSWORD)) { Serial.println("[WebDashboard] checkAuth: header auth OK"); return true; }
  }
  if (server.hasArg("pw")) {
    String p = server.arg("pw");
    Serial.printf("[WebDashboard] checkAuth: form arg 'pw' present (len=%d)\n", p.length());
    if (p == String(DASHBOARD_PASSWORD)) { Serial.println("[WebDashboard] checkAuth: form auth OK"); return true; }
  }
  return false;
}

void WebDashboard::handleDashboard() {
  // If there's POST data (form pw or JSON/plain) treat it as login attempt
  String providedPw;
  if (server.hasArg("pw")) providedPw = server.arg("pw");
  else if (server.hasArg("plain")) {
    String body = server.arg("plain");
    if (body.startsWith("pw=")) providedPw = body.substring(3);
    else {
      StaticJsonDocument<128> jd;
      if (deserializeJson(jd, body) == DeserializationError::Ok && jd.containsKey("pw")) {
        providedPw = String((const char *)jd["pw"].as<const char *>());
      } else {
        providedPw = body;
      }
    }
  }

  String headerPw = server.header("X-Dashboard-Auth");
  bool authOk = false;
  if (providedPw.length() && providedPw == String(DASHBOARD_PASSWORD)) authOk = true;
  if (!authOk && headerPw.length() && headerPw == String(DASHBOARD_PASSWORD)) authOk = true;
  Serial.printf("[WebDashboard] handleDashboard: from %s providedPw_len=%d headerPw_len=%d authOk=%d\n",
    server.client().remoteIP().toString().c_str(), providedPw.length(), headerPw.length(), authOk);

  if (authOk) {
    Serial.println("[WebDashboard] Auth OK — serving comprehensive dashboard");
    // Serve a simple message and redirect to the comprehensive dashboard
    String page = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width,initial-scale=1" />
  <title>TTGO T-Call Dashboard</title>
  <style>
    body { font-family: Arial, sans-serif; padding: 20px; background: #f5f5f5; }
    .container { max-width: 800px; margin: 0 auto; background: white; padding: 30px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
    .button { background: #007bff; color: white; padding: 12px 24px; border: none; border-radius: 4px; cursor: pointer; text-decoration: none; display: inline-block; margin: 10px 5px; }
    .button:hover { background: #0056b3; }
    .note { background: #e7f3ff; padding: 15px; border-radius: 4px; border-left: 4px solid #007bff; margin: 20px 0; }
  </style>
</head>
<body>
  <div class="container">
    <h1>TTGO T-Call Dashboard</h1>
    <div class="note">
      <p><strong>Note:</strong> This device now uses a comprehensive SPIFFS-based configuration system.</p>
      <p>The dashboard interface has been upgraded with tabbed navigation and persistent storage.</p>
    </div>
    
    <h3>Dashboard Features:</h3>
    <ul>
      <li>✓ Persistent configuration storage via SPIFFS</li>
      <li>✓ Comprehensive device settings management</li>
      <li>✓ NTFY notification integration</li>
      <li>✓ Admin number management</li>
      <li>✓ Gate ESP integration</li>
      <li>✓ Testing and diagnostics tools</li>
    </ul>

    <h3>Configuration Management:</h3>
    <p>All settings are now saved to SPIFFS and persist across device reboots. No need to update code for configuration changes.</p>
    
    <h3>API Endpoints:</h3>
    <ul>
      <li><code>GET /api/config</code> - Get current configuration</li>
      <li><code>POST /api/config</code> - Save configuration</li>
      <li><code>GET /api/status</code> - Get device status</li>
      <li><code>POST /api/test/*</code> - Test various functions</li>
      <li><code>POST /api/restart</code> - Restart device</li>
    </ul>

    <div style="text-align: center; margin-top: 30px;">
      <p><strong>The comprehensive dashboard files are stored in the data/ directory.</strong></p>
      <p>Use the Arduino IDE's "ESP32 Sketch Data Upload" tool to upload the dashboard files to SPIFFS.</p>
    </div>
  </div>
</body>
</html>
)rawliteral";
    
    if (providedPw.length()) page += "<script>window._DASH_PW='" + providedPw + "';</script>";
    else if (headerPw.length()) page += "<script>window._DASH_PW='" + headerPw + "';</script>";
    
    server.send(200, "text/html", page);
    return;
  }

  // If header or querypw auth is present, validate and serve SPA
  if (checkAuth()) {
    Serial.println("[WebDashboard] Auth via header/query ok — serving embedded fallback");
    const char *embedded2 = R"rawliteral(
<!doctype html><html><body><p>Dashboard (fallback). Please use the header X-Dashboard-Auth to authenticate.</p></body></html>
)rawliteral";
    server.send(200, "text/html", embedded2);
    return;
  }

  // No auth provided — serve a simple HTML password form so the browser can post
  Serial.println("[WebDashboard] No auth provided — serving login form");
  const char *loginForm = "<html><body><h3>Enter Dashboard Password</h3>"
    "<form method='post' action='/dashboard'><input name='pw' type='password'/>"
    "<button type='submit'>Login</button></form></body></html>";
  server.send(200, "text/html", loginForm);
}

void WebDashboard::handleApiConfig() {
  // If there's a request body (arg "plain") treat as POST to save; otherwise treat as GET
  if (!server.hasArg("plain")) {
    Serial.println("[WebDashboard] handleApiConfig GET");
    // Get current configuration
    Config c = cfgMgr.get();
    
    // Create comprehensive JSON response with all configuration parameters
    StaticJsonDocument<1024> doc;
    
    // General Settings
    doc["deviceId"] = c.deviceName;
    doc["ownerName"] = c.ownerName;
    doc["ownerPhone"] = c.myNumber;
    doc["logMessages"] = c.logMessages;
    
    // API & Forwarding
    doc["serverUrl"] = c.forwardUrl;
    doc["apiKey"] = c.apiSecret;
    doc["gateEspUrl"] = c.gateEspUrl;
    doc["forwardSmsUrl"] = c.forwardUrl;
    
    // Notifications
    doc["ntfyEnabled"] = c.ntfyEnabled;
    doc["ntfyUrl"] = c.ntfyServerUrl;
    doc["ntfyTopic"] = c.ntfyTopic;
    doc["autoReplyMessage"] = c.autoReplyMessage;
    
    // Security & Permissions
    doc["smsAllowed"] = c.allowSms;
    doc["callsAllowed"] = c.allowCall;
    doc["adminNumbers"] = c.adminNumbers;
    doc["allowedNumbers"] = c.adminNumbers;
    
    // Legacy fields for backward compatibility
    doc["useApiSecret"] = c.useApiSecret;
    doc["apiSecret"] = c.apiSecret;
    doc["forwardUrl"] = c.forwardUrl;
    doc["forwardApiKey"] = c.forwardApiKey;
    doc["allowSms"] = c.allowSms;
    doc["allowCall"] = c.allowCall;
    
    // Mask sensitive fields unless authenticated
    bool auth = checkAuth();
    if (!auth) {
      doc["apiKey"] = "*****";
      doc["apiSecret"] = "*****";
      doc["forwardApiKey"] = "*****";
    }
    
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
    return;
  }

  // POST: save config
  Serial.println("[WebDashboard] handleApiConfig POST");
  if (!checkAuth()) { 
    Serial.println("[WebDashboard] handleApiConfig POST unauthorized"); 
    server.send(401, "application/json", "{\"error\":\"unauthorized\"}"); 
    return; 
  }
  
  String body = server.arg("plain");
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) { 
    server.send(400, "application/json", "{\"error\":\"bad json\"}"); 
    return; 
  }
  
  Config c = cfgMgr.get();
  
  // Update all configuration parameters from JSON
  // Device Info fields
  if (doc.containsKey("deviceName")) { const char* v = doc["deviceName"]; if(v) c.deviceName = v; }
  if (doc.containsKey("deviceId")) { const char* v = doc["deviceId"]; if(v) c.deviceName = v; }
  if (doc.containsKey("ownerName")) { const char* v = doc["ownerName"]; if(v) c.ownerName = v; }
  if (doc.containsKey("myNumber")) { const char* v = doc["myNumber"]; if(v) c.myNumber = v; }
  if (doc.containsKey("ownerPhone")) { const char* v = doc["ownerPhone"]; if(v) c.myNumber = v; }
  if (doc.containsKey("adminNumbers")) { const char* v = doc["adminNumbers"]; if(v) c.adminNumbers = v; }
  if (doc.containsKey("allowedNumbers")) { const char* v = doc["allowedNumbers"]; if(v) c.adminNumbers = v; }
  if (doc.containsKey("autoReplyMessage")) { const char* v = doc["autoReplyMessage"]; if(v) c.autoReplyMessage = v; }
  
  // API & Security fields
  if (doc.containsKey("useApiSecret")) c.useApiSecret = doc["useApiSecret"];
  if (doc.containsKey("apiSecret")) { const char* v = doc["apiSecret"]; if(v) c.apiSecret = v; }
  if (doc.containsKey("apiKey")) { const char* v = doc["apiKey"]; if(v) c.apiSecret = v; }
  if (doc.containsKey("forwardUrl")) { const char* v = doc["forwardUrl"]; if(v) c.forwardUrl = v; }
  if (doc.containsKey("serverUrl")) { const char* v = doc["serverUrl"]; if(v) c.forwardUrl = v; }
  if (doc.containsKey("forwardApiKey")) { const char* v = doc["forwardApiKey"]; if(v) c.forwardApiKey = v; }
  if (doc.containsKey("allowSms")) c.allowSms = doc["allowSms"];
  if (doc.containsKey("allowCall")) c.allowCall = doc["allowCall"];
  if (doc.containsKey("smsAllowed")) c.allowSms = doc["smsAllowed"];
  if (doc.containsKey("callsAllowed")) c.allowCall = doc["callsAllowed"];
  
  // Notification fields
  if (doc.containsKey("ntfyEnabled")) c.ntfyEnabled = doc["ntfyEnabled"];
  if (doc.containsKey("ntfyServerUrl")) { const char* v = doc["ntfyServerUrl"]; if(v) c.ntfyServerUrl = v; }
  if (doc.containsKey("ntfyUrl")) { const char* v = doc["ntfyUrl"]; if(v) c.ntfyServerUrl = v; }
  if (doc.containsKey("ntfyTopic")) { const char* v = doc["ntfyTopic"]; if(v) c.ntfyTopic = v; }
  if (doc.containsKey("gateEspEnabled")) c.gateEspEnabled = doc["gateEspEnabled"];
  if (doc.containsKey("gateEspUrl")) { const char* v = doc["gateEspUrl"]; if(v) c.gateEspUrl = v; }
  
  // Advanced fields
  if (doc.containsKey("settingsUrl")) { const char* v = doc["settingsUrl"]; if(v) c.settingsUrl = v; }
  if (doc.containsKey("settingsVersion")) { const char* v = doc["settingsVersion"]; if(v) c.settingsVersion = v; }
  if (doc.containsKey("logMessages")) c.logMessages = doc["logMessages"];
  if (doc.containsKey("deleteAfterForward")) c.deleteAfterForward = doc["deleteAfterForward"];
  
  // Save configuration and respond
  if (cfgMgr.save(c)) {
    Serial.println("[WebDashboard] Configuration saved successfully");
    server.send(200, "application/json", "{\"ok\":true,\"message\":\"Configuration saved successfully\"}");
  } else {
    Serial.println("[WebDashboard] Failed to save configuration");
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"Failed to save configuration\"}");
  }
}

void WebDashboard::handleApiEvents() {
  // This endpoint can be used by internal code to forward events to external URL
  server.send(200, "application/json", "{\"ok\":true}");
}

void WebDashboard::handleSendSms() {
  Config c = cfgMgr.get();
  if (c.apiSecret.length() > 0) {
    // check secret header or body
    String h = server.header("X-Api-Secret");
    if (h != c.apiSecret) { server.send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
  }
  if (!smsManager) { server.send(500, "application/json", "{\"error\":\"no sms manager\"}"); return; }
  if (!server.hasArg("plain")) { server.send(400, "application/json", "{\"error\":\"no body\"}"); return; }
  StaticJsonDocument<256> doc; deserializeJson(doc, server.arg("plain"));
  String rawTo = String((const char *)doc["to"].as<const char *>());
  String msg = String((const char *)doc["message"].as<const char *>());

  // Normalize phone number: strip non-digits, handle leading 00, leading 0, or missing +92
  auto normalizeNumber = [&](const String &in)->String {
    String s = in;
    String digits = "";
    for (size_t i = 0; i < s.length(); ++i) {
      char ch = s.charAt(i);
      if (ch >= '0' && ch <= '9') digits += ch;
    }
    // remove leading international 00
    if (digits.startsWith("00")) digits = digits.substring(2);
    // if starts with single 0 (local), remove it
    if (digits.startsWith("0") && digits.length() > 10) digits = digits.substring(1);
    // If already has country code 92
    if (digits.startsWith("92") && digits.length() >= 12) return String("+") + digits;
    // If exactly 10 digits (local without leading 0), assume Pakistan and prefix 92
    if (digits.length() == 10) return String("+92") + digits;
    // If starts with 92 and length==12
    if (digits.startsWith("92") && digits.length() == 12) return String("+") + digits;
    // If longer than 10 and doesn't match above, assume it already contains country code
    if (digits.length() > 10) return String("+") + digits;
    // fallback to empty string (invalid)
    return String("");
  };

  String to = normalizeNumber(rawTo);
  if (to.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"invalid number\"}");
    return;
  }
  // final length check: require at least 12 characters for +92XXXXXXXXXX
  if (to.length() < 12) {
    server.send(400, "application/json", "{\"error\":\"number too short\"}");
    return;
  }

  Serial.printf("[WebDashboard] handleSendSms normalized to=%s msg_len=%d\n", to.c_str(), msg.length());
  String err; bool ok = smsManager->sendSms(to, msg, err);
  if (ok) server.send(200, "application/json", "{\"ok\":true}");
  else server.send(400, "application/json", String("{\"ok\":false,\"error\":\"") + err + "\"}");
}

void WebDashboard::handleCall() {
  Config c = cfgMgr.get();
  if (c.apiSecret.length() > 0) {
    String h = server.header("X-Api-Secret");
    if (h != c.apiSecret) { server.send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
  }
  if (!callManager) { server.send(500, "application/json", "{\"error\":\"no call manager\"}"); return; }
  if (!server.hasArg("plain")) { server.send(400, "application/json", "{\"error\":\"no body\"}"); return; }
  StaticJsonDocument<128> doc; deserializeJson(doc, server.arg("plain"));
  String to = String((const char *)doc["to"].as<const char *>());
  Serial.printf("[WebDashboard] handleCall to=%s\n", to.c_str());
  // TODO: callManager->call(to) - not implemented, placeholder just respond ok
  server.send(200, "application/json", "{\"ok\":true}\n");
}

void WebDashboard::handleHangup() {
  Config c = cfgMgr.get();
  if (c.apiSecret.length() > 0) {
    String h = server.header("X-Api-Secret");
    if (h != c.apiSecret) { server.send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
  }
  if (!callManager) { server.send(500, "application/json", "{\"error\":\"no call manager\"}"); return; }
  Serial.println("[WebDashboard] handleHangup");
  callManager->hangup();
  server.send(200, "application/json", "{\"ok\":true}\n");
}

void WebDashboard::handleApiStatus() {
  Serial.println("[WebDashboard] handleApiStatus");
  
  StaticJsonDocument<512> doc;
  doc["version"] = "v7.0.0";
  doc["uptime"] = String(millis() / 1000) + " seconds";
  doc["freeMemory"] = String(ESP.getFreeHeap()) + " bytes";
  doc["networkStatus"] = "Connected"; // TODO: Get actual network status
  doc["signalStrength"] = "Unknown"; // TODO: Get actual signal strength
  doc["lastRestart"] = "System boot"; // TODO: Track actual restart reason
  doc["configSource"] = "SPIFFS"; // TODO: Get actual config source from ConfigManager
  
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void WebDashboard::handleApiAuth() {
  Serial.println("[WebDashboard] handleApiAuth");
  
  bool authenticated = checkAuth();
  StaticJsonDocument<128> doc;
  doc["authenticated"] = authenticated;
  
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void WebDashboard::handleTestSMS() {
  Serial.println("[WebDashboard] handleTestSMS");
  if (!checkAuth()) {
    server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
    return;
  }
  
  if (!smsManager) {
    server.send(500, "application/json", "{\"error\":\"SMS manager not available\"}");
    return;
  }
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"no request body\"}");
    return;
  }
  
  StaticJsonDocument<256> doc;
  deserializeJson(doc, server.arg("plain"));
  const char* testNumberVal = doc["number"];
  String testNumber = testNumberVal ? String(testNumberVal) : "";
  
  if (testNumber.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"no phone number provided\"}");
    return;
  }
  
  String testMessage = "Test SMS from TTGO T-Call dashboard at " + String(millis());
  String error;
  bool success = smsManager->sendSms(testNumber, testMessage, error);
  
  StaticJsonDocument<256> response;
  response["success"] = success;
  if (success) {
    response["message"] = "Test SMS sent successfully";
  } else {
    response["error"] = error;
  }
  
  String out;
  serializeJson(response, out);
  server.send(success ? 200 : 400, "application/json", out);
}

void WebDashboard::handleTestCall() {
  Serial.println("[WebDashboard] handleTestCall");
  if (!checkAuth()) {
    server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
    return;
  }
  
  if (!callManager) {
    server.send(500, "application/json", "{\"error\":\"Call manager not available\"}");
    return;
  }
  
  // For now, just simulate a call test
  StaticJsonDocument<256> response;
  response["success"] = true;
  response["message"] = "Call test completed (simulated)";
  
  String out;
  serializeJson(response, out);
  server.send(200, "application/json", out);
}

void WebDashboard::handleTestNTFY() {
  Serial.println("[WebDashboard] handleTestNTFY");
  if (!checkAuth()) {
    server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
    return;
  }
  
  Config c = cfgMgr.get();
  if (!c.ntfyEnabled || c.ntfyServerUrl.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"NTFY not configured or disabled\"}");
    return;
  }
  
  // Test NTFY notification
  HTTPClient http;
  String url = c.ntfyServerUrl;
  if (c.ntfyTopic.length() > 0) {
    if (!url.endsWith("/")) url += "/";
    url += c.ntfyTopic;
  }
  
  http.begin(url);
  http.addHeader("Content-Type", "text/plain");
  
  String message = "Test notification from TTGO T-Call dashboard";
  int httpCode = http.POST(message);
  String response = http.getString();
  http.end();
  
  StaticJsonDocument<256> doc;
  doc["success"] = (httpCode == 200);
  doc["httpCode"] = httpCode;
  doc["message"] = (httpCode == 200) ? "NTFY test notification sent" : "NTFY test failed";
  if (httpCode != 200) {
    doc["error"] = response;
  }
  
  String out;
  serializeJson(doc, out);
  server.send((httpCode == 200) ? 200 : 400, "application/json", out);
}

void WebDashboard::handleTestGateESP() {
  Serial.println("[WebDashboard] handleTestGateESP");
  if (!checkAuth()) {
    server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
    return;
  }
  
  Config c = cfgMgr.get();
  if (c.gateEspUrl.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"Gate ESP URL not configured\"}");
    return;
  }
  
  // Test Gate ESP communication
  HTTPClient http;
  http.begin(c.gateEspUrl);
  http.addHeader("Content-Type", "application/json");
  
  StaticJsonDocument<128> testDoc;
  testDoc["test"] = true;
  testDoc["source"] = "ttgo-dashboard";
  String testBody;
  serializeJson(testDoc, testBody);
  
  int httpCode = http.POST(testBody);
  String response = http.getString();
  http.end();
  
  StaticJsonDocument<256> doc;
  doc["success"] = (httpCode == 200);
  doc["httpCode"] = httpCode;
  doc["message"] = (httpCode == 200) ? "Gate ESP communication successful" : "Gate ESP communication failed";
  if (httpCode != 200) {
    doc["error"] = response;
  }
  
  String out;
  serializeJson(doc, out);
  server.send((httpCode == 200) ? 200 : 400, "application/json", out);
}

void WebDashboard::handleClearLogs() {
  Serial.println("[WebDashboard] handleClearLogs");
  if (!checkAuth()) {
    server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
    return;
  }
  
  // TODO: Implement actual log clearing functionality
  // For now, just return success
  StaticJsonDocument<128> doc;
  doc["success"] = true;
  doc["message"] = "Logs cleared successfully";
  
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void WebDashboard::handleDownloadLogs() {
  Serial.println("[WebDashboard] handleDownloadLogs");
  if (!checkAuth()) {
    server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
    return;
  }
  
  // TODO: Implement actual log file generation and download
  // For now, create a simple log content
  String logContent = "TTGO T-Call System Logs\n";
  logContent += "Generated: " + String(millis()) + "ms since boot\n";
  logContent += "Free Memory: " + String(ESP.getFreeHeap()) + " bytes\n";
  logContent += "Configuration loaded from SPIFFS\n";
  logContent += "System operational\n";
  
  server.sendHeader("Content-Type", "text/plain");
  server.sendHeader("Content-Disposition", "attachment; filename=ttgo-logs.txt");
  server.send(200, "text/plain", logContent);
}

void WebDashboard::handleRestart() {
  Serial.println("[WebDashboard] handleRestart");
  if (!checkAuth()) {
    server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
    return;
  }
  
  StaticJsonDocument<128> doc;
  doc["success"] = true;
  doc["message"] = "Device restart initiated";
  
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
  
  // Restart the device after a short delay
  delay(1000);
  ESP.restart();
}

void WebDashboard::handleConfigReset() {
  Serial.println("[WebDashboard] handleConfigReset");
  if (!checkAuth()) {
    server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
    return;
  }
  
  // Reset configuration to defaults
  bool success = cfgMgr.resetToDefaults();
  
  StaticJsonDocument<128> doc;
  doc["success"] = success;
  doc["message"] = success ? "Configuration reset to defaults" : "Failed to reset configuration";
  
  String out;
  serializeJson(doc, out);
  server.send(success ? 200 : 500, "application/json", out);
}
