#include "MyWebServer.h"
#include <WiFi.h>
#include "arduino_secrets.h"

const char* ssid = MY_SSID;
const char* password = MY_PASSWORD;

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

    // Print the last received data from the POST request
    String receivedData = myServer.getLastReceivedData();
    if (receivedData.length() > 0) {
        Serial.println("Received data from Python server: " + receivedData);
    }

    // Example: Send data to Python server
    // String dataToSend = "Hello from ESP32";

      if (Serial.available()) {
    String dataToSend = Serial.readString();
    Serial.println("i get: "+dataToSend);
    myServer.sendDataToPythonServer(dataToSend);
  }

    // Add a delay to avoid spamming the server
    // delay(5000);  // Send data every 5 seconds
}
