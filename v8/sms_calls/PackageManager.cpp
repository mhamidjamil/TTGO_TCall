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

// Epoch → "YYYY-MM-DD HH:MM PKT" (or "unset" when 0/invalid) for status output.
String pktDate(unsigned long epochSeconds) {
  if (epochSeconds < kRealTimeThreshold) {
    return String("unset");
  }
  time_t pkt = (time_t)epochSeconds + 5 * 60 * 60;
  struct tm timeInfo;
  if (!gmtime_r(&pkt, &timeInfo)) {
    return String("unset");
  }
  char buffer[28];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M PKT", &timeInfo);
  return String(buffer);
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
      Serial.print("[PACKAGE] state loaded expiresAt=");
      Serial.print(state.expiryEpoch == 0 ? String("(unknown)") : pktDate(state.expiryEpoch));
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

bool PackageManager::refreshFromCloud(unsigned long nowEpoch) {
  if (firebaseManager == nullptr || !firebaseManager->isReady()) {
    return false;
  }
  PackageState fresh;
  if (!firebaseManager->fetchPackageState(fresh, PackageManager::kDefaultTokens, PackageManager::kDefaultSafetyMarginDays)) {
    Serial.print("[PACKAGE] cloud refresh failed: ");
    Serial.println(firebaseManager->lastError());
    return false;
  }

  bool expiryChanged = fresh.expiryEpoch != state.expiryEpoch;
  bool tokensChanged = fresh.matchTokens != state.matchTokens;
  state = fresh;
  loaded = true;

  if (expiryChanged) {
    // A different expiry (manual RTDB edit or another writer) — re-arm the
    // reminder and confirm to the operator that the device picked it up.
    reminderSentForExpiry = 0;
    String detail = isKnown()
                        ? (String("expiresAt now ") + pktDate(state.expiryEpoch) +
                           "; daysLeft=" + String(daysRemaining(nowEpoch)) +
                           " allowed=" + (isSmsAllowed(nowEpoch) ? "yes" : "no"))
                        : String("expiresAt cleared (unknown package; sending allowed)");
    Serial.print("[PACKAGE] ");
    Serial.println(detail);
    if (ntfyManager != nullptr) {
      ntfyManager->notify("package updated", detail);
    }
  }
  if (tokensChanged) {
    Serial.print("[PACKAGE] matchTokens updated from cloud: ");
    Serial.println(state.matchTokens);
  }
  return true;
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
  // Fail-open: no expiry recorded, or the clock is not yet trustworthy.
  if (state.expiryEpoch == 0 || !hasRealTime(nowEpoch)) {
    return true;
  }
  return nowEpoch <= state.expiryEpoch;
}

int PackageManager::daysRemaining(unsigned long nowEpoch) const {
  if (state.expiryEpoch == 0 || !hasRealTime(nowEpoch)) {
    return -1;
  }
  if (nowEpoch >= state.expiryEpoch) {
    return 0;
  }
  return (int)((state.expiryEpoch - nowEpoch) / kSecondsPerDay);
}

void PackageManager::loop(unsigned long nowEpoch) {
  if (state.expiryEpoch == 0 || !hasRealTime(nowEpoch)) {
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
  bool legacy = state.legacyFieldsPresent;
  state = PackageState();
  state.matchTokens = tokens;
  state.safetyMarginDays = margin;
  state.legacyFieldsPresent = legacy;
  reminderSentForExpiry = 0;
  if (firebaseManager != nullptr && firebaseManager->isReady()) {
    firebaseManager->pushPackageState(state);
  }
}

String PackageManager::statusLine(unsigned long nowEpoch) const {
  if (!isKnown()) {
    return String("package: UNKNOWN (sending allowed) tokens=") + state.matchTokens;
  }
  int left = daysRemaining(nowEpoch);
  String leftStr = left < 0 ? String("?") : String(left);
  return String("package: validity=") + String(state.validityDays) + "d" +
         " sms=" + String(state.smsAllowance) +
         " daysLeft=" + leftStr +
         " expires=" + pktDate(state.expiryEpoch) +
         " allowed=" + (isSmsAllowed(nowEpoch) ? "yes" : "no") +
         " margin=" + String(state.safetyMarginDays) + "d";
}
