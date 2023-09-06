#include <HTTPClient.h>
#include <WiFi.h>
int test = 200;


const char *ssid = "Revenant";
const char *password = "56787656";

// Your Domain name with URL path or IP address with path
//  String serverName = "http://192.168.1.106:1880/update-sensor";
String serverName =
    "https://api.callmebot.com/whatsapp.php?phone=+923354888420&text=";
String linkEnd = "&apikey=518125";

// https://api.callmebot.com/whatsapp.php?phone=+923354888420&text=This+is+a+test&apikey=518125

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an
// int.
unsigned long lastTime = 0;
// Timer set to 10 minutes (600000)
// unsigned long timerDelay = 600000;
// Set timer to 5 seconds (5000)
unsigned long timerDelay = 5000;

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Timer set to 5 seconds (timerDelay variable), it will take 5 "
                 "seconds before publishing the first reading.");
}
void loop() {
  // Send an HTTP POST request every 10 minutes
  if ((millis() - lastTime) > timerDelay) {
    // Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;

      String serverPath = serverName +"Test+:+"+ String(test)+linkEnd;

      // Your Domain name with URL path or IP address with path
      //   http.begin(serverPath.c_str());
      test++;
      http.begin(serverPath);

      // Send HTTP GET request
      int httpResponseCode = http.GET();

      if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
      } else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      // Free resources
      http.end();
    } else {
      Serial.println("WiFi Disconnected");
    }
    delay(5000);
    // lastTime = millis();
  }
}
