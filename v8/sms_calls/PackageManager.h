#ifndef V8_PACKAGE_MANAGER_H
#define V8_PACKAGE_MANAGER_H

#include <Arduino.h>
#include "FirebaseManager.h"

class NtfyManager;

// Result of parsing one incoming SMS against the package-detection tokens.
struct PackageParseResult {
  bool matched = false;      // all required tokens present AND validity days found
  int validityDays = 0;      // extracted number before "day"
  long smsAllowance = 0;     // extracted number before "SMS" (0 if not found)
  int tokensMatched = 0;
  int tokensTotal = 0;
  int confidencePercent = 0; // tokensMatched / tokensTotal * 100
};

// Detects SIM package/subscription SMS, tracks expiry, and gates outgoing SMS.
//
// Detection is fully on-device and configurable: `matchTokens` is a comma-
// separated list of substrings that must ALL appear (case-insensitive) for a
// message to count as a subscription — edit it in Firebase /ttgo_tcall/package
// when the operator changes wording. The validity-day count and SMS count are
// extracted separately and robustly ("30day", "30 day", "30days" all work).
//
// Expiry = subscribe date + (validityDays - safetyMarginDays). Sending is allowed
// while the package is valid OR while the state is unknown (fail-open) so a missed
// subscription SMS never bricks sending.
class PackageManager {
public:
  // Compile-time defaults; healed into Firebase if the node is missing.
  static constexpr const char *kDefaultTokens = "been subscribed,SMS,validity";
  static constexpr int kDefaultSafetyMarginDays = 1;

  void begin(FirebaseManager *firebaseManager, NtfyManager *ntfyManager);
  // True when sending should be permitted (valid package OR unknown state).
  bool isSmsAllowed(unsigned long nowEpoch) const;
  bool isKnown() const { return state.known; }
  int daysRemaining(unsigned long nowEpoch) const;
  // Inspect an incoming SMS; if it is a subscription confirmation, update state,
  // persist to Firebase, and ntfy the operator. Returns true if handled.
  bool handleIncomingSms(const String &text, unsigned long nowEpoch);
  // Periodic tick: sends a one-time ntfy reminder ~reminderDays before expiry.
  void loop(unsigned long nowEpoch);
  // Manual override (serial/dashboard): set validity in days from now.
  bool setManual(int validityDays, unsigned long nowEpoch);
  void clear();
  String statusLine(unsigned long nowEpoch) const;

private:
  PackageParseResult parse(const String &text) const;
  void applySubscription(int validityDays, long smsAllowance, const String &text, unsigned long nowEpoch);

  FirebaseManager *firebaseManager = nullptr;
  NtfyManager *ntfyManager = nullptr;
  PackageState state;
  bool loaded = false;
  unsigned long reminderSentForExpiry = 0;  // expiryEpoch we already reminded for
  static const int kReminderDays = 1;        // ntfy this many days before expiry
};

#endif
