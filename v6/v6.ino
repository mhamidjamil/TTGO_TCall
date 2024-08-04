#define TARGETED_NUMBER "+923354888420"

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800   // Modem is SIM800
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb

#include <TinyGsmClient.h>
#include <Wire.h>
#include "ModemManager.h"
#include "DisplayManager.h"
#include "arduino_secrets.h" // Include your secrets

ModemManager modemManager;
DisplayManager displayManager;

void serialTask(void *pvParameters) {
    while (true) {
        modemManager.handleSerialCommunication();
        vTaskDelay(10 / portTICK_PERIOD_MS); // Short delay to avoid WDT reset
    }
}

void setup() {
    modemManager.initialize();
    displayManager.initialize();
    xTaskCreate(serialTask, "SerialTask", 4096, NULL, 1, NULL);
}

void loop() {
    // Update the display with a random message
    String message = "Hello World! " + String(random(99));
    displayManager.updateDisplay(message);

    // Main loop can be used for other tasks
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}
