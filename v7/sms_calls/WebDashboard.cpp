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
  // Route /dashboard is protected and handled by handleDashboard
  server.on("/dashboard", [this]() {
    Serial.printf("[WebDashboard] Request /dashboard from %s\n", server.client().remoteIP().toString().c_str());
    this->handleDashboard();
  });
  // Register API routes (handler checks request body/method as needed)
  // SPIFFS removed: no spiffs diagnostics endpoint
  server.on("/api/config", [this]() { Serial.println("[WebDashboard] /api/config"); this->handleApiConfig(); });
  server.on("/api/events", [this]() { Serial.println("[WebDashboard] /api/events"); this->handleApiEvents(); });
  server.on("/api/send_sms", [this]() { Serial.println("[WebDashboard] /api/send_sms"); this->handleSendSms(); });
  server.on("/api/call", [this]() { Serial.println("[WebDashboard] /api/call"); this->handleCall(); });
  server.on("/api/hangup", [this]() { Serial.println("[WebDashboard] /api/hangup"); this->handleHangup(); });
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
    Serial.println("[WebDashboard] Auth OK — serving embedded dashboard");
    const char *embedded = R"rawliteral(
<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SMS & Calls Dashboard</title>
<style>body{font-family:Arial,sans-serif;padding:12px;background:#f7f7f7}main{max-width:900px;margin:18px auto;background:#fff;padding:18px;border-radius:6px}label{display:block;margin:8px 0}input[type=text]{width:100%;padding:8px;border:1px solid #ccc;border-radius:4px}button{padding:8px 12px;border:0;background:#007bff;color:#fff;border-radius:4px;margin-top:8px}pre{background:#eee;padding:8px;border-radius:4px}</style>
</head><body><main>
<h2>SMS & Calls Dashboard</h2>
<section id="config">
<h3>Settings</h3>
<label><input id="useApiSecret" type="checkbox"> Use API Secret</label>
<label>API Secret: <input id="apiSecret" type="text"></label>
<label>Forward URL: <input id="forwardUrl" type="text"></label>
<label>Forward API Key: <input id="forwardApiKey" type="text"></label>
<label>Settings URL: <input id="settingsUrl" type="text"></label>
<label>Settings Version: <input id="settingsVersion" type="text" readonly></label>
<label><input id="allowSms" type="checkbox"> Allow SMS</label>
<label><input id="allowCall" type="checkbox"> Allow Calls</label>
<div><button id="save">Save</button></div>
<pre id="saveResult"></pre>
</section>
<section id="remote"><h3>Remote Settings</h3>
<div><button id="checkSettings">Check Settings</button> <span id="checkSettingsOut"></span></div>
</section>
<section id="tests"><h3>Tests</h3>
<div><button id="testForward">Test Forward (simulate incoming SMS)</button><pre id="testForwardOut"></pre></div>
<div><h4>Send SMS</h4><label>To: <input id="smsTo" type="text"></label><label>Message: <input id="smsMsg" type="text"></label><button id="sendSms">Send SMS</button><pre id="sendSmsOut"></pre></div>
<div><h4>Simulate Incoming Call</h4><label>Number: <input id="callNum" type="text" value="+1000000000"></label><button id="testCall">Simulate Call</button><pre id="testCallOut"></pre></div>
</section>
<script>
window._DASH_PW = window._DASH_PW || '';
const headers = (window._DASH_PW)?{'X-Dashboard-Auth':window._DASH_PW,'Content-Type':'application/json'}:{'Content-Type':'application/json'};
const $=(id)=>document.getElementById(id);
function load(){fetch('/api/config',{headers}).then(r=>r.json()).then(j=>{ $('useApiSecret').checked=!!j.useApiSecret; $('apiSecret').value=j.apiSecret||''; $('forwardUrl').value=j.forwardUrl||''; $('forwardApiKey').value=j.forwardApiKey||''; $('allowSms').checked=!!j.allowSms; $('allowCall').checked=!!j.allowCall; $('settingsUrl').value=j.settingsUrl||''; $('settingsVersion').value=j.settingsVersion||''; }).catch(e=>alert('Failed to load config'))}
load();
$('save').addEventListener('click',()=>{const body={useApiSecret:$('useApiSecret').checked,apiSecret:$('apiSecret').value,forwardUrl:$('forwardUrl').value,forwardApiKey:$('forwardApiKey').value,allowSms:$('allowSms').checked,allowCall:$('allowCall').checked}; fetch('/api/config',{method:'POST',headers,body:JSON.stringify(body)}).then(r=>r.json()).then(j=>$('saveResult').textContent=JSON.stringify(j)).catch(e=>$('saveResult').textContent='Save failed')});
$('save').addEventListener('click',()=>{const body={useApiSecret:$('useApiSecret').checked,apiSecret:$('apiSecret').value,forwardUrl:$('forwardUrl').value,forwardApiKey:$('forwardApiKey').value,allowSms:$('allowSms').checked,allowCall:$('allowCall').checked,settingsUrl:$('settingsUrl').value,settingsVersion:$('settingsVersion').value}; fetch('/api/config',{method:'POST',headers,body:JSON.stringify(body)}).then(r=>r.json()).then(j=>$('saveResult').textContent=JSON.stringify(j)).catch(e=>$('saveResult').textContent='Save failed')});
$('testForward').addEventListener('click',()=>{fetch('/api/test_forward',{method:'POST',headers}).then(r=>r.json()).then(j=>$('testForwardOut').textContent=JSON.stringify(j)).catch(e=>$('testForwardOut').textContent='failed')});
$('sendSms').addEventListener('click',()=>{const body={to:$('smsTo').value,message:$('smsMsg').value}; fetch('/api/send_sms',{method:'POST',headers,body:JSON.stringify(body)}).then(r=>r.json()).then(j=>$('sendSmsOut').textContent=JSON.stringify(j)).catch(e=>$('sendSmsOut').textContent='send failed')});
$('sendSms').addEventListener('click',()=>{
  // client-side normalization: strip non-digits, handle local formats and prefix +92
  let raw = $('smsTo').value || '';
  let digits = raw.replace(/\D/g,'');
  if (digits.startsWith('00')) digits = digits.substring(2);
  if (digits.startsWith('0') && digits.length>10) digits = digits.substring(1);
  if (digits.length === 10) digits = '92' + digits; // local without leading 0
  if (digits.length < 11) { $('sendSmsOut').textContent='Invalid number'; return; }
  const normalized = '+' + digits;
  $('smsTo').value = normalized;
  const body={to:normalized,message:$('smsMsg').value};
  fetch('/api/send_sms',{method:'POST',headers,body:JSON.stringify(body)}).then(r=>r.json()).then(j=>$('sendSmsOut').textContent=JSON.stringify(j)).catch(e=>$('sendSmsOut').textContent='send failed');
});
$('testCall').addEventListener('click',()=>{const body={number:$('callNum').value}; fetch('/api/test_call',{method:'POST',headers,body:JSON.stringify({number:$('callNum').value})}).then(r=>r.json()).then(j=>$('testCallOut').textContent=JSON.stringify(j)).catch(e=>$('testCallOut').textContent='failed')});
$('checkSettings').addEventListener('click',()=>{fetch('/api/check_settings',{method:'POST',headers}).then(r=>r.json()).then(j=>{ $('checkSettingsOut').textContent=JSON.stringify(j); if (j.applied) load(); }).catch(e=>$('checkSettingsOut').textContent='failed')});
</script>
</main></body></html>
)rawliteral";
    String page = "";
    if (providedPw.length()) page += "<script>window._DASH_PW='" + providedPw + "';</script>";
    else if (headerPw.length()) page += "<script>window._DASH_PW='" + headerPw + "';</script>";
    page += embedded;
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
    // allow public GET so the UI can load defaults; mask apiSecret unless authenticated
    Config c = cfgMgr.get();
  // overlay compile-time defaults from secrets.h when fields are empty
#ifdef USE_API_SECRET_DEFAULT
  if (!c.useApiSecret) c.useApiSecret = (USE_API_SECRET_DEFAULT != 0);
#endif
#ifdef API_SECRET_DEFAULT
  if (c.apiSecret.length() == 0) c.apiSecret = String(API_SECRET_DEFAULT);
#endif
#ifdef FORWARD_URL_DEFAULT
  if (c.forwardUrl.length() == 0) c.forwardUrl = String(FORWARD_URL_DEFAULT);
#endif
#ifdef FORWARD_API_KEY_DEFAULT
  if (c.forwardApiKey.length() == 0) c.forwardApiKey = String(FORWARD_API_KEY_DEFAULT);
#endif
#ifdef ALLOW_SMS_DEFAULT
  // do not overwrite persisted boolean flags here; ConfigManager.loadFrom NVS
  // already applied compile-time defaults at startup if needed.
#endif
#ifdef ALLOW_CALL_DEFAULT
  // do not overwrite persisted boolean flags here; ConfigManager.loadFrom NVS
  // already applied compile-time defaults at startup if needed.
#endif
#ifdef SETTINGS_URL_DEFAULT
  if (c.settingsUrl.length() == 0) c.settingsUrl = String(SETTINGS_URL_DEFAULT);
#endif
#ifdef SETTINGS_VERSION_DEFAULT
  if (c.settingsVersion.length() == 0) c.settingsVersion = String(SETTINGS_VERSION_DEFAULT);
#endif
    StaticJsonDocument<512> doc;
    doc["useApiSecret"] = c.useApiSecret;
    doc["apiSecret"] = c.apiSecret;
    doc["forwardUrl"] = c.forwardUrl;
    doc["forwardApiKey"] = c.forwardApiKey;
    doc["allowSms"] = c.allowSms;
    doc["allowCall"] = c.allowCall;
  doc["settingsUrl"] = c.settingsUrl;
  doc["settingsVersion"] = c.settingsVersion;
    // mask apiSecret unless auth provided
    bool auth = false;
    if (server.hasHeader("X-Dashboard-Auth") && server.header("X-Dashboard-Auth") == String(DASHBOARD_PASSWORD)) auth = true;
    if (!auth) doc["apiSecret"] = "*****";
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
    return;
  }

  // POST: save config
  Serial.println("[WebDashboard] handleApiConfig POST");
  if (!checkAuth()) { Serial.println("[WebDashboard] handleApiConfig POST unauthorized"); server.send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
  String body = server.arg("plain");
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) { server.send(400, "application/json", "{\"error\":\"bad json\"}"); return; }
  Config c = cfgMgr.get();
  c.useApiSecret = doc["useApiSecret"] | c.useApiSecret;
  c.apiSecret = String((const char *)doc["apiSecret"].as<const char *>());
  c.forwardUrl = String((const char *)doc["forwardUrl"].as<const char *>());
  c.forwardApiKey = String((const char *)doc["forwardApiKey"].as<const char *>());
  c.allowSms = doc["allowSms"] | c.allowSms;
  c.allowCall = doc["allowCall"] | c.allowCall;
  c.settingsUrl = String((const char *)doc["settingsUrl"].as<const char *>());
  c.settingsVersion = String((const char *)doc["settingsVersion"].as<const char *>());
  if (cfgMgr.save(c)) server.send(200, "application/json", "{\"ok\":true}");
  else server.send(500, "application/json", "{\"ok\":false}\n");
}

void WebDashboard::handleApiEvents() {
  // This endpoint can be used by internal code to forward events to external URL
  server.send(200, "application/json", "{\"ok\":true}");
}

void WebDashboard::handleSendSms() {
  Config c = cfgMgr.get();
  if (c.useApiSecret) {
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
  if (c.useApiSecret) {
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
  if (c.useApiSecret) {
    String h = server.header("X-Api-Secret");
    if (h != c.apiSecret) { server.send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
  }
  if (!callManager) { server.send(500, "application/json", "{\"error\":\"no call manager\"}"); return; }
  Serial.println("[WebDashboard] handleHangup");
  callManager->hangup();
  server.send(200, "application/json", "{\"ok\":true}\n");
}
