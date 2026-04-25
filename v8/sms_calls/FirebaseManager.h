#ifndef V8_FIREBASE_MANAGER_H
#define V8_FIREBASE_MANAGER_H

class FirebaseManager {
public:
  bool begin();
  bool pollCommands();
};

#endif
