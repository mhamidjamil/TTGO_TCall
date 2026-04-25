#ifndef V8_FIREBASE_MANAGER_H
#define V8_FIREBASE_MANAGER_H

#include <Arduino.h>
#include "ConfigManager.h"

struct FirebaseCommand {
  String id;
  String type;
  String number;
  String message;
  String status;
  String errorReason;
};

class FirebaseManager {
public:
  bool begin(const V8Config &config);
  bool isReady() const;
  bool pollCommands();
  bool pushStatus(const FirebaseCommand &command, const String &status, const String &errorReason = String());
  String lastError() const;

private:
  V8Config config{};
  bool ready = false;
  String error;
};

#endif
