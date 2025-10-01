#include "SMSManager.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

SMSManager::SMSManager(ConfigManager &cfgMgr) : cfgMgr(cfgMgr) {}

void SMSManager::begin() {
  // Serial1 should already be configured in main sketch with modem TX/RX pins and baud
  Serial.println("SMSManager initialized");
}

void SMSManager::loop() {
  // Poll Serial1 for modem unsolicited notifications like +CMTI
  String line;
  while (Serial1.available()) {
    char ch = (char)Serial1.read();
    line += ch;
    if (ch == '\n') {
      line.trim();
      if (line.startsWith("+CMTI:")) {
        // +CMTI: "SM",3  -> extract index
        int comma = line.lastIndexOf(',');
        if (comma > 0) {
          String idx = line.substring(comma + 1);
          idx.trim();
          int index = idx.toInt();
          if (index > 0) {
            // read SMS at index
            readAndForwardSms(index);
          }
        }
      }
      line = "";
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
  unsigned long start = millis();
  String resp;
  while (millis() - start < 5000) {
    while (Serial1.available()) resp += (char)Serial1.read();
    if (resp.indexOf("OK") >= 0 || resp.indexOf("ERROR") >= 0) break;
    delay(50);
  }

  // Parse +CMGR response: first line contains header like +CMGR: "REC UNREAD","+9233...",,"20/.."
  int hdrPos = resp.indexOf("+CMGR:");
  if (hdrPos < 0) { Serial.println("[SMSManager] No CMGR header found"); return; }
  int nl = resp.indexOf('\n', hdrPos);
  if (nl < 0) nl = resp.indexOf('\r', hdrPos);
  String hdr = resp.substring(hdrPos, nl >= 0 ? nl : hdrPos + 80);
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
    int bodyEnd = resp.indexOf('\r', bodyStart);
    if (bodyEnd < 0) bodyEnd = resp.indexOf('\n', bodyStart);
    if (bodyEnd < 0) bodyEnd = resp.length();
    body = resp.substring(bodyStart, bodyEnd);
    body.trim();
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
  if (c.useApiSecret && c.apiSecret.length() == 0) return false;

  HTTPClient http;
  http.begin(c.forwardUrl);
  http.addHeader("Content-Type", "application/json");
  if (c.forwardApiKey.length()) http.addHeader("X-Api-Key", c.forwardApiKey);

  StaticJsonDocument<256> doc;
  doc["type"] = type;
  doc["number"] = number;
  doc["body"] = body;
  if (c.useApiSecret) doc["secret"] = c.apiSecret;

  String b;
  serializeJson(doc, b);
  int code = http.POST(b);
  String resp = code>0?http.getString():String();
  http.end();
  if (code == 200) {
    Serial.println("[SMSManager] Forwarded event OK");
    return true;
  }
  Serial.println("[SMSManager] Forward failed, code=" + String(code) + " resp=" + resp);
  return false;
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

void SMSManager::handleIncomingSms(const String &from, const String &body) {
  Serial.println("Incoming SMS from " + from + ": " + body);
  forwardEvent("incoming_sms", from, body);
}

void SMSManager::forwardEvent(const String &type, const String &number, const String &body) {
  Config c = cfgMgr.get();
  if (c.forwardUrl.length() == 0) return;
  if (c.useApiSecret && c.apiSecret.length() == 0) return;

  HTTPClient http;
  http.begin(c.forwardUrl);
  http.addHeader("Content-Type", "application/json");
  if (c.forwardApiKey.length()) http.addHeader("X-Api-Key", c.forwardApiKey);

  StaticJsonDocument<256> doc;
  doc["type"] = type;
  doc["number"] = number;
  doc["body"] = body;
  if (c.useApiSecret) doc["secret"] = c.apiSecret;

  String b;
  serializeJson(doc, b);
  int code = http.POST(b);
  if (code > 0) {
    Serial.println("Forwarded event, response: " + http.getString());
  } else {
    Serial.println("Failed to forward event: " + String(code));
  }
  http.end();
}
