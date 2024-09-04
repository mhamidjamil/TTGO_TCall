#ifndef MY_WEB_SERVER_H
#define MY_WEB_SERVER_H

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

class MyWebServer {
public:
    MyWebServer(int port);
    void begin();
    void handleClient();
    String getLastReceivedData();
    void sendDataToPythonServer(const String &data);  // Public method to send data

private:
    WebServer server;
    String lastReceivedData;

    void handleRoot();
    void handleNotFound();
    void handleGetRequest();
    void handlePostRequest();
};

#endif
