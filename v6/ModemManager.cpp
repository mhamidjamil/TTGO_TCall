#include "ModemManager.h"

#define MODEM_RST 5
#define MODEM_PWKEY 4
#define MODEM_POWER_ON 23
#define MODEM_TX 27
#define MODEM_RX 26
#define I2C_SDA 21
#define I2C_SCL 22

#define IP5306_ADDR 0x75
#define IP5306_REG_SYS_CTL0 0x00

#define SerialMon Serial
#define SerialAT Serial1

ModemManager::ModemManager() : modem(SerialAT) {}

void ModemManager::initialize() {
    // Set console baud rate
    SerialMon.begin(115200);

    // Keep power when running from battery
    Wire.begin(I2C_SDA, I2C_SCL);
    bool isOk = setPowerBoostKeepOn(1);
    SerialMon.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));

    setupPins();

    // Set GSM module baud rate and UART pins
    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
    delay(3000);
    SerialMon.println("Initializing modem...");
    modem.restart();
}

void ModemManager::handleSerialCommunication() {
    if (SerialMon.available()) {
        char c = SerialMon.read();
        SerialAT.write(c);
    }

    if (SerialAT.available()) {
        char c = SerialAT.read();
        SerialMon.write(c);
    }
}

void ModemManager::setupPins() {
    pinMode(MODEM_PWKEY, OUTPUT);
    pinMode(MODEM_RST, OUTPUT);
    pinMode(MODEM_POWER_ON, OUTPUT);
    digitalWrite(MODEM_PWKEY, LOW);
    digitalWrite(MODEM_RST, HIGH);
    digitalWrite(MODEM_POWER_ON, HIGH);
}

bool ModemManager::setPowerBoostKeepOn(int en) {
    Wire.beginTransmission(IP5306_ADDR);
    Wire.write(IP5306_REG_SYS_CTL0);
    if (en) {
        Wire.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
    } else {
        Wire.write(0x35); // 0x37 is default reg value
    }
    return Wire.endTransmission() == 0;
}
