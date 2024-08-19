#ifndef DEBUG_MANAGER_H
#define DEBUG_MANAGER_H

#include <Arduino.h>

class DebugManager {
public:
    void println(const String& str);
    void print(const String& str);
    void println(int debuggerID, const String& str);
    void print(int debuggerID, const String& str);
private:
    void output(const String& str);
};

#endif // DEBUG_MANAGER_H
