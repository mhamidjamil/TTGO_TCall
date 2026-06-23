#include <Arduino.h>
#include <Wire.h>
#include <time.h>

#if __has_include("secrets.h")
#include "secrets.h"
#elif __has_include("secrets.example.h")
#include "secrets.example.h"
#endif

#include "ConfigManager.h"
#include "WiFiManager.h"
#include "FirebaseManager.h"
#include "RateLimitManager.h"
#include "SMSManager.h"
#include "CallManager.h"
#include "DisplayManager.h"
#include "DHTManager.h"
#include "ThingSpeakManager.h"
#include "WebDashboard.h"
#include "Logger.h"
#include "NtfyManager.h"

static ConfigManager configManager;
static WiFiManager wifiManager;
static FirebaseManager firebaseManager;
static RateLimitManager rateLimitManager;
static SMSManager smsManager;
static CallManager callManager;
static DisplayManager displayManager;
static DHTManager dhtManager;
static ThingSpeakManager thingSpeakManager;
static WebDashboard webDashboard;
static NtfyManager ntfyManager;

static unsigned long lastUiRefresh = 0;
static unsigned long lastCloudPoll = 0;
static unsigned long lastTelemetryPush = 0;
static unsigned long lastThingSpeakPush = 0;
static unsigned long lastRuntimeSettingsSync = 0;
static unsigned long lastHeartbeatPush = 0;
static unsigned long lastRecoveryRun = 0;
static unsigned long pendingPollStartMs = 0;
static V8Config runtimeConfig;
static String startupBootTime;
static String startupIp;
static bool lastTelemetryPushOk = false;
static String lastTelemetryPushMessage = "not_attempted";
static unsigned long telemetryIntervalMs = 15000UL;
static unsigned long thingSpeakIntervalMs = 15000UL;
static const unsigned long runtimeSettingsSyncIntervalMs = 10UL * 60UL * 1000UL;
static const unsigned long heartbeatIntervalMs = 60UL * 1000UL;
static const unsigned long recoveryIntervalMs = 60UL * 1000UL;
static const unsigned long stuckJobAgeSeconds = 5UL * 60UL;
static const unsigned long missedCallRingMs = 8000UL;
static const bool missedCallMode = true;
static bool showFirebasePushLogs = true;
static bool showThingSpeakPushLogs = true;
static bool jobLogs = true;
static BlockLists blockLists;
static unsigned long lastBlockListSync = 0;
static const unsigned long blockListSyncIntervalMs = 60UL * 1000UL;
static String modemLineBuffer;
static bool awaitingSmsBody = false;
static String pendingSmsNumber;
static int pendingSmsIndex = -1;
static String lastCallNumber;
static unsigned long lastCallHandledMs = 0;
static const unsigned long duplicateCallWindowMs = 30000UL;

static void initializeModemHardware() {
  Wire.begin(I2C_SDA_PIN_DEFAULT, I2C_SCL_PIN_DEFAULT);

  Wire.beginTransmission(0x75);
  Wire.write(0x00);
  Wire.write(0x37);
  Wire.endTransmission();

  pinMode(MODEM_PWKEY_PIN_DEFAULT, OUTPUT);
  pinMode(MODEM_RST_PIN_DEFAULT, OUTPUT);
  pinMode(MODEM_POWER_ON_PIN_DEFAULT, OUTPUT);
  digitalWrite(MODEM_PWKEY_PIN_DEFAULT, LOW);
  digitalWrite(MODEM_RST_PIN_DEFAULT, HIGH);
  digitalWrite(MODEM_POWER_ON_PIN_DEFAULT, HIGH);

  Serial1.begin(MODEM_SERIAL_BAUD_DEFAULT, SERIAL_8N1, MODEM_RX_PIN_DEFAULT, MODEM_TX_PIN_DEFAULT);
  delay(3000);
}

static String digitsOnly(const String &input) {
  String out;
  for (size_t i = 0; i < input.length(); ++i) {
    char c = input.charAt(i);
    if (c >= '0' && c <= '9') {
      out += c;
    }
  }
  return out;
}

static String normalizePhoneNumber(const String &raw) {
  String digits = digitsOnly(raw);
  if (digits.startsWith("00")) {
    digits = digits.substring(2);
  }
  if (digits.startsWith("0") && digits.length() == 11) {
    digits = String(runtimeConfig.defaultCountryCode) + digits.substring(1);
  }
  if (digits.length() == 10) {
    digits = String(runtimeConfig.defaultCountryCode) + digits;
  }
  if (digits.length() < 12) {
    return String();
  }
  return String("+") + digits;
}

static String displayPhoneNumber(const String &raw) {
  String normalized = normalizePhoneNumber(raw);
  return normalized.length() > 0 ? normalized : raw;
}

static bool numbersMatch(const String &left, const String &right) {
  String leftDigits = digitsOnly(left);
  String rightDigits = digitsOnly(right);
  // Alphanumeric sender IDs (e.g. "JAZZ", "Telenor") carry no digits, so the
  // numeric comparison below can never match them. Compare them as case-
  // insensitive text instead, otherwise such senders can never be blocked.
  if (leftDigits.length() == 0 || rightDigits.length() == 0) {
    String leftText = left;
    String rightText = right;
    leftText.trim();
    rightText.trim();
    leftText.toLowerCase();
    rightText.toLowerCase();
    return leftText.length() > 0 && leftText == rightText;
  }
  if (leftDigits == rightDigits) {
    return true;
  }
  String leftNormalized = normalizePhoneNumber(left);
  String rightNormalized = normalizePhoneNumber(right);
  if (leftNormalized.length() > 0 && rightNormalized.length() > 0 && leftNormalized == rightNormalized) {
    return true;
  }
  return leftDigits.length() >= 10 &&
         rightDigits.length() >= 10 &&
         leftDigits.substring(leftDigits.length() - 10) == rightDigits.substring(rightDigits.length() - 10);
}

static bool isBlockedNumber(const String &number, const String *blockedNumbers, size_t blockedCount) {
  for (size_t i = 0; i < blockedCount; ++i) {
    if (numbersMatch(number, blockedNumbers[i])) {
      return true;
    }
  }
  return false;
}

// ---- Incoming SMS UCS2/UTF-16BE normalization ----
static bool isHexString(const String &value) {
  if (value.length() == 0) {
    return false;
  }
  for (size_t i = 0; i < value.length(); ++i) {
    char c = value.charAt(i);
    bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    if (!hex) {
      return false;
    }
  }
  return true;
}

static int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return 0;
}

// Decode a UTF-16BE hex string to UTF-8. Returns false if it does not decode to
// mostly-printable text (so non-text hex is left untouched). BMP only.
static bool decodeUcs2Hex(const String &hex, String &out) {
  out = "";
  size_t n = hex.length();
  if (n == 0 || n % 4 != 0) {
    return false;
  }
  size_t units = n / 4;
  size_t printable = 0;
  for (size_t i = 0; i < units; ++i) {
    int hi = (hexNibble(hex.charAt(i * 4)) << 4) | hexNibble(hex.charAt(i * 4 + 1));
    int lo = (hexNibble(hex.charAt(i * 4 + 2)) << 4) | hexNibble(hex.charAt(i * 4 + 3));
    uint16_t cp = (uint16_t)((hi << 8) | lo);
    if (cp >= 0xD800 && cp <= 0xDFFF) {
      return false;  // surrogate pair (non-BMP) — not handled
    }
    if (cp < 0x80) {
      out += (char)cp;
    } else if (cp < 0x800) {
      out += (char)(0xC0 | (cp >> 6));
      out += (char)(0x80 | (cp & 0x3F));
    } else {
      out += (char)(0xE0 | (cp >> 12));
      out += (char)(0x80 | ((cp >> 6) & 0x3F));
      out += (char)(0x80 | (cp & 0x3F));
    }
    if (cp == 0x09 || cp == 0x0A || cp == 0x0D || (cp >= 0x20 && cp != 0x7F)) {
      printable++;
    }
  }
  return (printable * 100) >= (units * 80);
}

struct SmsNormalization {
  String text;
  String original;
  bool wasDecoded;
};

// If the body looks like UCS2 (hex, length %4, starts 00xx, decodes to text),
// return the decoded text; otherwise return the original unchanged.
static SmsNormalization normalizeSmsBody(const String &raw) {
  SmsNormalization result;
  result.original = raw;
  result.text = raw;
  result.wasDecoded = false;

  if (raw.length() >= 4 && raw.length() % 4 == 0 && isHexString(raw) && raw.startsWith("00")) {
    String decoded;
    if (decodeUcs2Hex(raw, decoded)) {
      result.text = decoded;
      result.wasDecoded = true;
    } else {
      Serial.println("[SMS] UCS2 decode failed or not printable; keeping original message");
    }
  }
  return result;
}

static void logJob(const String &message) {
  if (jobLogs) {
    Serial.print("[JOB] ");
    Serial.println(message);
  }
}

static unsigned long currentEpochSeconds() {
  time_t now = time(nullptr);
  if (now > 1000) {
    return (unsigned long)now;
  }
  return millis() / 1000UL;
}

static String formatEventTime(unsigned long epochSeconds) {
  if (epochSeconds > 1000) {
    time_t pakistanTime = (time_t)epochSeconds + (5 * 60 * 60);
    struct tm timeInfo;
    if (gmtime_r(&pakistanTime, &timeInfo)) {
      char buffer[24];
      strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S PKT", &timeInfo);
      return String(buffer);
    }
  }
  return String("uptime ") + String(millis() / 1000UL) + String("s");
}

static String dayKeyFromEpoch(unsigned long epochSeconds) {
  if (epochSeconds > 1000) {
    time_t raw = (time_t)epochSeconds;
    struct tm timeInfo;
    if (gmtime_r(&raw, &timeInfo)) {
      char buffer[12];
      strftime(buffer, sizeof(buffer), "%Y-%m-%d", &timeInfo);
      return String(buffer);
    }
  }
  return String("1970-01-01");
}

static String readModemResponse(unsigned long timeoutMs) {
  String buffer;
  unsigned long started = millis();
  while (millis() - started < timeoutMs) {
    while (Serial1.available()) {
      buffer += (char)Serial1.read();
      if (buffer.indexOf("\r\nOK\r\n") != -1 || buffer.indexOf("\r\nERROR\r\n") != -1) {
        return buffer;
      }
    }
    delay(10);
  }
  return buffer;
}

static int querySignalStrength() {
  Serial1.println("AT+CSQ");
  String response = readModemResponse(1500);
  int marker = response.indexOf("+CSQ:");
  if (marker < 0) {
    return -1;
  }
  int comma = response.indexOf(',', marker);
  if (comma < 0) {
    return -1;
  }
  String rssi = response.substring(marker + 5, comma);
  rssi.trim();
  return rssi.toInt();
}

static int queryBatteryPercent() {
  Serial1.println("AT+CBC");
  String response = readModemResponse(1500);
  int marker = response.indexOf("+CBC:");
  if (marker < 0) {
    return -1;
  }
  int firstComma = response.indexOf(',', marker);
  int secondComma = response.indexOf(',', firstComma + 1);
  if (firstComma < 0 || secondComma < 0) {
    return -1;
  }
  String percent = response.substring(firstComma + 1, secondComma);
  percent.trim();
  return percent.toInt();
}

static String queryNetworkOperator() {
  Serial1.println("AT+COPS?");
  String response = readModemResponse(2000);
  int firstQuote = response.indexOf('"');
  int secondQuote = response.indexOf('"', firstQuote + 1);
  if (firstQuote < 0 || secondQuote < 0) {
    return String();
  }
  return response.substring(firstQuote + 1, secondQuote);
}

static String quotedField(const String &line, int fieldIndex) {
  int searchFrom = 0;
  for (int i = 0; i <= fieldIndex; ++i) {
    int startQuote = line.indexOf('"', searchFrom);
    if (startQuote < 0) {
      return String();
    }
    int endQuote = line.indexOf('"', startQuote + 1);
    if (endQuote < 0) {
      return String();
    }
    if (i == fieldIndex) {
      return line.substring(startQuote + 1, endQuote);
    }
    searchFrom = endQuote + 1;
  }
  return String();
}

static void processIncomingCall(const String &rawNumber) {
  String number = displayPhoneNumber(rawNumber);
  unsigned long epochSeconds = currentEpochSeconds();
  String pakistanTimestamp = formatEventTime(epochSeconds);
  if (numbersMatch(number, lastCallNumber) && millis() - lastCallHandledMs < duplicateCallWindowMs) {
    Serial.print("[CALL] duplicate ignored number=");
    Serial.println(number);
    return;
  }
  lastCallNumber = number;
  lastCallHandledMs = millis();

  bool blocked = isBlockedNumber(number, blockLists.incomingCallers, blockLists.incomingCallerCount);
  bool firebaseOk = false;
  if (firebaseManager.isReady()) {
    firebaseOk = firebaseManager.pushCallReceived(number, !blocked, blocked, pakistanTimestamp);
    if (!firebaseOk) {
      Logger::warn("FIREBASE", firebaseManager.lastError().c_str());
    }
    firebaseManager.incrementDeviceCounter("totalCallsReceived");
    if (blocked) {
      firebaseManager.incrementDeviceCounter("totalCallsMutedIncoming");
    }
  }

  Serial.print("[CALL] incoming number=");
  Serial.print(number);
  Serial.print(" blocked=");
  Serial.print(blocked ? "yes" : "no");
  Serial.print(" time=");
  Serial.print(pakistanTimestamp);
  Serial.print(" firestore=");
  Serial.println(firebaseOk ? "ok" : "failed");

  if (blocked) {
    Logger::info("CALL", "Blocked caller logged without ntfy");
    return;
  }

  String message = String("Incoming call at ") + pakistanTimestamp;
  if (ntfyManager.notify(String("call from ") + number, message)) {
    Serial.print("[NTFY] call notification sent number=");
    Serial.println(number);
  } else {
    Logger::warn("NTFY", ntfyManager.lastError().c_str());
  }
}

static void processIncomingSms(const String &rawNumber, const String &rawMessage, int smsIndex) {
  String number = displayPhoneNumber(rawNumber);
  unsigned long epochSeconds = currentEpochSeconds();
  String pakistanTimestamp = formatEventTime(epochSeconds);

  // Normalize UCS2/UTF-16BE payloads before storing or notifying.
  SmsNormalization norm = normalizeSmsBody(rawMessage);
  bool blocked = isBlockedNumber(number, blockLists.incomingSms, blockLists.incomingSmsCount);

  bool firebaseOk = false;
  if (firebaseManager.isReady()) {
    firebaseOk = firebaseManager.pushSmsReceived(number, norm.original, norm.text, norm.wasDecoded, !blocked, blocked, pakistanTimestamp, smsIndex);
    if (!firebaseOk) {
      Logger::warn("FIREBASE", firebaseManager.lastError().c_str());
    }
    firebaseManager.incrementDeviceCounter("totalSmsReceived");
    if (blocked) {
      firebaseManager.incrementDeviceCounter("totalSmsMutedIncoming");
    }
  }

  Serial.print("[SMS] incoming index=");
  Serial.print(smsIndex);
  Serial.print(" number=");
  Serial.print(number);
  Serial.print(" blocked=");
  Serial.print(blocked ? "yes" : "no");
  Serial.print(" decoded=");
  Serial.print(norm.wasDecoded ? "yes" : "no");
  Serial.print(" time=");
  Serial.print(pakistanTimestamp);
  Serial.print(" firestore=");
  Serial.print(firebaseOk ? "ok" : "failed");
  Serial.print(" message=");
  Serial.println(norm.text);

  if (blocked) {
    Logger::info("SMS", "Blocked SMS sender logged without ntfy");
  } else {
    if (ntfyManager.notify(String("sms from ") + number, norm.text)) {
      Serial.print("[NTFY] sms notification sent number=");
      Serial.print(number);
      Serial.print(" message=");
      Serial.println(norm.text);
    } else {
      Logger::warn("NTFY", ntfyManager.lastError().c_str());
    }
  }

  if (firebaseOk && smsIndex > 0) {
    if (smsManager.deleteMessage(smsIndex)) {
      Serial.print("[SMS] deleted from SIM index=");
      Serial.println(smsIndex);
    } else {
      Serial.print("[SMS] delete failed index=");
      Serial.println(smsIndex);
    }
  } else if (!firebaseOk && smsIndex > 0) {
    Serial.print("[SMS] kept on SIM because Firestore upload failed index=");
    Serial.println(smsIndex);
  }
}

static void handleModemLine(String line) {
  line.trim();
  if (line.length() == 0) {
    return;
  }

  if (awaitingSmsBody) {
    awaitingSmsBody = false;
    processIncomingSms(pendingSmsNumber, line, pendingSmsIndex);
    pendingSmsNumber = String();
    pendingSmsIndex = -1;
    return;
  }

  if (line.startsWith("+CLIP:")) {
    String callerNumber = quotedField(line, 0);
    callManager.hangUp();
    processIncomingCall(callerNumber);
    return;
  }

  if (line.startsWith("+CMT:")) {
    pendingSmsNumber = quotedField(line, 0);
    awaitingSmsBody = pendingSmsNumber.length() > 0;
    return;
  }

  if (line.startsWith("+CMGR:")) {
    pendingSmsNumber = quotedField(line, 1);
    awaitingSmsBody = pendingSmsNumber.length() > 0;
    return;
  }

  if (line.startsWith("+CMTI:")) {
    int comma = line.lastIndexOf(',');
    if (comma > 0) {
      String index = line.substring(comma + 1);
      index.trim();
      pendingSmsIndex = index.toInt();
      Serial1.print("AT+CMGR=");
      Serial1.println(index);
    }
  }
}

static void handleModemEvents() {
  while (Serial1.available()) {
    char c = (char)Serial1.read();
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      handleModemLine(modemLineBuffer);
      modemLineBuffer = String();
      continue;
    }
    modemLineBuffer += c;
    if (modemLineBuffer.length() > 512) {
      modemLineBuffer = String();
    }
  }
}

static bool processPendingCommand() {
  bool claimedAny = false;

  FirestoreJob smsJob;
  if (firebaseManager.fetchNextSmsJob(smsJob)) {
    claimedAny = true;
    logJob(String("sms claimed id=") + smsJob.id + " enque_by=" + smsJob.enqueBy);
    String normalizedNumber = normalizePhoneNumber(smsJob.phoneNumber);
    bool active = true;
    String limitReason;
    if (normalizedNumber.length() == 0) {
      logJob(String("sms failed id=") + smsJob.id + " reason=number_invalid");
      firebaseManager.updateSmsJobStatus(smsJob, "failed", "number_invalid");
    } else if (!firebaseManager.fetchGatewayActive(active)) {
      logJob(String("sms failed id=") + smsJob.id + " reason=device_status_unavailable");
      firebaseManager.updateSmsJobStatus(smsJob, "failed", "device_status_unavailable");
    } else if (!active) {
      logJob(String("sms blocked id=") + smsJob.id + " reason=device_inactive");
      firebaseManager.updateSmsJobStatus(smsJob, "blocked", "device_inactive");
    } else if (isBlockedNumber(normalizedNumber, blockLists.outgoingSms, blockLists.outgoingSmsCount)) {
      logJob(String("sms blocked id=") + smsJob.id + " number=" + normalizedNumber + " reason=blocked_outgoing");
      firebaseManager.updateSmsJobStatus(smsJob, "blocked", "blocked_outgoing");
      firebaseManager.incrementDeviceCounter("totalSmsBlockedOutgoing");
    } else if (!rateLimitManager.canSend(limitReason)) {
      logJob(String("sms failed id=") + smsJob.id + " reason=" + limitReason);
      firebaseManager.updateSmsJobStatus(smsJob, "failed", limitReason);
    } else {
      logJob(String("sms sending id=") + smsJob.id + " number=" + normalizedNumber);
      bool sent = smsManager.sendMessage(normalizedNumber, smsJob.message);
      if (sent) {
        rateLimitManager.recordSend();
        firebaseManager.updateCounterSnapshot(rateLimitManager.dailyCount(), rateLimitManager.weeklyCount(), rateLimitManager.monthlyCount());
        firebaseManager.updateSmsJobStatus(smsJob, "sent", String());
        firebaseManager.incrementDeviceCounter("totalSmsSent");
        logJob(String("sms sent id=") + smsJob.id + " number=" + normalizedNumber);
      } else {
        firebaseManager.updateSmsJobStatus(smsJob, "failed", "send_failed");
        logJob(String("sms failed id=") + smsJob.id + " reason=send_failed");
      }
    }
  }

  FirestoreJob callJob;
  if (firebaseManager.fetchNextCallJob(callJob)) {
    claimedAny = true;
    logJob(String("call claimed id=") + callJob.id + " enque_by=" + callJob.enqueBy);
    String normalizedNumber = normalizePhoneNumber(callJob.phoneNumber);
    bool active = true;
    if (normalizedNumber.length() == 0) {
      logJob(String("call failed id=") + callJob.id + " reason=number_invalid");
      firebaseManager.updateCallJobStatus(callJob, "failed", false, 0, "number_invalid");
    } else if (!firebaseManager.fetchGatewayActive(active)) {
      logJob(String("call failed id=") + callJob.id + " reason=device_status_unavailable");
      firebaseManager.updateCallJobStatus(callJob, "failed", false, 0, "device_status_unavailable");
    } else if (!active) {
      logJob(String("call blocked id=") + callJob.id + " reason=device_inactive");
      firebaseManager.updateCallJobStatus(callJob, "blocked", false, 0, "device_inactive");
    } else if (isBlockedNumber(normalizedNumber, blockLists.outgoingCallers, blockLists.outgoingCallerCount)) {
      logJob(String("call blocked id=") + callJob.id + " number=" + normalizedNumber + " reason=blocked_outgoing");
      firebaseManager.updateCallJobStatus(callJob, "blocked", false, 0, "blocked_outgoing");
      firebaseManager.incrementDeviceCounter("totalCallsBlockedOutgoing");
    } else {
      logJob(String("call dialing id=") + callJob.id + " number=" + normalizedNumber);
      bool userPicked = false;
      int durationSeconds = 0;
      bool completed = callManager.placeMissedCall(normalizedNumber, missedCallRingMs, userPicked, durationSeconds);
      firebaseManager.updateCallJobStatus(
          callJob,
          completed ? "called" : "failed",
          userPicked,
          durationSeconds,
          completed ? String() : String("call_failed"));
      if (completed) {
        firebaseManager.incrementDeviceCounter("totalCallsMade");
      }
      logJob(String("call ") + (completed ? "called" : "failed") + " id=" + callJob.id + " picked=" + (userPicked ? "true" : "false") + " duration=" + String(durationSeconds));
    }
  }

  // Surface liveness on serial: when nothing was claimed, print an idle line
  // at most once per idle window so the operator can see polling is running
  // without flooding the terminal on every 3s poll.
  static unsigned long lastIdlePollLogMs = 0;
  const unsigned long idlePollLogIntervalMs = 30000UL;
  if (!claimedAny && millis() - lastIdlePollLogMs >= idlePollLogIntervalMs) {
    lastIdlePollLogMs = millis();
    logJob("poll idle: no pending sms/call jobs");
  }

  return claimedAny;
}

static void syncCountersFromCloud() {
  int daily = 0;
  int weekly = 0;
  int monthly = 0;
  if (firebaseManager.fetchCounterSnapshot(daily, weekly, monthly)) {
    rateLimitManager.loadSnapshot(daily, weekly, monthly);
    Logger::info("RATE_LIMIT", "Counters restored from Firebase");
  } else {
    Logger::warn("RATE_LIMIT", firebaseManager.lastError().c_str());
  }
}

static void printDhtStatus(const char *source) {
  float temperature = dhtManager.readTemperature();
  float humidity = dhtManager.readHumidity();
  Serial.print("[DHT] ");
  Serial.print(source);
  Serial.print(" temp=");
  if (temperature <= -999.0f) {
    Serial.print("NA");
  } else {
    Serial.print(temperature, 1);
  }
  Serial.print(" hum=");
  if (humidity < 0.0f) {
    Serial.println("NA");
  } else {
    Serial.println(humidity, 1);
  }
}

static void printCommandHelp() {
  Serial.println("[CMD] available commands:");
  Serial.println(" - dht    : print temperature and humidity");
  Serial.println(" - status : print wifi/ip/firebase status");
  Serial.println(" - sync   : sync runtime settings from Firebase now");
  Serial.println(" - ntfy test | test ntfy : send ntfy test notification");
  Serial.println(" - show sms : list all SMS messages with SIM indexes");
  Serial.println(" - delete sms <index> : delete one SMS by SIM index");
  Serial.println(" - delete all sms : delete every SMS from SIM memory");
  Serial.println(" - help   : show this command list");
}

static void printRuntimeSettingChange(const char *name, const String &oldValue, const String &newValue) {
  Serial.println();
  Serial.println("============================");
  Serial.print("old value of variable (");
  Serial.print(name);
  Serial.print("): ");
  Serial.print(oldValue);
  Serial.print("    |   new value is: ");
  Serial.println(newValue);
  Serial.println("============================");
  Serial.println();
}

// Pull the four block-list arrays from sim_module/device into RAM. Called on a
// short timer and on every settings sync so blocking a number in Firestore
// takes effect on the device within ~1 minute (and instantly via Sync).
static void refreshBlockLists() {
  if (!firebaseManager.isReady()) {
    return;
  }
  size_t oldIn = blockLists.incomingCallerCount + blockLists.incomingSmsCount;
  size_t oldOut = blockLists.outgoingCallerCount + blockLists.outgoingSmsCount;
  if (firebaseManager.fetchBlockLists(blockLists)) {
    size_t newIn = blockLists.incomingCallerCount + blockLists.incomingSmsCount;
    size_t newOut = blockLists.outgoingCallerCount + blockLists.outgoingSmsCount;
    if (oldIn != newIn || oldOut != newOut) {
      Serial.print("[BLOCK] lists updated incomingCallers=");
      Serial.print(blockLists.incomingCallerCount);
      Serial.print(" incomingSms=");
      Serial.print(blockLists.incomingSmsCount);
      Serial.print(" outgoingCallers=");
      Serial.print(blockLists.outgoingCallerCount);
      Serial.print(" outgoingSms=");
      Serial.println(blockLists.outgoingSmsCount);
    }
  } else {
    Logger::warn("FIRESTORE", firebaseManager.lastError().c_str());
  }
  lastBlockListSync = millis();
}

static bool syncRuntimeSettingsFromCloud(const char *source) {
  if (!firebaseManager.isReady()) {
    Logger::warn("FIREBASE", "Runtime settings sync skipped: firebase not ready");
    return false;
  }

  const uint32_t defaultIntervalOfDhtSeconds = 15;
  const bool defaultShowFirebasePushLogs = true;
  const bool defaultShowThingSpeakPushLogs = true;

  FirebaseRuntimeSettings settings;
  if (!firebaseManager.fetchRuntimeSettings(settings, defaultIntervalOfDhtSeconds, defaultShowFirebasePushLogs, defaultShowThingSpeakPushLogs, String(runtimeConfig.ntfyUrl))) {
    Logger::warn("FIREBASE", firebaseManager.lastError().c_str());
    return false;
  }

  unsigned long oldIntervalSeconds = telemetryIntervalMs / 1000UL;
  bool oldShowFirebasePushLogs = showFirebasePushLogs;
  bool oldShowThingSpeakPushLogs = showThingSpeakPushLogs;
  bool oldJobLogs = jobLogs;
  int oldDailyLimit = runtimeConfig.dailySmsLimit;
  int oldWeeklyLimit = runtimeConfig.weeklySmsLimit;
  int oldMonthlyLimit = runtimeConfig.monthlySmsLimit;
  String oldNtfyUrl = runtimeConfig.ntfyUrl;

  telemetryIntervalMs = (unsigned long)settings.intervalOfDhtSeconds * 1000UL;
  thingSpeakIntervalMs = telemetryIntervalMs < 15000UL ? 15000UL : telemetryIntervalMs;
  showFirebasePushLogs = settings.showFirebasePushLogs;
  showThingSpeakPushLogs = settings.showThingSpeakPushLogs;
  jobLogs = settings.jobLogs;
  runtimeConfig.dailySmsLimit = settings.dailySmsLimit;
  runtimeConfig.weeklySmsLimit = settings.weeklySmsLimit;
  runtimeConfig.monthlySmsLimit = settings.monthlySmsLimit;
  strlcpy(runtimeConfig.ntfyUrl, settings.ntfyUrl.c_str(), sizeof(runtimeConfig.ntfyUrl));
  ntfyManager.setUrl(settings.ntfyUrl);
  rateLimitManager.setLimits(runtimeConfig.dailySmsLimit, runtimeConfig.weeklySmsLimit, runtimeConfig.monthlySmsLimit);

  if (settings.createdIntervalOfDht) {
    Serial.println("[SYNC] created or healed Firebase variable: intervalOfDhtSeconds");
  }
  if (settings.createdShowFirebasePushLogs) {
    Serial.println("[SYNC] created or healed Firebase variable: showFirebasePushLogs");
  }
  if (settings.createdShowThingSpeakPushLogs) {
    Serial.println("[SYNC] created or healed Firebase variable: showThingSpeakPushLogs");
  }
  if (settings.createdJobLogs) {
    Serial.println("[SYNC] created or healed Firebase variable: jobLogs");
  }
  if (settings.createdDailySmsLimit) {
    Serial.println("[SYNC] created or healed Firebase variable: dailySmsLimit");
  }
  if (settings.createdWeeklySmsLimit) {
    Serial.println("[SYNC] created or healed Firebase variable: weeklySmsLimit");
  }
  if (settings.createdMonthlySmsLimit) {
    Serial.println("[SYNC] created or healed Firebase variable: monthlySmsLimit");
  }
  if (settings.createdNtfyUrl) {
    Serial.println("[SYNC] created or healed Firebase variable: ntfyUrl");
  }

  if (oldIntervalSeconds != settings.intervalOfDhtSeconds) {
    printRuntimeSettingChange("intervalOfDhtSeconds", String(oldIntervalSeconds), String(settings.intervalOfDhtSeconds));
  }
  if (oldShowFirebasePushLogs != showFirebasePushLogs) {
    printRuntimeSettingChange("showFirebasePushLogs", oldShowFirebasePushLogs ? "true" : "false", showFirebasePushLogs ? "true" : "false");
  }
  if (oldShowThingSpeakPushLogs != showThingSpeakPushLogs) {
    printRuntimeSettingChange("showThingSpeakPushLogs", oldShowThingSpeakPushLogs ? "true" : "false", showThingSpeakPushLogs ? "true" : "false");
  }
  if (oldJobLogs != jobLogs) {
    printRuntimeSettingChange("jobLogs", oldJobLogs ? "true" : "false", jobLogs ? "true" : "false");
  }
  if (oldDailyLimit != runtimeConfig.dailySmsLimit) {
    printRuntimeSettingChange("dailySmsLimit", String(oldDailyLimit), String(runtimeConfig.dailySmsLimit));
  }
  if (oldWeeklyLimit != runtimeConfig.weeklySmsLimit) {
    printRuntimeSettingChange("weeklySmsLimit", String(oldWeeklyLimit), String(runtimeConfig.weeklySmsLimit));
  }
  if (oldMonthlyLimit != runtimeConfig.monthlySmsLimit) {
    printRuntimeSettingChange("monthlySmsLimit", String(oldMonthlyLimit), String(runtimeConfig.monthlySmsLimit));
  }
  if (oldNtfyUrl != String(runtimeConfig.ntfyUrl)) {
    printRuntimeSettingChange("ntfyUrl", oldNtfyUrl, String(runtimeConfig.ntfyUrl));
  }

  refreshBlockLists();

  // Apply dashboard-managed WiFi pairs. Treat the cloud values as the desired
  // state; persist to SPIFFS only when something actually changed and at least
  // one SSID is set, so the empty default never wipes locally stored networks.
  bool anyWifiProvided = settings.wifiSsid1.length() > 0 || settings.wifiSsid2.length() > 0;
  bool wifiChanged =
      settings.wifiSsid1 != String(runtimeConfig.userWifiSsid1) ||
      settings.wifiPass1 != String(runtimeConfig.userWifiPass1) ||
      settings.wifiSsid2 != String(runtimeConfig.userWifiSsid2) ||
      settings.wifiPass2 != String(runtimeConfig.userWifiPass2);
  if (anyWifiProvided && wifiChanged) {
    if (configManager.updateUserWifi(settings.wifiSsid1, settings.wifiPass1, settings.wifiSsid2, settings.wifiPass2)) {
      strlcpy(runtimeConfig.userWifiSsid1, settings.wifiSsid1.c_str(), sizeof(runtimeConfig.userWifiSsid1));
      strlcpy(runtimeConfig.userWifiPass1, settings.wifiPass1.c_str(), sizeof(runtimeConfig.userWifiPass1));
      strlcpy(runtimeConfig.userWifiSsid2, settings.wifiSsid2.c_str(), sizeof(runtimeConfig.userWifiSsid2));
      strlcpy(runtimeConfig.userWifiPass2, settings.wifiPass2.c_str(), sizeof(runtimeConfig.userWifiPass2));
      Serial.println("[SYNC] WiFi credentials updated from cloud; reboot to connect with the new network");
    } else {
      Serial.println("[SYNC] WiFi credentials update failed to save to SPIFFS");
    }
  }

  Serial.print("[SYNC] source=");
  Serial.print(source);
  Serial.print(" intervalOfDhtSeconds=");
  Serial.print(settings.intervalOfDhtSeconds);
  Serial.print(" showFirebasePushLogs=");
  Serial.print(showFirebasePushLogs ? "true" : "false");
  Serial.print(" showThingSpeakPushLogs=");
  Serial.print(showThingSpeakPushLogs ? "true" : "false");
  Serial.print(" jobLogs=");
  Serial.println(jobLogs ? "true" : "false");

  lastRuntimeSettingsSync = millis();
  return true;
}

static void handleSerialCommand(String command) {
  command.trim();
  command.toLowerCase();
  if (command == "dht") {
    printDhtStatus("serial");
    return;
  }
  if (command == "status") {
    Serial.print("[STATUS] wifi=");
    Serial.print(wifiManager.modeName());
    Serial.print(" ip=");
    Serial.print(wifiManager.localIp().toString());
    Serial.print(" firebase=");
    Serial.println(firebaseManager.isReady() ? "ready" : "not_ready");
    return;
  }
  if (command == "help" || command == "?") {
    printCommandHelp();
    return;
  }
  if (command == "sync") {
    if (syncRuntimeSettingsFromCloud("manual")) {
      Serial.println("[SYNC] manual sync complete");
    } else {
      Serial.println("[SYNC] manual sync failed");
    }
    return;
  }
  if (command == "ntfy test" || command == "test ntfy") {
    if (ntfyManager.test()) {
      Serial.println("[NTFY] test notification sent");
    } else {
      Serial.print("[NTFY] test failed: ");
      Serial.println(ntfyManager.lastError());
    }
    return;
  }
  if (command == "show sms") {
    Serial.println("[SMS] listing SIM messages");
    Serial.println(smsManager.listMessages());
    return;
  }
  if (command == "delete all sms" || command == "delete sms all") {
    if (smsManager.deleteAllMessages()) {
      Serial.println("[SMS] all SIM messages deleted");
    } else {
      Serial.println("[SMS] delete all failed");
    }
    return;
  }
  if (command.startsWith("delete sms ")) {
    String indexText = command.substring(String("delete sms ").length());
    indexText.trim();
    int index = indexText.toInt();
    if (index <= 0) {
      Serial.println("[SMS] invalid delete index");
      return;
    }
    if (smsManager.deleteMessage(index)) {
      Serial.print("[SMS] deleted index=");
      Serial.println(index);
    } else {
      Serial.print("[SMS] delete failed index=");
      Serial.println(index);
    }
    return;
  }
  printCommandHelp();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Logger::info("BOOT", "Starting v8 runtime");
  startupBootTime = String(millis()) + String("ms");

  configManager.begin();
  runtimeConfig = configManager.get();
  if (runtimeConfig.pollingIntervalSeconds < 3) {
    runtimeConfig.pollingIntervalSeconds = 3;
  }
  ntfyManager.begin(runtimeConfig.ntfyUrl);

  initializeModemHardware();
  Logger::info("MODEM", "Hardware initialized");

  wifiManager.begin(runtimeConfig);
  Logger::info("WIFI", wifiManager.modeName().c_str());
  if (wifiManager.isStationConnected()) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  }

  rateLimitManager.begin(runtimeConfig);
  if (thingSpeakManager.begin(runtimeConfig)) {
    Logger::info("THINGSPEAK", "ThingSpeak manager ready");
  } else {
    Logger::warn("THINGSPEAK", thingSpeakManager.lastError().c_str());
  }

  firebaseManager.begin(runtimeConfig);
  if (firebaseManager.isReady()) {
    Logger::info("FIREBASE", "Firebase manager ready");
    if (firebaseManager.bootstrapGateway(
            runtimeConfig.deviceName,
            runtimeConfig.pollingIntervalSeconds,
            missedCallMode)) {
      Logger::info("FIRESTORE", "Gateway collections bootstrapped");
    } else {
      Logger::warn("FIRESTORE", firebaseManager.lastError().c_str());
    }
    syncCountersFromCloud();
    syncRuntimeSettingsFromCloud("startup");
  } else {
    Logger::warn("FIREBASE", firebaseManager.lastError().c_str());
  }

  if (!smsManager.begin()) {
    Logger::warn("SMS", "SMS manager init failed");
  }
  callManager.begin();
  displayManager.begin();
  dhtManager.begin();
  webDashboard.begin(runtimeConfig, wifiManager, firebaseManager, ntfyManager);

  startupIp = wifiManager.localIp().toString();
  Logger::info("BOOT", startupIp.c_str());
  Logger::info("API", webDashboard.docsUrl().c_str());
  pendingPollStartMs = millis();

  if (firebaseManager.isReady()) {
    if (firebaseManager.pushStartupStatus(startupBootTime, wifiManager.modeName(), startupIp, true)) {
      Logger::info("FIREBASE", "Startup status pushed");
    } else {
      Logger::warn("FIREBASE", firebaseManager.lastError().c_str());
    }
    firebaseManager.pushDeviceHeartbeat(
        runtimeConfig.deviceName,
        queryBatteryPercent(),
        querySignalStrength(),
        queryNetworkOperator(),
        currentEpochSeconds(),
        runtimeConfig.pollingIntervalSeconds,
        missedCallMode);
  }
}

void loop() {
  firebaseManager.pollCommands();

  if (firebaseManager.isReady() && millis() - lastRuntimeSettingsSync >= runtimeSettingsSyncIntervalMs) {
    syncRuntimeSettingsFromCloud("periodic");
  }

  // Keep the block lists fresh so changes made in Firestore apply without a
  // reboot or a full settings sync.
  if (firebaseManager.isReady() && millis() - lastBlockListSync >= blockListSyncIntervalMs) {
    refreshBlockLists();
  }

  if (firebaseManager.isReady() && millis() - lastRecoveryRun >= recoveryIntervalMs) {
    lastRecoveryRun = millis();
    unsigned long now = currentEpochSeconds();
    if (now > stuckJobAgeSeconds &&
        !firebaseManager.recoverStuckJobs(now - stuckJobAgeSeconds)) {
      Logger::warn("FIRESTORE", firebaseManager.lastError().c_str());
    } else if (jobLogs) {
      logJob("stuck job recovery checked");
    }
  }

  if (firebaseManager.isReady() && millis() - lastHeartbeatPush >= heartbeatIntervalMs) {
    lastHeartbeatPush = millis();
    if (!firebaseManager.pushDeviceHeartbeat(
            runtimeConfig.deviceName,
            queryBatteryPercent(),
            querySignalStrength(),
            queryNetworkOperator(),
            currentEpochSeconds(),
            runtimeConfig.pollingIntervalSeconds,
            missedCallMode)) {
      Logger::warn("FIRESTORE", firebaseManager.lastError().c_str());
    }
  }

  if (firebaseManager.isReady() && millis() >= pendingPollStartMs &&
      millis() - lastCloudPoll >= (unsigned long)runtimeConfig.pollingIntervalSeconds * 1000UL) {
    lastCloudPoll = millis();
    // If we just drained a job there may be more queued behind it. Re-arm the
    // poll timer so the next loop iteration polls again immediately and the
    // queue empties quickly, instead of waiting a full interval per job.
    if (processPendingCommand()) {
      lastCloudPoll = 0;
    }
  }

  callManager.loop();
  handleModemEvents();
  webDashboard.loop();
  if (webDashboard.consumeRuntimeSyncRequest()) {
    syncRuntimeSettingsFromCloud("dashboard");
  }

  while (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    handleSerialCommand(command);
  }

  if (millis() - lastUiRefresh > 3000) {
    lastUiRefresh = millis();
    float temperature = dhtManager.readTemperature();
    float humidity = dhtManager.readHumidity();

    String telemetryState = lastTelemetryPushOk ? "ok" : "fail";
    displayManager.update(
        temperature,
        humidity,
        wifiManager.modeName().c_str(),
        firebaseManager.isReady() ? telemetryState.c_str() : "LOCAL",
        rateLimitManager.dailyCount(),
        rateLimitManager.weeklyCount(),
        rateLimitManager.monthlyCount());

    if (firebaseManager.isReady() && millis() - lastTelemetryPush > telemetryIntervalMs) {
      lastTelemetryPush = millis();
      time_t now = time(nullptr);
      unsigned long epochSeconds = (now > 1000) ? (unsigned long)now : (millis() / 1000UL);
      lastTelemetryPushOk = firebaseManager.pushTelemetry(
          temperature,
          humidity,
          epochSeconds);
      lastTelemetryPushMessage = lastTelemetryPushOk ? "Telemetry pushed" : firebaseManager.lastError();
      if (lastTelemetryPushOk) {
        if (showFirebasePushLogs) {
          Logger::info("FIREBASE", lastTelemetryPushMessage.c_str());
        }
      } else {
        Logger::warn("FIREBASE", lastTelemetryPushMessage.c_str());
      }

      if (thingSpeakManager.isReady() && millis() - lastThingSpeakPush > thingSpeakIntervalMs) {
        lastThingSpeakPush = millis();
        bool thingSpeakOk = thingSpeakManager.update(temperature, humidity);
        if (thingSpeakOk) {
          if (showThingSpeakPushLogs) {
            Logger::info("THINGSPEAK", "Temperature and humidity pushed");
          }
        } else {
          Logger::warn("THINGSPEAK", thingSpeakManager.lastError().c_str());
        }
      }

      bool landingOk = firebaseManager.pushLandingSnapshot(
          temperature,
          humidity,
          rateLimitManager.dailyCount(),
          rateLimitManager.weeklyCount(),
          rateLimitManager.monthlyCount(),
          wifiManager.modeName(),
          startupIp,
          firebaseManager.isReady(),
          lastTelemetryPushOk,
          lastTelemetryPushMessage,
          epochSeconds);
      if (landingOk) {
        if (showFirebasePushLogs) {
          Logger::info("FIREBASE", "Landing snapshot pushed");
        }
      } else {
        Logger::warn("FIREBASE", firebaseManager.lastError().c_str());
      }
    }
  }
}
