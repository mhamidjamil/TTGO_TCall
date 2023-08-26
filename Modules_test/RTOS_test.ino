// #include <Arduino_FreeRTOS.h>
// Function that will run as a task

//! failed attempt

#define TARGETED_NUMBER "+923354888420"

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800   // Modem is SIM800
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb

#include <TinyGsmClient.h>
#include <Wire.h>

// TTGO T-Call pins
#define MODEM_RST 5
#define MODEM_PWKEY 4
#define MODEM_POWER_ON 23
#define MODEM_TX 27
#define MODEM_RX 26
#define I2C_SDA 21
#define I2C_SCL 22

#define SerialMon Serial
#define SerialAT Serial1

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

#define IP5306_ADDR 0x75
#define IP5306_REG_SYS_CTL0 0x00

bool setPowerBoostKeepOn(int en) {
  Wire.beginTransmission(IP5306_ADDR);
  Wire.write(IP5306_REG_SYS_CTL0);
  if (en) {
    Wire.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
  } else {
    Wire.write(0x35); // 0x37 is default reg value
  }
  return Wire.endTransmission() == 0;
}

void taskFunction(void *pvParameters) {

  for (;;) {
    // code to be executed by the task
    // This could be reading a sensor, controlling an output, etc.
    Serial.println("Hello from task 1!");
    digitalWrite(13, LOW);
    vTaskDelay(1000 / portTICK_PERIOD_MS); // delay for 1 second
    digitalWrite(13, HIGH);
    vTaskDelay(1000 / portTICK_PERIOD_MS); // delay for 1 second
  }
}

void taskFunction2(void *pvParameters) {

  for (;;) {
    // code to be executed by the task
    // This could be reading a sensor, controlling an output, etc.
    Serial.println("Hello from task 2!");
    if (SerialMon.available()) {
      char c = SerialMon.read();
      SerialAT.write(c);
    }

    if (SerialAT.available()) {
      char c = SerialAT.read();
      SerialMon.write(c);
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS); // delay for 1 second
  }
  vTaskDelay(1000 / portTICK_PERIOD_MS); // delay for 1 second
}

void setup() {
  Serial.begin(115200);

  // Keep power when running from battery
  Wire.begin(I2C_SDA, I2C_SCL);
  bool isOk = setPowerBoostKeepOn(1);
  SerialMon.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));

  // Set modem reset, enable, power pins
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  // Set GSM module baud rate and UART pins
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);
  SerialMon.println("Initializing modem...");
  modem.restart();
  pinMode(13, OUTPUT);
  // Create the task
  xTaskCreate(taskFunction, "Task 1", 4096, NULL, 1, NULL);
  xTaskCreate(taskFunction2, "Task 2", 4096, NULL, 1, NULL);
}

void loop() {
  // nothing goes here, all the work is done in the tasks
}
