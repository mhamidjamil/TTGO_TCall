#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <U8g2lib.h>

class DisplayManager {
public:
    DisplayManager();
    void initialize();
    void updateDisplay(const String& message);

private:
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
};

#endif // DISPLAYMANAGER_H
