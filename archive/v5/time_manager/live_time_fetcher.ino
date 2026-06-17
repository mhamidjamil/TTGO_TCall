#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// Replace with your network credentials
const char *ssid = MY_SSID;
const char *password = MY_PASSWORD;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Variables to save date and time
String formattedDate;

struct RTC {
  // Final data : 23/08/26,05:38:34+20
  int year = 0; // year will be 23
  int milliSeconds = 0;
  int month = 0;   // month will be 08
  int date = 0;    // date will be 26
  int hour = 0;    // hour will be 05
  int minutes = 0; // minutes will be 38
  int seconds = 0; // seconds will be 34
};
RTC myRTC;

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize a NTPClient to get time
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  timeClient.setTimeOffset(18000);
}
void loop() {
  int i = 100;
  while (!timeClient.update() && i > 0) {
    timeClient.forceUpdate();
    i -= 10;
    delay(100);
  }
  // The formattedDate comes with the following format:
  // 2018-05-28T16:00:13Z
  // We need to extract date and time
  formattedDate = timeClient.getFormattedDate();
  updateOnlineTime(formattedDate, myRTC);
  Serial.println("Update time: " + getRTC_Time());
  Serial.println("Updated RTC values:");
  Serial.printf("Year: %02d ", myRTC.year);
  Serial.printf("Month: %02d ", myRTC.month);
  Serial.printf("Date: %02d ", myRTC.date);
  Serial.printf("Hour: %02d ", myRTC.hour);
  Serial.printf("Minutes: %02d ", myRTC.minutes);
  Serial.printf("Seconds: %02d\n", myRTC.seconds);
  delay(1000);
}

void updateOnlineTime(String formattedDate, RTC &rtc) {
  sscanf(formattedDate.c_str(), "%d-%d-%dT%d:%d:%dZ", &rtc.year, &rtc.month,
         &rtc.date, &rtc.hour, &rtc.minutes, &rtc.seconds);
  rtc.year = rtc.year % 100;
}

String getRTC_Time() {
  return (String(myRTC.hour) + ":" + String(myRTC.minutes) + ":" +
          String(myRTC.seconds) + " ___ " + String(myRTC.month) + "/" +
          String(myRTC.date) + "/" + String(myRTC.year));
}
