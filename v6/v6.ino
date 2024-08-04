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

    // Main loop can be used for other tasks
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}
