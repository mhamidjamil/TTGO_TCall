#include <Arduino.h>
#include "SMSManager.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <WiFi.h>
#include <PubSubClient.h>

extern PubSubClient mqttClient;

SMSManager::SMSManager(ConfigManager &cfgMgr) : cfgMgr(cfgMgr), lastPingTime(0) {}

void SMSManager::begin() {
  // Serial1 should already be configured in main sketch with modem TX/RX pins and baud
  Serial.println("SMSManager initialized");
  // Try pinging the bridge/server to validate connectivity
  pingBridge();
  lastPingTime = millis();
}

void SMSManager::loop() {
  // Ping bridge every 5 minutes
  if (millis() - lastPingTime > 5 * 60 * 1000UL) {
    pingBridge();
    lastPingTime = millis();
  }

  // Poll Serial1 for modem unsolicited notifications like +CMTI using robust read
  String response = readResponseFromSerial1(200);
  if (response.length()) {
    if (response.indexOf("+CMTI:") != -1) {
      // Extract index from +CMTI: "SM",3
      int comma = response.lastIndexOf(',');
      if (comma > 0) {
        String idx = response.substring(comma + 1);
        idx.trim();
        int index = idx.toInt();
        if (index > 0) readAndForwardSms(index);
      }
    }
  }
}

// Helper: read SMS at index via AT+CMGR and forward to server, delete on success
void SMSManager::readAndForwardSms(int index) {
  Serial.printf("[SMSManager] Reading SMS at index %d\n", index);
  // Ensure text mode
  Serial1.println("AT+CMGF=1");
  delay(200);
  // Read message
  Serial1.printf("AT+CMGR=%d\r\n", index);
  String resp = readResponseFromSerial1(3000);

  // Parse +CMGR response: first line contains header like +CMGR: "REC UNREAD","+9233...",,"20/.."
  int hdrPos = resp.indexOf("+CMGR:");
  if (hdrPos < 0) {
    // Try alternative: some modems return header with CRLF before OK; attempt to find first line
    int firstNl = resp.indexOf('\n');
    if (firstNl > 0) hdrPos = 0; // we'll treat from start
  }
  if (hdrPos < 0) { Serial.println("[SMSManager] No CMGR header found; full response:\n" + resp); return; }
  int nl = resp.indexOf('\n', hdrPos);
  if (nl < 0) nl = resp.indexOf('\r', hdrPos);
  String hdr = resp.substring(hdrPos, nl >= 0 ? nl : resp.length());
  // Extract number between quotes
  int firstQuote = hdr.indexOf('"');
  int secondQuote = hdr.indexOf('"', firstQuote + 1);
  int thirdQuote = hdr.indexOf('"', secondQuote + 1);
  int fourthQuote = hdr.indexOf('"', thirdQuote + 1);
  String from = "";
  if (thirdQuote >= 0 && fourthQuote > thirdQuote) {
    from = hdr.substring(thirdQuote + 1, fourthQuote);
  } else if (firstQuote >= 0 && secondQuote > firstQuote) {
    from = hdr.substring(firstQuote + 1, secondQuote);
  }

  // body is after header line
  String body = "";
  int bodyStart = nl + 1;
  if (bodyStart > 0 && bodyStart < resp.length()) {
    body = resp.substring(bodyStart);
    // remove trailing OK or ERROR
    int okPos = body.indexOf("OK");
    if (okPos >= 0) body = body.substring(0, okPos);
    body.trim();
    // remove any leading CR/LF
    while (body.length() && (body.charAt(0) == '\r' || body.charAt(0) == '\n')) body.remove(0,1);
  }

  Serial.printf("[SMSManager] Incoming SMS from %s: %s\n", from.c_str(), body.c_str());
  // forward event, but we need the HTTP result to decide whether to delete
  bool forwarded = forwardEventWithResult("incoming_sms", from, body);
  if (forwarded) {
    // delete message to free space
    Serial1.printf("AT+CMGD=%d\r\n", index);
    delay(200);
    // read delete response
    String delr;
    unsigned long s2 = millis();
    while (millis() - s2 < 2000) {
      while (Serial1.available()) delr += (char)Serial1.read();
      if (delr.indexOf("OK") >= 0 || delr.indexOf("ERROR") >= 0) break;
      delay(50);
    }
    Serial.println("[SMSManager] Deleted SMS index " + String(index));
  }
}

// Forward event but return whether server returned 200
bool SMSManager::forwardEventWithResult(const String &type, const String &number, const String &body) {
  Config c = cfgMgr.get();
  if (c.forwardUrl.length() == 0) return false;

  HTTPClient http;
  Serial.println("[SMSManager] Forwarding to: " + c.forwardUrl);
  http.begin(c.forwardUrl);
  http.addHeader("Content-Type", "application/json");
  if (c.forwardApiKey.length()) http.addHeader("X-Api-Key", c.forwardApiKey);
  StaticJsonDocument<256> doc;
  doc["type"] = type;
  doc["number"] = number;
  doc["body"] = body;
  String b; serializeJson(doc, b);
  Serial.println("[SMSManager] Payload: " + b);
  int code = http.POST(b);
  String resp = code>0?http.getString():String();
  Serial.println("[SMSManager] Forward response code=" + String(code) + " body=" + resp);
  http.end();
  if (code == 200) {
    Serial.println("[SMSManager] Forwarded event OK");
    return true;
  }
  Serial.println("[SMSManager] Forward failed");
  return false;
}

void SMSManager::pingBridge() {
  Config c = cfgMgr.get();
  String url = c.forwardUrl;
  if (url.length() == 0) {
    Serial.println("[SMSManager] No forwardUrl configured, skipping ping");
    return;
  }
  // Build origin (scheme + host[:port]) then use /ping so we hit the bridge health endpoint
  int schemePos = url.indexOf("//");
  int start = (schemePos >= 0) ? schemePos + 2 : 0;
  int slashPos = url.indexOf('/', start);
  String origin = (slashPos > 0) ? url.substring(0, slashPos) : url;
  if (origin.endsWith("/")) origin = origin.substring(0, origin.length()-1);
  String pingUrl = origin + "/ping";
  Serial.println("[SMSManager] Pinging bridge at " + pingUrl);
  HTTPClient http;
  http.begin(pingUrl);
  int code = http.GET();
  String resp = code>0?http.getString():String();
  Serial.println("[SMSManager] Ping response code=" + String(code) + " body=" + resp);
  http.end();
}

String SMSManager::readResponseFromSerial1(unsigned long timeoutMs) {
  unsigned long start = millis();
  String response = "";
  // small initial delay to allow modem to respond
  delay(20);
  while (millis() - start < timeoutMs) {
    while (Serial1.available()) {
      response += Serial1.readString();
      // brief pause to accumulate
      delay(5);
    }
    if (response.length()) break;
    delay(10);
  }
  return response;
}

bool SMSManager::sendSms(const String &to, const String &message, String &err) {
  auto cfg = cfgMgr.get();
  if (!cfg.allowSms) { err = "Sending SMS disabled"; return false; }

  Serial.printf("[SMSManager] Sending SMS to %s: %s\n", to.c_str(), message.c_str());

  // Set text mode
  Serial1.println("AT+CMGF=1");
  delay(200);

  // Start send
  Serial1.print("AT+CMGS=\"");
  Serial1.print(to);
  Serial1.println("\"");
  delay(200);

  Serial1.print(message);
  Serial1.write((char)26); // Ctrl+Z

  // read response
  unsigned long start = millis();
  String resp;
  while (millis() - start < 15000) {
    while (Serial1.available()) resp += (char)Serial1.read();
    if (resp.indexOf("OK") >= 0) break;
    if (resp.indexOf("ERROR") >= 0) break;
    delay(100);
  }

  if (resp.indexOf("OK") >= 0) {
    forwardEvent("sent_sms", to, message);
    return true;
  }
  err = resp;
  return false;
}

String SMSManager::listAllMessages() {
  // Ensure text mode
  Serial1.println("AT+CMGF=1");
  delay(200);
  // List all messages
  Serial1.println("AT+CMGL=\"ALL\"\r");
  String resp = readResponseFromSerial1(3000);
  // Parse responses with lines starting +CMGL: index,"<stat>","<number>",... then body on next line
  DynamicJsonDocument out(2048);
  JsonArray arr = out.to<JsonArray>();
  int pos = 0;
  while (true) {
    int hdr = resp.indexOf("+CMGL:", pos);
    if (hdr < 0) break;
    int nl = resp.indexOf('\n', hdr);
    String hdrLine = (nl>hdr)?resp.substring(hdr, nl):resp.substring(hdr);
    // extract index
    int comma = hdrLine.indexOf(',');
    int idx = -1;
    if (comma > 0) {
      String sidx = hdrLine.substring(6, comma);
      sidx.trim(); idx = sidx.toInt();
    }
    // extract number within quotes
    int q1 = hdrLine.indexOf('"');
    int q2 = hdrLine.indexOf('"', q1+1);
    int q3 = hdrLine.indexOf('"', q2+1);
    int q4 = hdrLine.indexOf('"', q3+1);
    String num = "";
    if (q3>=0 && q4>q3) num = hdrLine.substring(q3+1, q4);
    // body is next line after hdr
    int bodyStart = nl+1;
    int nextHdr = resp.indexOf("+CMGL:", bodyStart);
    int bodyEnd = nextHdr>0?nextHdr:resp.length();
    String body = resp.substring(bodyStart, bodyEnd);
    // trim trailing OK/ERROR
    int okp = body.indexOf("OK"); if (okp>=0) body = body.substring(0, okp);
    body.trim();
    DynamicJsonDocument obj(512);
    obj["index"] = idx;
    obj["number"] = num;
    obj["body"] = body;
    arr.add(obj);
    pos = bodyEnd;
  }
  String outS; serializeJson(out, outS);
  return outS;
}

bool SMSManager::deleteAllMessages() {
  // List indices and delete each
  Serial1.println("AT+CMGF=1");
  delay(200);
  Serial1.println("AT+CMGL=\"ALL\"\r");
  String resp = readResponseFromSerial1(3000);
  int pos = 0;
  bool any = false;
  while (true) {
    int hdr = resp.indexOf("+CMGL:", pos);
    if (hdr < 0) break;
    int comma = resp.indexOf(',', hdr);
    if (comma < 0) break;
    String sidx = resp.substring(hdr+6, comma);
    sidx.trim();
    int idx = sidx.toInt();
    if (idx > 0) {
      Serial1.printf("AT+CMGD=%d\r\n", idx);
      delay(200);
      any = true;
    }
    pos = comma;
  }
  return any;
}

void SMSManager::handleIncomingSms(const String &from, const String &body) {
  Serial.println("Incoming SMS from " + from + ": " + body);
  forwardEvent("incoming_sms", from, body);
}

void SMSManager::forwardEvent(const String &type, const String &number, const String &body) {
  // Send via WebSocket
  StaticJsonDocument<256> doc;
  doc["type"] = type;
  doc["number"] = number;
  doc["body"] = body;

  String jsonStr;
  serializeJson(doc, jsonStr);
  // Publish to MQTT topic for this device on the broker
  String clientId = WiFi.macAddress();
  String topic = String("ttgo/") + clientId + "/events";
  if (mqttClient.connected()) {
    mqttClient.publish(topic.c_str(), jsonStr.c_str());
    Serial.println("Forwarded event via MQTT: " + jsonStr + " to " + topic);
  } else {
    Serial.println("MQTT not connected - cannot forward event: " + jsonStr);
  }
}
