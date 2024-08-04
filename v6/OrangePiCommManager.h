#ifndef ORANGE_PI_COMM_MANAGER_H
#define ORANGE_PI_COMM_MANAGER_H

#include <HTTPClient.h>
#include "DebugManager.h"
#include "arduino_secrets.h" // Include your secrets

class OrangePiCommManager {
public:
    OrangePiCommManager(DebugManager& debugManager);
    void sendData(const String& data);

private:
    DebugManager& debug;
    const char* serverName = ORANGEPI_SERVER;
};

#endif // ORANGE_PI_COMM_MANAGER_H
