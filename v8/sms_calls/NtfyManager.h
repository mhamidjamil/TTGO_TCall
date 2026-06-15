#ifndef V8_NTFY_MANAGER_H
#define V8_NTFY_MANAGER_H

#include <Arduino.h>

class NtfyManager {
public:
  bool begin(const String &url);
  void setUrl(const String &url);
  bool notify(const String &title, const String &message);
  bool test();
  String currentUrl() const;
  String lastError() const;

private:
  String topicUrl;
  String error;
};

#endif
