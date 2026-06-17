#include "MyWebServer.h"
#include "arduino_secrets.h"
#include <ArduinoJson.h>
#include <WiFi.h>

const char *ssid = MY_SSID;
const char *password = MY_PASSWORD;

MyWebServer myServer(80);

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");
  Serial.println("IP Address: " + WiFi.localIP().toString());

  myServer.begin();
}

void loop() {
  myServer.handleClient();

  // Receive data from the Python server
  String receivedData = myServer.getLastReceivedData();
  if (receivedData.length() > 0) {
    // Parse the received JSON data
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, receivedData);

    if (error) {
      Serial.print("Failed to parse JSON: ");
      Serial.println(error.c_str());
    } else {
      // Extract the "data" field
      String data = doc["data"];
      Serial.println("Extracted data from Python server: " + data);
    }

    // Clear the received data after processing
    myServer.clearLastReceivedData();
  }

  // Send data to the Python server from serial port
  if (Serial.available() > 0) {
    String dataToSend = Serial.readString();
    Serial.println("i get: " + dataToSend);
    myServer.sendDataToPythonServer(dataToSend);
  }
}
