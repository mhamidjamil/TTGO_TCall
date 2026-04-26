#ifndef V8_DHT_MANAGER_H
#define V8_DHT_MANAGER_H

class DHTManager {
public:
  bool begin();
  float readTemperature();
  float readHumidity();
};

#endif
