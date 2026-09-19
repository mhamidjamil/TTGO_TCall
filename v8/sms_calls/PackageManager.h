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
// Expiry = subscribe date + (validityDays - safetyMarginDays), stored in Firebase
// as the single plain-text field `expiresAt` ("2026-09-28 01:44 PKT"). That one
// field is both what the operator edits and what the device reads back, so there
// is no second copy to fall out of step. Sending is allowed while the package is
// valid OR while expiresAt is empty/unreadable (fail-open) so a missed
// subscription SMS never bricks sending.
class PackageManager {
public:
  // Compile-time defaults; healed into Firebase if the node is missing.
  static constexpr const char *kDefaultTokens = "been subscribed,SMS,validity";
  static constexpr int kDefaultSafetyMarginDays = 1;

  void begin(FirebaseManager *firebaseManager, NtfyManager *ntfyManager);
  // True when sending should be permitted (valid package OR unknown state).
  bool isSmsAllowed(unsigned long nowEpoch) const;
  // A recorded expiry is the only thing that makes the package "known" - there is
  // no separate flag to keep in step with it.
  bool isKnown() const { return state.expiryEpoch != 0; }
  int daysRemaining(unsigned long nowEpoch) const;
  // Inspect an incoming SMS; if it is a subscription confirmation, update state,
  // persist to Firebase, and ntfy the operator. Returns true if handled.
  bool handleIncomingSms(const String &text, unsigned long nowEpoch);
  // Re-read /ttgo_tcall/package from RTDB so manual cloud edits (e.g. typing a new
  // expiresAt by hand) apply without a reboot. Call on the periodic sync.
  bool refreshFromCloud(unsigned long nowEpoch);
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
