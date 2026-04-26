#include "Logger.h"
#include <Arduino.h>

void Logger::info(const char *tag, const char *message) {
  Serial.print("[");
  Serial.print(tag);
  Serial.print("] ");
  Serial.println(message);
}

void Logger::warn(const char *tag, const char *message) {
  Serial.print("[");
  Serial.print(tag);
  Serial.print("] WARN: ");
  Serial.println(message);
}

void Logger::error(const char *tag, const char *message) {
  Serial.print("[");
  Serial.print(tag);
  Serial.print("] ERROR: ");
  Serial.println(message);
}
