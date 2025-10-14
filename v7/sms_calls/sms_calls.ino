#include <Arduino.h>
#include <WiFi.h>
// SPIFFS removed for v7 runtime; using embedded/dashboard defaults and secrets.h
#include "secrets.h"
#include <Wire.h>
#define TINY_GSM_MODEM_SIM800   // Modem is SIM800
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb
#include <TinyGsmClient.h>
#include "ConfigManager.h"
#include "WebDashboard.h"
#include "SMSManager.h"
#include "CallManager.h"
#include "DisplayManager.h"
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "DHTManager.h"

// SPIFFSUtils removed from v7 runtime

ConfigManager configManager;
SMSManager smsManager(configManager);
CallManager callManager(configManager);
WebDashboard dashboard(configManager, &smsManager, &callManager);
DisplayManager displayManager;
// MQTT client
WiFiClient espClient;
PubSubClient mqttClient(espClient);
// DHT sensor (optional) - pin and type can be changed in secrets.h or defaults
#ifndef DHT_PIN
#define DHT_PIN 33
#endif
#ifndef DHT_TYPE
#define DHT_TYPE DHT11
#endif
DHTManager dht(DHT_PIN, DHT_TYPE);

// Serial command buffer
String serialCmd = "";

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting SMS & Calls Project");

  // SPIFFS disabled for v7 build — using embedded dashboard and secrets.h values
  Serial.println("SPIFFS disabled in this build; using embedded dashboard and secrets.h for defaults");

  configManager.begin();
  dht.initialize();
  // After loading defaults from secrets.h and persisted settings.
  // NOTE: remote settings may perform HTTP requests — defer that until
  // after we have a WiFi connection (see below) to avoid running network
  // code while network stack isn't ready.

  // --- Modem / power initialization (copied from v6 ModemManager)
  // pin definitions (adjust if your board wiring differs)
#define MODEM_RST 5
#define MODEM_PWKEY 4
#define MODEM_POWER_ON 23
#define MODEM_TX 27
#define MODEM_RX 26
#define I2C_SDA 21
#define I2C_SCL 22

  // Ensure power boost keep on (IP5306) so module stays powered from battery
  Wire.begin(I2C_SDA, I2C_SCL);
  // write to IP5306 register 0x00 to set keep-on bit
  Wire.beginTransmission(0x75);
  Wire.write(0x00);
  Wire.write(0x37);
  Wire.endTransmission();

  // setup modem power pins
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  // start serial1 for modem with pins
  Serial1.begin(MODEM_SERIAL_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);

  // TinyGSM modem instance and restart to initialize SIM module
  TinyGsm modem(Serial1);
  delay(3000);
  Serial.println("Initializing modem...");
  modem.restart();

  // Now connect to WiFi using secrets.h values
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
    // Connect to MQTT broker
    connectMQTT();
    // Now that WiFi is connected, it's safe to check remote settings
    if (configManager.checkAndApplyRemoteSettings()) {
      Serial.println("Applied remote settings on startup");
    } else {
      Serial.println("No remote settings applied on startup");
    }
  } else {
    Serial.println("WiFi not connected");
  }

  // Start dashboard (web server + API)
  dashboard.begin();

  // Initialize modem-related managers (they'll use Serial1 by default)
  smsManager.begin();
  callManager.begin();

  // Initialize the display
  displayManager.initialize();

  // Example: Update the display with dummy data
  displayManager.update(25.5, 60.0, 10, 5, 3, 2, true, true);
}

void loop() {
  dashboard.handleClient();
  smsManager.loop();
  callManager.loop();

  // Ensure MQTT loop runs. If disconnected, attempt reconnect every 3s without blocking main loop.
  static unsigned long lastMqttTry = 0;
  if (mqttClient.connected()) {
    mqttClient.loop();
  } else {
    if (millis() - lastMqttTry > 3000) {
      lastMqttTry = millis();
      bool ok = tryMQTTConnect();
      if (ok) Serial.println("[MQTT] Reconnected via periodic attempt");
    }
  }

  // Handle serial input commands
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      serialCmd.trim();
      if (serialCmd.length() > 0) {
  if (serialCmd.equalsIgnoreCase("status")) {
          // Print useful status info
          Serial.println("---- STATUS ----");
          Serial.print("WiFi: "); Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
          Serial.print("Local IP: "); Serial.println(WiFi.localIP());
          Serial.print("MQTT: "); Serial.println(mqttClient.connected() ? "Connected" : "Disconnected");
          // print config values
          Config c = configManager.get();
          Serial.print("forwardUrl: "); Serial.println(c.forwardUrl);
          Serial.print("allowSms: "); Serial.println(c.allowSms ? "1" : "0");
          Serial.print("allowCall: "); Serial.println(c.allowCall ? "1" : "0");
          // DHT readings
          float t = dht.readTemperature();
          float h = dht.readHumidity();
          Serial.print("DHT Temperature: "); Serial.println(isnan(t) ? NAN : t);
          Serial.print("DHT Humidity: "); Serial.println(isnan(h) ? NAN : h);
          Serial.println("----------------");
        } else if (serialCmd.equalsIgnoreCase("connect")) {
          Serial.println("[CMD] Forcing MQTT connect...");
          if (mqttClient.connected()) mqttClient.disconnect();
          delay(200);
          connectMQTT();
        } else if (serialCmd.startsWith("auth ")) {
          String token = serialCmd.substring(5);
          StaticJsonDocument<256> doc;
          doc["command"] = "auth";
          doc["token"] = token;
          String out; serializeJson(doc, out);
          String clientId = WiFi.macAddress();
          String topic = String("ttgo/") + clientId + "/auth";
          if (mqttClient.connected()) {
            mqttClient.publish(topic.c_str(), out.c_str());
            Serial.println("[CMD] Sent auth token over MQTT to " + topic);
          } else {
            Serial.println("[CMD] MQTT not connected; attempting to connect then authenticate");
            connectMQTT();
            delay(500);
            if (mqttClient.connected()) {
              mqttClient.publish(topic.c_str(), out.c_str());
              Serial.println("[CMD] Sent auth token after connect");
            } else {
              Serial.println("[CMD] Failed to connect MQTT");
            }
          }
        } else if (serialCmd.equalsIgnoreCase("dht")) {
          float t = dht.readTemperature();
          float h = dht.readHumidity();
          Serial.print("DHT T: "); Serial.println(t);
          Serial.print("DHT H: "); Serial.println(h);
        } else {
          Serial.println("Unknown command: " + serialCmd);
          Serial.println("Commands: status, connect, auth <token>, dht");
        }
      }
      serialCmd = "";
    } else {
      serialCmd += c;
    }
  }
  delay(1);
}

// MQTT callback
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // copy payload into a string
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.println("[MQTT] Received on " + String(topic) + ": " + msg);
  // Parse JSON
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, msg);
  if (!error) {
    const char* command = doc["command"];
    if (command && strcmp(command, "send_sms") == 0) {
      const char* to = doc["to"];
      const char* message = doc["message"];
      Serial.printf("[MQTT] Processing send_sms to %s: %s\n", to, message);
      String err;
      smsManager.sendSms(String(to), String(message), err);
      if (err.length() > 0) {
        Serial.println("[MQTT] SMS send error: " + err);
      } else {
        Serial.println("[MQTT] SMS sent successfully");
      }
    } else {
      Serial.println("[MQTT] Unknown command received");
    }
  } else {
    Serial.println("[MQTT] Failed to parse JSON");
  }
}

// connect to MQTT broker with 3s retry
void connectMQTT() {
  const char* broker = "test.mosquitto.org"; // default public broker; change via secrets.h if desired
  const int port = 1883;
  String clientId = WiFi.macAddress();
  mqttClient.setServer(broker, port);
  mqttClient.setCallback(mqttCallback);
  unsigned long lastAttempt = 0;
  while (!mqttClient.connected()) {
    if (millis() - lastAttempt < 3000) { delay(100); continue; }
    lastAttempt = millis();
    Serial.println("[MQTT] Attempting MQTT connection...");
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("[MQTT] connected");
      // subscribe to commands for this device
      String cmdTopic = String("ttgo/") + clientId + "/cmd";
      mqttClient.subscribe(cmdTopic.c_str());
  // subscribe to broadcast commands so server can reach us without needing direct IP
  mqttClient.subscribe("ttgo/all/cmd");
      // publish online status
      String statusTopic = String("ttgo/") + clientId + "/status";
      StaticJsonDocument<128> st;
      st["status"] = "online";
      st["ip"] = WiFi.localIP().toString();
      String s; serializeJson(st, s);
      mqttClient.publish(statusTopic.c_str(), s.c_str(), true);
      break;
    } else {
      Serial.print("[MQTT] failed, rc="); Serial.println(mqttClient.state());
      Serial.println("[MQTT] retrying in 3s...");
    }
  }
}

// Attempt a single MQTT connect (non-blocking caller). Returns true if connected.
bool tryMQTTConnect() {
  const char* broker = "test.mosquitto.org"; // should match connectMQTT default
  const int port = 1883;
  String clientId = WiFi.macAddress();
  mqttClient.setServer(broker, port);
  mqttClient.setCallback(mqttCallback);
  Serial.println("[MQTT] Trying single connect attempt...");
  if (mqttClient.connect(clientId.c_str())) {
    Serial.println("[MQTT] connected");
    // subscribe to commands for this device
    String cmdTopic = String("ttgo/") + clientId + "/cmd";
    mqttClient.subscribe(cmdTopic.c_str());
      // subscribe to broadcast commands so server can reach us without needing direct IP
      mqttClient.subscribe("ttgo/all/cmd");
    // publish online status
    String statusTopic = String("ttgo/") + clientId + "/status";
    StaticJsonDocument<128> st;
    st["status"] = "online";
    st["ip"] = WiFi.localIP().toString();
    String s; serializeJson(st, s);
    mqttClient.publish(statusTopic.c_str(), s.c_str(), true);
    return true;
  } else {
    Serial.print("[MQTT] connect failed, rc="); Serial.println(mqttClient.state());
    return false;
  }
}
