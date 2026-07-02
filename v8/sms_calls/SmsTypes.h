#ifndef V8_SMS_TYPES_H
#define V8_SMS_TYPES_H

#include <Arduino.h>

// Result of decoding an incoming SMS body. Kept in a header (not the .ino) so it
// is defined before the Arduino auto-generated function prototypes, which are
// inserted right after the #includes — a free function in the .ino returning this
// type by value would otherwise fail with "does not name a type".
struct SmsNormalization {
  String text;
  String original;
  bool wasDecoded;
};

#endif
