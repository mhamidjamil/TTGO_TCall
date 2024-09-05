#include "MyWebServer.h"
#include <ArduinoJson.h>

MyWebServer::MyWebServer(int port) : server(port), lastReceivedData("") {}

void MyWebServer::begin() {
  server.on("/", HTTP_GET, std::bind(&MyWebServer::handleRoot, this));
  server.on("/data", HTTP_GET, std::bind(&MyWebServer::handleGetRequest, this));
  server.on("/data", HTTP_POST,
            std::bind(&MyWebServer::handlePostRequest, this));
  server.onNotFound(std::bind(&MyWebServer::handleNotFound, this));
  server.begin();
}

void MyWebServer::handleClient() { server.handleClient(); }

void MyWebServer::handleRoot() {
  server.send(200, "text/plain", "Welcome to the ESP32 Web Server");
}

void MyWebServer::clearLastReceivedData() {
  lastReceivedData = ""; // Clear the stored data
}

void MyWebServer::handleGetRequest() {
  String message = "Received GET request!";
  server.send(200, "application/json", "{\"message\":\"" + message + "\"}");
  sendDataToPythonServer(message);
}

void MyWebServer::handlePostRequest() {
  if (server.hasArg("plain")) {
    lastReceivedData = server.arg("plain"); // Store the received data
    server.send(200, "application/json",
                "{\"received\":\"" + lastReceivedData + "\"}");
    sendDataToPythonServer(lastReceivedData);
  } else {
    server.send(400, "application/json",
                "{\"error\":\"No message body received\"}");
  }
}

void MyWebServer::sendDataToPythonServer(const String &data) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://192.168.1.238:6678/esp32"); // Server URL
    http.addHeader("Content-Type",
                   "application/json"); // Specify the content type

    // Create a JSON document
    StaticJsonDocument<200> jsonDoc;
    jsonDoc["data"] = data;

    // Serialize JSON document to string
    String requestBody;
    serializeJson(jsonDoc, requestBody);

    // Send POST request
    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Response from server: " + response);
    } else {
      Serial.println("Error on sending POST: " + String(httpResponseCode));
    }

    http.end(); // Close connection
  } else {
    Serial.println("WiFi not connected");
  }
}

void MyWebServer::handleNotFound() {
  server.send(404, "text/plain", "404: Not Found");
}

String MyWebServer::getLastReceivedData() { return lastReceivedData; }
