#include "WebDashboard.h"
#include "secrets.h"
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>
#include "SMSManager.h"
#include "CallManager.h"

WebDashboard::WebDashboard(ConfigManager &cfgMgr, SMSManager *sms, CallManager *call, int port)
  : server(port), cfgMgr(cfgMgr), smsManager(sms), callManager(call) {}

void WebDashboard::begin() {
  // root serves a small info or redirect to /dashboard
  server.on("/", [this]() { server.send(200, "text/plain", "SMS & Calls module running"); });
  // Route /dashboard is protected and handled by handleDashboard
  server.on("/dashboard", [this]() { this->handleDashboard(); });
  // Register API routes (handler checks request body/method as needed)
  server.on("/api/config", [this]() { this->handleApiConfig(); });
  server.on("/api/events", [this]() { this->handleApiEvents(); });
  server.on("/api/send_sms", [this]() { this->handleSendSms(); });
  server.on("/api/call", [this]() { this->handleCall(); });
  server.on("/api/hangup", [this]() { this->handleHangup(); });

  // serve static assets from SPIFFS /data folder
  server.serveStatic("/dashboard.css", SPIFFS, "/data/dashboard.css");
  server.serveStatic("/dashboard.js", SPIFFS, "/data/dashboard.js");

  server.begin();
}

void WebDashboard::handleClient() { server.handleClient(); }

bool WebDashboard::checkAuth() {
  // basic auth: check header 'X-Dashboard-Auth' or query param 'pw'
  if (server.hasHeader("X-Dashboard-Auth")) {
    String v = server.header("X-Dashboard-Auth");
    if (v == String(DASHBOARD_PASSWORD)) return true;
  }
  if (server.hasArg("pw")) {
    if (server.arg("pw") == String(DASHBOARD_PASSWORD)) return true;
  }
  return false;
}

void WebDashboard::handleDashboard() {
  // If there's POST data (form pw or JSON/plain) treat it as login attempt
  if (server.hasArg("pw") || server.hasArg("plain")) {
    String pw;
    if (server.hasArg("pw")) {
      pw = server.arg("pw");
    } else {
      String body = server.arg("plain");
      // body may be URL-encoded 'pw=...' or raw pw or JSON {"pw":"..."}
      if (body.startsWith("pw=")) pw = body.substring(3);
      else {
        // try JSON
        StaticJsonDocument<128> jd;
        DeserializationError e = deserializeJson(jd, body);
        if (!e && jd.containsKey("pw")) pw = String((const char *)jd["pw"].as<const char *>());
        else pw = body;
      }
    }

    if (pw == String(DASHBOARD_PASSWORD) || server.header("X-Dashboard-Auth") == String(DASHBOARD_PASSWORD)) {
      // Serve the SPA directly after successful password
      File f = SPIFFS.open("/data/dashboard.html", "r");
      if (!f) { server.send(500, "text/plain", "Dashboard not found"); return; }
      server.streamFile(f, "text/html");
      f.close();
      return;
    }
    server.send(401, "text/plain", "Unauthorized - wrong password");
    return;
  }

  // If header or querypw auth is present, validate and serve SPA
  if (checkAuth()) {
    File f = SPIFFS.open("/data/dashboard.html", "r");
    if (!f) { server.send(500, "text/plain", "Dashboard not found"); return; }
    server.streamFile(f, "text/html");
    f.close();
    return;
  }

  // No auth provided — serve a simple HTML password form so the browser can post
  const char *loginForm = "<html><body><h3>Enter Dashboard Password</h3>"
    "<form method='post' action='/dashboard'><input name='pw' type='password'/>"
    "<button type='submit'>Login</button></form></body></html>";
  server.send(200, "text/html", loginForm);
}

void WebDashboard::handleApiConfig() {
  // If there's a request body (arg "plain") treat as POST to save; otherwise treat as GET
  if (!server.hasArg("plain")) {
    if (!checkAuth()) { server.send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
    Config c = cfgMgr.get();
    StaticJsonDocument<512> doc;
    doc["useApiSecret"] = c.useApiSecret;
    doc["apiSecret"] = c.apiSecret;
    doc["forwardUrl"] = c.forwardUrl;
    doc["forwardApiKey"] = c.forwardApiKey;
    doc["allowSms"] = c.allowSms;
    doc["allowCall"] = c.allowCall;
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
    return;
  }

  // POST: save config
  if (!checkAuth()) { server.send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
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
  String to = String((const char *)doc["to"].as<const char *>());
  String msg = String((const char *)doc["message"].as<const char *>());
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
  callManager->hangup();
  server.send(200, "application/json", "{\"ok\":true}\n");
}
