#ifndef V8_LOGGER_H
#define V8_LOGGER_H

class Logger {
public:
  static void info(const char *tag, const char *message);
  static void warn(const char *tag, const char *message);
  static void error(const char *tag, const char *message);
};

#endif
