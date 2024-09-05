#include "MyWebServer.h"
#include "arduino_secrets.h"
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
    Serial.println("Received data from Python server: " + receivedData);
    myServer.clearLastReceivedData();
  }

  // Send data to the Python server from serial port
  if (Serial.available() > 0) {
    String dataToSend = Serial.readString();
    Serial.println("i get: " + dataToSend);
    myServer.sendDataToPythonServer(dataToSend);
  }
}
