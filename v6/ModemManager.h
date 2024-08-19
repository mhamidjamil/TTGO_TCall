#ifndef MODEMMANAGER_H
#define MODEMMANAGER_H

#define TINY_GSM_MODEM_SIM800   // Modem is SIM800
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb

#include <TinyGsmClient.h>
#include <Wire.h>

class ModemManager {
public:
    ModemManager();
    void initialize();
    void handleSerialCommunication();

private:
    void setupPins();
    bool setPowerBoostKeepOn(int en);

    TinyGsm modem;
};

#endif // MODEMMANAGER_H
