#define TARGETED_NUMBER "+923354888420"

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800   // Modem is SIM800
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb

#include <TinyGsmClient.h>
#include <Wire.h>
#include "ModemManager.h"
#include "DisplayManager.h"
#include "DHTManager.h"
#include "arduino_secrets.h" // Include your secrets

#include <WiFi.h>
#include <ThingSpeak.h>

// Wi-Fi credentials
const char* ssid = MY_SSID;
const char* password = MY_PASSWORD;
const unsigned long ts_channel_id = MY_CHANNEL_ID;
const char *ts_api_key = THINGSPEAK_API;

WiFiClient espClient;

void connect_wifi() {
  // if (String(ssid).indexOf("skip") != -1 && wifi_working) {
  //   return;
  // }
  WiFi.begin(ssid, password); // Connect to Wi-Fi
  for (int i = 0; !wifiConnected(); i++) {
    if (i > 10) {
      // println("Timeout: Unable to connect to WiFi");
      break;
    }
    // Delay(500);
  }
  if (wifiConnected()) {
    // println("Wi-Fi connected successfully");
  } else {
    // digitalWrite(LED, LOW);
  }
}

#define DHTPIN 33 // Change the pin if necessary
#define DHTTYPE DHT11

ModemManager modemManager;
DisplayManager displayManager;
DHTManager dhtManager(DHTPIN, DHTTYPE);

void serialTask(void *pvParameters) {
    while (true) {
        modemManager.handleSerialCommunication();
        vTaskDelay(10 / portTICK_PERIOD_MS); // Short delay to avoid WDT reset
    }
}

void setup() {
    modemManager.initialize();
    displayManager.initialize();
    dhtManager.initialize();
    connect_wifi();
    initThingSpeak();
    // Show boot message
    displayManager.showBootMessage();

    xTaskCreate(serialTask, "SerialTask", 4096, NULL, 1, NULL);
}

void loop() {
    // Read temperature and humidity
    float temperature = dhtManager.readTemperature();
    float humidity = dhtManager.readHumidity();

    // Prepare status string
    String status = "sample message here";
    // status += "SMS: " + String(sms_allowed ? "_A" : "_N") + " ";
    // status += "Battery: " + String(battery_percentage) + "% ";
    // status += "Time: " + String(myRTC.hour) + ":" + String(myRTC.minutes) + ":" + String(myRTC.seconds);

    // Prepare lines for display
    String line1 = String(temperature) + " C, " + String(humidity) + " %";
    String line2 = "sample message here too";

    // Update the display
    displayManager.updateDisplay(line1, line2, status);
    updateThingSpeak(temperature, humidity);

    // Main loop can be used for other tasks
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}
void initThingSpeak() {
  // println("\nThingSpeak initializing...\n");
  ThingSpeak.begin(espClient); // Initialize ThingSpeak
  delay(500);
}

void updateThingSpeak(float temperature, int humidity) {
  if (humidity < 110) {
    ThingSpeak.setField(1, temperature); // Set temperature value
    ThingSpeak.setField(2, humidity);    // Set humidity value
    // Println(4, "After setting up fields");

    int updateStatus = ThingSpeak.writeFields(ts_channel_id, ts_api_key);
    if (updateStatus == 200) {
      // Println(4, "ThingSpeak update successful");
      // successMsg();
    } else {
      // Println(4, "Error updating ThingSpeak. Status: " + String(updateStatus));
      // errorMsg();
    }
  } else {
    // digitalWrite(LED, LOW);
  }
}

bool wifiConnected() {
  return (WiFi.status() == WL_CONNECTED);
}
