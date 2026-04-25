#ifndef V8_DISPLAY_MANAGER_H
#define V8_DISPLAY_MANAGER_H

class DisplayManager {
public:
  bool begin();
  void update(float temperature, float humidity, const char *networkMode, const char *cloudMode);
};

#endif
