#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <U8g2lib.h>

class DisplayManager {
public:
    DisplayManager();
    void initialize();
    void showBootMessage();
    void updateDisplay(const String& line1, const String& line2, const String& status);

private:
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
    bool display_working;
};

#endif // DISPLAYMANAGER_H
