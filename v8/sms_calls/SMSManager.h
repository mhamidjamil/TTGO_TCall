#ifndef V8_SMS_MANAGER_H
#define V8_SMS_MANAGER_H

class SMSManager {
public:
  bool begin();
  bool sendMessage(const char *number, const char *message);
};

#endif
