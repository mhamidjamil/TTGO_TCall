/*************************************************************

  You can send/receive any data using WidgetTerminal object.

  App dashboard setup:
    Terminal widget attached to Virtual Pin V1
 *************************************************************/

/* Fill-in information from Blynk Device Info here */
#define BLYNK_TEMPLATE_ID "TMPL6oAF8teFi"
#define BLYNK_TEMPLATE_NAME "ttgoTcall"
#define BLYNK_AUTH_TOKEN "V0GUIMD1pXGciOfg6LUB7Xd6rhS4iRPp"

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

#include <BlynkSimpleEsp32.h>
#include <WiFi.h>
#include <WiFiClient.h>


// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = "YourNetworkName";
char pass[] = "YourPassword";

// Attach virtual serial terminal to Virtual Pin V1
WidgetTerminal terminal(V7);

// You can send commands from Terminal to your hardware. Just use
// the same Virtual Pin as your Terminal Widget
BLYNK_WRITE(V1) {

  // if you type "Marco" into Terminal Widget - it will respond: "Polo:"
  if (String("Marco") == param.asStr()) {
    terminal.println("You said: 'Marco'");
    terminal.println("I said: 'Polo'");
  } else {

    // Send it back
    terminal.print("You said:");
    terminal.write(param.getBuffer(), param.getLength());
    terminal.println();
  }

  // Ensure everything is sent
  terminal.flush();
}

void setup() {
  // Debug console
  Serial.begin(115200);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  // You can also specify server:
  // Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass, "blynk.cloud", 80);
  // Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass, IPAddress(192,168,1,100), 8080);

  // Clear the terminal content
  terminal.clear();

  // This will print Blynk Software version to the Terminal Widget when
  // your hardware gets connected to Blynk Server
  terminal.println(F("Blynk v" BLYNK_VERSION ": Device started"));
  terminal.println(F("-------------"));
  terminal.println(F("Type 'Marco' and get a reply, or type"));
  terminal.println(F("anything else and get it printed back."));
  terminal.flush();
}

void loop() { Blynk.run(); }
