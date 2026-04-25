#ifndef V8_RATE_LIMIT_MANAGER_H
#define V8_RATE_LIMIT_MANAGER_H

class RateLimitManager {
public:
  bool canSend();
  void recordSend();
  void sync();
};

#endif
