#include "DebugManager.h"

void DebugManager::println(const String& str) {
    Serial.println(str);
    // Add BLE notification code here if needed
}

void DebugManager::print(const String& str) {
    Serial.print(str);
    // Add BLE notification code here if needed
}

void DebugManager::println(int debuggerID, const String& str) {
    if (debuggerID == 4) {
        println(str);
    }
}

void DebugManager::print(int debuggerID, const String& str) {
    if (debuggerID == 4) {
        print(str);
    }
}

void DebugManager::output(const String& str) {
    Serial.print(str);
    // Add BLE notification code here if needed
}
