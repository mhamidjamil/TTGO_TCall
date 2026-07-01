#include "PackageManager.h"

#include "NtfyManager.h"

namespace {
constexpr unsigned long kSecondsPerDay = 86400UL;
// Epoch is only trustworthy once NTP has set the clock (~2001+). Below this we
// only have millis()/1000, so date math is meaningless and we fail-open.
constexpr unsigned long kRealTimeThreshold = 1000000000UL;

bool hasRealTime(unsigned long nowEpoch) {
  return nowEpoch > kRealTimeThreshold;
}

// Read the integer immediately preceding `keyword` in `lower` (a lowercased
// copy of the message), skipping spaces. Handles "30day", "30 day", "30days".
// Returns -1 when the keyword or a preceding number is absent.
long extractNumberBefore(const String &lower, const char *keyword) {
  int idx = lower.indexOf(keyword);
  if (idx < 0) {
    return -1;
  }
  int i = idx - 1;
  while (i >= 0 && lower.charAt(i) == ' ') {
    i--;
  }
  int end = i;
  while (i >= 0 && lower.charAt(i) >= '0' && lower.charAt(i) <= '9') {
    i--;
  }
  int start = i + 1;
  if (start > end) {
    return -1;
  }
  return lower.substring(start, end + 1).toInt();
}
}

void PackageManager::begin(FirebaseManager *fb, NtfyManager *ntfy) {
  firebaseManager = fb;
  ntfyManager = ntfy;

  if (firebaseManager != nullptr && firebaseManager->isReady()) {
    if (firebaseManager->fetchPackageState(state, PackageManager::kDefaultTokens, PackageManager::kDefaultSafetyMarginDays)) {
      loaded = true;
      Serial.print("[PACKAGE] state loaded known=");
      Serial.print(state.known ? "yes" : "no");
      Serial.print(" validityDays=");
      Serial.print(state.validityDays);
      Serial.print(" tokens=");
      Serial.println(state.matchTokens);
      if (state.createdNode) {
        Serial.println("[PACKAGE] created/healed Firebase node /package with defaults");
      }
      return;
    }
    Serial.print("[PACKAGE] fetch failed: ");
    Serial.println(firebaseManager->lastError());
  }

  // Firebase unavailable — start with safe defaults (unknown → fail-open).
  state = PackageState();
  state.matchTokens = PackageManager::kDefaultTokens;
  state.safetyMarginDays = PackageManager::kDefaultSafetyMarginDays;
  Serial.println("[PACKAGE] using compile-time defaults (firebase unavailable)");
}

PackageParseResult PackageManager::parse(const String &text) const {
  PackageParseResult result;
  String lower = text;
  lower.toLowerCase();

  // Count how many of the required tokens are present.
  String tokens = state.matchTokens.length() > 0 ? state.matchTokens : String(PackageManager::kDefaultTokens);
  int start = 0;
  while (start <= (int)tokens.length()) {
    int comma = tokens.indexOf(',', start);
    String tok = (comma < 0) ? tokens.substring(start) : tokens.substring(start, comma);
    tok.trim();
    tok.toLowerCase();
    if (tok.length() > 0) {
      result.tokensTotal++;
      if (lower.indexOf(tok) != -1) {
        result.tokensMatched++;
      }
    }
    if (comma < 0) {
      break;
    }
    start = comma + 1;
  }

  long days = extractNumberBefore(lower, "day");
  long sms = extractNumberBefore(lower, "sms");
  result.validityDays = days > 0 ? (int)days : 0;
  result.smsAllowance = sms > 0 ? sms : 0;
  result.confidencePercent = result.tokensTotal > 0
                                 ? (result.tokensMatched * 100) / result.tokensTotal
                                 : 0;
  // Accept only when every required token is present AND a validity was found.
  result.matched = (result.tokensTotal > 0) &&
                   (result.tokensMatched == result.tokensTotal) &&
                   (result.validityDays > 0);
  return result;
}

bool PackageManager::handleIncomingSms(const String &text, unsigned long nowEpoch) {
  PackageParseResult r = parse(text);
  if (!r.matched) {
    return false;
  }

  applySubscription(r.validityDays, r.smsAllowance, text, nowEpoch);

  // Notify the operator with the extracted values and confidence.
  String daysLeftStr = hasRealTime(nowEpoch) ? String(daysRemaining(nowEpoch)) : String("?");
  String body = String("Package detected. validity=") + String(r.validityDays) + "d" +
                " sms=" + String(r.smsAllowance) +
                " confidence=" + String(r.confidencePercent) + "%" +
                " (" + String(r.tokensMatched) + "/" + String(r.tokensTotal) + " tokens)" +
                " daysLeft=" + daysLeftStr;
  Serial.print("[PACKAGE] ");
  Serial.println(body);
  if (ntfyManager != nullptr) {
    ntfyManager->notify("package subscription", body);
  }
  return true;
}

void PackageManager::applySubscription(int validityDays, long smsAllowance, const String &text, unsigned long nowEpoch) {
  state.known = true;
  state.subscribedEpoch = nowEpoch;
  state.validityDays = validityDays;
  state.smsAllowance = smsAllowance;
  int effectiveDays = validityDays - state.safetyMarginDays;
  if (effectiveDays < 0) {
    effectiveDays = 0;
  }
  // Only compute a real expiry epoch when the clock is trustworthy; otherwise
  // leave it 0 and rely on fail-open until a sync with valid time refreshes it.
  state.expiryEpoch = hasRealTime(nowEpoch)
                          ? nowEpoch + (unsigned long)effectiveDays * kSecondsPerDay
                          : 0;
  // Keep lastMessage short so the RTDB node stays lean.
  state.lastMessage = text.length() > 160 ? text.substring(0, 160) : text;
  reminderSentForExpiry = 0;

  if (firebaseManager != nullptr && firebaseManager->isReady()) {
    if (!firebaseManager->pushPackageState(state)) {
      Serial.print("[PACKAGE] persist failed: ");
      Serial.println(firebaseManager->lastError());
    }
  }
}

bool PackageManager::isSmsAllowed(unsigned long nowEpoch) const {
  // Fail-open: unknown package, or clock not yet trustworthy, or expiry unset.
  if (!state.known || !hasRealTime(nowEpoch) || state.expiryEpoch == 0) {
    return true;
  }
  return nowEpoch <= state.expiryEpoch;
}

int PackageManager::daysRemaining(unsigned long nowEpoch) const {
  if (!state.known || !hasRealTime(nowEpoch) || state.expiryEpoch == 0) {
    return -1;
  }
  if (nowEpoch >= state.expiryEpoch) {
    return 0;
  }
  return (int)((state.expiryEpoch - nowEpoch) / kSecondsPerDay);
}

void PackageManager::loop(unsigned long nowEpoch) {
  if (!state.known || !hasRealTime(nowEpoch) || state.expiryEpoch == 0) {
    return;
  }
  if (reminderSentForExpiry == state.expiryEpoch) {
    return;  // already reminded for this subscription
  }
  int left = daysRemaining(nowEpoch);
  if (left >= 0 && left <= kReminderDays) {
    reminderSentForExpiry = state.expiryEpoch;
    String body = String("SIM package expires in ") + String(left) +
                  " day(s). Re-subscribe to keep SMS sending active.";
    Serial.print("[PACKAGE] reminder: ");
    Serial.println(body);
    if (ntfyManager != nullptr) {
      ntfyManager->notify("package expiring", body);
    }
  }
}

bool PackageManager::setManual(int validityDays, unsigned long nowEpoch) {
  if (validityDays <= 0) {
    return false;
  }
  applySubscription(validityDays, 0, String("manual override"), nowEpoch);
  return true;
}

void PackageManager::clear() {
  String tokens = state.matchTokens;
  int margin = state.safetyMarginDays;
  state = PackageState();
  state.matchTokens = tokens;
  state.safetyMarginDays = margin;
  reminderSentForExpiry = 0;
  if (firebaseManager != nullptr && firebaseManager->isReady()) {
    firebaseManager->pushPackageState(state);
  }
}

String PackageManager::statusLine(unsigned long nowEpoch) const {
  if (!state.known) {
    return String("package: UNKNOWN (sending allowed) tokens=") + state.matchTokens;
  }
  int left = daysRemaining(nowEpoch);
  String leftStr = left < 0 ? String("?") : String(left);
  return String("package: validity=") + String(state.validityDays) + "d" +
         " sms=" + String(state.smsAllowance) +
         " daysLeft=" + leftStr +
         " allowed=" + (isSmsAllowed(nowEpoch) ? "yes" : "no") +
         " margin=" + String(state.safetyMarginDays) + "d";
}
