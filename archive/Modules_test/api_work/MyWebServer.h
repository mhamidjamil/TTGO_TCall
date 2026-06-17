#ifndef MY_WEB_SERVER_H
#define MY_WEB_SERVER_H

#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFi.h>

class MyWebServer {
public:
  MyWebServer(int port);
  void begin();
  void handleClient();
  String getLastReceivedData();
  void sendDataToPythonServer(const String &data); // Public method to send data

  void clearLastReceivedData();

private:
  WebServer server;
  String lastReceivedData;

  void handleRoot();
  void handleNotFound();
  void handleGetRequest();
  void handlePostRequest();
};

#endif
