#include "FirebaseManager.h"

#if __has_include(<Firebase_ESP_Client.h>)
#include <Firebase_ESP_Client.h>
#define V8_HAS_FIREBASE_CLIENT 1
#else
#define V8_HAS_FIREBASE_CLIENT 0
class FirebaseAuth {};
class FirebaseConfig {};
class FirebaseData {};
class Firebase_ESP_Client {
public:
  void begin(FirebaseConfig *, FirebaseAuth *) {}
  bool ready() const { return false; }
};
#endif

namespace {
#if V8_HAS_FIREBASE_CLIENT
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig fbconfig;
Firebase_ESP_Client firebase;
#endif
}

bool FirebaseManager::begin(const V8Config &incomingConfig) {
  config = incomingConfig;
  ready = false;
  error = String();

  if (String(config.firebaseProjectId).isEmpty() || String(config.firebaseDatabaseUrl).isEmpty()) {
    error = "Firebase config missing";
    return false;
  }

#if V8_HAS_FIREBASE_CLIENT
  fbconfig.api_key = config.firebaseApiKey;
  fbconfig.database_url = config.firebaseDatabaseUrl;
  auth.user.email = config.firebaseUserEmail;
  auth.user.password = config.firebaseUserPassword;
  firebase.begin(&fbconfig, &auth);
  ready = true;
#else
  error = "Firebase client library unavailable in this build";
#endif

  return ready;
}

bool FirebaseManager::isReady() const {
  return ready;
}

bool FirebaseManager::pollCommands() {
  if (!ready) {
    return false;
  }

#if V8_HAS_FIREBASE_CLIENT
  // Phase 2 target: later wire command parsing and state transitions here.
  return true;
#else
  return false;
#endif
}

bool FirebaseManager::pushStatus(const FirebaseCommand &command, const String &status, const String &errorReason) {
  (void)command;
  (void)status;
  (void)errorReason;
  return ready;
}

String FirebaseManager::lastError() const {
  return error;
}
