#ifndef V8_SECRETS_EXAMPLE_H
#define V8_SECRETS_EXAMPLE_H

// WiFi defaults
#define WIFI_SSID_DEFAULT "YOUR_WIFI_SSID"
#define WIFI_PASS_DEFAULT "YOUR_WIFI_PASSWORD"
#define WIFI_SSID_BACKUP_DEFAULT "YOUR_BACKUP_WIFI_SSID"
#define WIFI_PASS_BACKUP_DEFAULT "YOUR_BACKUP_WIFI_PASSWORD"

// AP fallback defaults
#define AP_SSID_DEFAULT "TTGO-TCALL"
#define AP_PASS_DEFAULT "YOUR_AP_PASSWORD"
#define WEB_SERVER_PORT_DEFAULT 420

// Dashboard / operator defaults
#define DASHBOARD_PASSWORD_DEFAULT "YOUR_DASHBOARD_PASSWORD"

// Device identity defaults
#define DEVICE_ID_DEFAULT "device_001"
#define DEVICE_NAME_DEFAULT "TTGO T-Call v8"
#define OWNER_NAME_DEFAULT "Device Owner"
#define MY_NUMBER_DEFAULT "+92XXXXXXXXXX"
#define DEFAULT_COUNTRY_CODE_DEFAULT "92"

// Legacy SMS and call allowlists
#define ADMIN_NUMBERS_DEFAULT ""
#define AUTHENTIC_NUMBERS_DEFAULT ""
#define BYPASS_KEY_DEFAULT "YOUR_BYPASS_KEY"

// Hardware defaults
#define MODEM_SERIAL_BAUD_DEFAULT 115200
#define MODEM_RST_PIN_DEFAULT 5
#define MODEM_PWKEY_PIN_DEFAULT 4
#define MODEM_POWER_ON_PIN_DEFAULT 23
#define MODEM_TX_PIN_DEFAULT 27
#define MODEM_RX_PIN_DEFAULT 26
#define I2C_SDA_PIN_DEFAULT 21
#define I2C_SCL_PIN_DEFAULT 22
#define DHT_PIN_DEFAULT 33
#define DHT_TYPE_DEFAULT 11

// Firebase defaults
#define FIREBASE_PROJECT_ID_DEFAULT "YOUR_FIREBASE_PROJECT_ID"
#define FIREBASE_DATABASE_URL_DEFAULT "https://YOUR_PROJECT-default-rtdb.firebaseio.com"
#define FIREBASE_API_KEY_DEFAULT "YOUR_FIREBASE_API_KEY"
#define FIREBASE_USE_ANONYMOUS_DEFAULT 1
#define FIREBASE_USER_EMAIL_DEFAULT "YOUR_DEVICE_EMAIL"
#define FIREBASE_USER_PASSWORD_DEFAULT "YOUR_DEVICE_PASSWORD"
#define FIREBASE_COMMAND_PATH_DEFAULT "/ttgo_tcall/commands/pending"
#define FIREBASE_HISTORY_PATH_DEFAULT "/ttgo_tcall/commands/history"
#define FIREBASE_COUNTER_PATH_DEFAULT "/ttgo_tcall/counters"
#define FIREBASE_STATUS_PATH_DEFAULT "/ttgo_tcall/status"
#define FIREBASE_TELEMETRY_PATH_DEFAULT "/ttgo_tcall/telemetry"

// ntfy defaults
// User-facing notifications (incoming SMS/calls, package events).
#define NTFY_URL_DEFAULT "https://ntfy.innovorix.com/oracle_ntfy"
// Operational log channel (job status: pending/processing/sent/failed + errors).
// Subscribe + mute this one; it is chatty by design.
#define NTFY_LOG_URL_DEFAULT "https://ntfy.innovorix.com/ttgo_stuff"

// ThingSpeak defaults
#define THINGSPEAK_CHANNEL_ID_DEFAULT 0UL
#define THINGSPEAK_WRITE_API_KEY_DEFAULT "YOUR_THINGSPEAK_WRITE_API_KEY"

// Polling and limits
#define POLLING_INTERVAL_SECONDS_DEFAULT 3
#define DAILY_SMS_LIMIT_DEFAULT 200
#define WEEKLY_SMS_LIMIT_DEFAULT 950
#define MONTHLY_SMS_LIMIT_DEFAULT 4900

// Logging defaults
#define LOG_VERBOSE_DEFAULT 1

#endif
