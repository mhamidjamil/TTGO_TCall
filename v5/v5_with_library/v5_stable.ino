//$ last work 17/March/25
// # version 5.7.6 Utilize smsSend function

#include "arduino_secrets.h"

const char simPIN[] = "";

String MY_NUMBER = PHONE_NUMBER;
String byPass_key_from_orangePi = "";

String whatsapp_numbers[4] = {WHATSAPP_NUMBER_1, WHATSAPP_NUMBER_2,
                              WHATSAPP_NUMBER_3, WHATSAPP_NUMBER_4};

String API[4] = {WHATSAPP_API_1, WHATSAPP_API_2, WHATSAPP_API_3,
                 WHATSAPP_API_4};

const char *mqtt_server = MY_MQTT_SERVER_IP;

String server = "https://api.callmebot.com/whatsapp.php?phone=";

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800   // Modem is SIM800
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb

#include <TinyGsmClient.h>
#include <Wire.h>
//`===================================
#include <DHT.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include <ThingSpeak.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <random>

#include "MyWebServer.h"
#include "SPIFFS.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
String formattedDate;
//`===================================

//% pin details
//  TTGO T-Call pins
#define MODEM_RST 5
#define MODEM_PWKEY 4
#define MODEM_POWER_ON 23
#define MODEM_TX 27
#define MODEM_RX 26
#define I2C_SDA 21
#define I2C_SCL 22
#define LED                                                                    \
  13 // indicate that data is uploaded successfully last time on thingSpeak

#define DHTPIN 33 // Change the pin if necessary
DHT dht(DHTPIN, DHT11);

#define IP5306_ADDR 0x75
#define IP5306_REG_SYS_CTL0 0x00

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define SerialMon Serial
#define SerialAT Serial1
#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

#define IP5306_ADDR 0x75
#define IP5306_REG_SYS_CTL0 0x00

bool thingspeak_enabled = true;
bool thingsboard_enabled = true;
bool thingspeak_turn = false;
#define MAX_MESSAGES 15
#define UPDATE_THING_SPEAK_TH_AFTER 5
#define ALLOW_CREATING_NEW_VARIABLE_FILE true
#define CONFIG_FILE "/config.txt"
const String relayControllerURL = "http://192.168.0.7/?minute";
#define SEND_MSG_ON_REBOOT true

bool setPowerBoostKeepOn(int en) {
  Wire.beginTransmission(IP5306_ADDR);
  Wire.write(IP5306_REG_SYS_CTL0);
  if (en) {
    Wire.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
  } else {
    Wire.write(0x35); // 0x37 is default reg value
  }
  return Wire.endTransmission() == 0;
}

const char *ssid = MY_SSID;
const char *password = MY_PASSWORD;
// ThingSpeak parameters:
const unsigned long ts_channel_id = MY_CHANNEL_ID;
const char *ts_api_key = THINGSPEAK_API;

#define TS_MSG_COUNTER_FIELD 5
#define TS_MSG_SEND_DATE_FIELD 6
#define TS_PACKAGE_EXPIRY_DATE 7
int whatsapp_message_number = -1;
unsigned int last_ts_update_time =
    2; // TS will update when current time >= this variable
unsigned int last_update = 0; // in minutes

WiFiClient espClient;
PubSubClient client(espClient);

String end_value = "=";
String line_1 = "098 ";
String line_2 = "786";
String last_line = "Not initialized";

String received_message = ""; // Global variable to store received message
double battery_voltage = 0;
int battery_percentage = 0;
int messages_in_inbox = 0;
int messageStack[MAX_MESSAGES] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int current_target_index = 0;

long duration;    // variable for the duration of sound wave travel
int distance = 0; // variable for the distance measurement

float temperature;
int humidity;

bool DEBUGGING = true;
#define DEBUGGING_OPTIONS 8
int allowed_debugging[DEBUGGING_OPTIONS] = {0, 0, 1, 0, 1, 0, 0, 1};
// 0 => allowed, 1 => not allowed
// (debuggerID == 0)     // debugging WIFI_debug functionality
// (debuggerID == 1) // debugging LCD_debug functionality
// (debuggerID == 2) // debugging SIM800L_debug functionality
// (debuggerID == 3) // debugging THINGSPEAK_debug functionality
// (debuggerID == 4) // debugging WHATSAPP_debug functionality
// (debuggerID == 5) // debugging BLE_debug functionality
// (debuggerID == 6) // debugging SPIFFS_debug functionality
// (debuggerID == 7) // debugging Orange-pi functionality

int debug_for = 130; // mentioned in seconds
bool ultraSoundWorking = false;
bool wifi_working = true;
bool display_working = true;
bool sms_allowed = false;
bool message_saving_mode = false;
int package_expiry_date = 0;
unsigned int BLE_last_receive_time = 0;
// 10921 => 09 = month, 21 = day

String messageTemplate = "#setting <ultra sound alerts 0> <display 1> <wifi "
                         "connectivity 1> <sms 1> <termination time 300> "
                         "<battery charge time 1800>";

int termination_time = 60 * 5; // 5 minutes
String rtc = "";               // real Time Clock
String BLE_Input = "";
String BLE_Output = "";

// int batteryChargeTime = 30;   // 30 minutes
// unsigned int wapdaInTime = 0; // when wapda is on it store the time stamp.
// bool batteriesCharged = false;

String ret_string = "";
String OWNER_NAME = DEVELOPER;
String error_codes = "";
String company_numbers = "";
// String chargingStatus = "";
// these error codes will be moving in the last row of lcd

bool mqtt_connected = false;
bool pending_test_message = false;
String demo_message_index, demo_message_number, demo_message_content;

String pi_incoming = "";

struct RTC {
  // Final data: 23/08/26,05:38:34+20
  int year = 0; // year will be 23
  int milliSeconds = 0;
  int month = 0;   // month will be 08
  int date = 0;    // date will be 26
  int hour = 0;    // hour will be 05
  int minutes = 0; // minutes will be 38
  int seconds = 0; // seconds will be 34
};
RTC myRTC;

MyWebServer myServer(80);

// # ......... functions > .......
void println(String str);
void Println(String str);
void print(String str);
void say(String str);
void giveMissedCall();
void call(String number);
void sendSMS(String sms);
void sendSMS(String sms, String number);
void updateSerial();
void moduleManager();
void getLastMessageAndIndex(String response, String &lastMessage,
                            int &messageNumber);
String getResponse();
int getNewMessageNumber(String response);
String readMessage(int index);
String removeOk(String str);
String executeCommand(String str);
String updateBatteryStatus();
void updateBatteryParameters(String response);
bool timeOut(unsigned int sec, unsigned int entrySec);
void forwardMessage(int index);
String getMobileNumberOfMsg(String index);
int totalUnreadMessages(); //! depreciate it or execute once
void terminateLastMessage();
bool checkStack(int messageNumber);
int getIndex(int messageNumber);
void arrangeStack();
void deleteIndexFromStack(int messageNumber);
int getLastIndexToTerminate();
int lastMessageIndex();
int getMessageNumberBefore(int messageNumber);
int firstMessageIndex();
String sendAllMessagesWithIndex();
bool messageExists(int index);
void connect_wifi();
void updateThingSpeak(float temperature, int humidity);
void successMsg();
void errorMsg();
bool wifiConnected();
void lcdPrint();
String thingSpeakLastUpdate();
void drawWifiSymbol(bool isConnected);
void updateDistance();
bool changeDetector(int newValue, int previousValue, int margin);
String getVariablesValues();
void updateVariablesValues(String str);
int findOccurrences(String str, String target);
void wait(unsigned int miliSeconds);
void setTime(String timeOfMessage);
void setTime();
String fetchDetails(String str, String begin_end, int padding);
void updateRTC();
void Delay(int milliSeconds);
bool isNum(String num);
void deleteMessages(String index);
void inputManager(String input, int inputFrom); // 1 = BLE, 2 = Loop, 3 = SMS
// void initBLE();
void BLE_inputManager(String input);
bool isNum(String num);
bool wapdaOn();
void backup(String state);
void chargeBatteries(bool charge);
String getCompleteString(String str, String target);
int fetchNumber(String str);
bool companyMsg(String);
void sendWhatsappMsg(String message);
String getServerPath(String message);
int getMessagesCounter();
int getThingSpeakFieldData(int fieldNumber);
void updateWhatsappMessageCounter();
void setThingSpeakFieldData(int field, int data);
void writeThingSpeakData();
unsigned int getMint();
String getHTTPString(String message);
//! outdated
// TODO: update functions and add there description
//  # ......... < functions .......
// #include <BLEDevice.h>
// #include <BLEServer.h>
// #include <BLEUtils.h>

// BLEServer *pServer = NULL;
// BLECharacteristic *pCharacteristic = NULL;
// bool deviceConnected = false;
// bool oldDeviceConnected = false;
// std::string receivedData = "";

// class MyServerCallbacks : public BLEServerCallbacks {
//   void onConnect(BLEServer *pServer) { deviceConnected = true; }

//   void onDisconnect(BLEServer *pServer) { deviceConnected = false; }
// };

// class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
//   void onWrite(BLECharacteristic *pCharacteristic) {
//     std::string value = pCharacteristic->getValue();
//     if (value.length() > 0) {
//       receivedData = value;
//       Serial.print("Received from BLE: ");
//       Serial.println(receivedData.c_str());
//       BLE_inputManager(String(receivedData.c_str()));

//       // Print the received data back to the BLE connection
//       receivedData = BLE_Output.c_str();
//       pCharacteristic->setValue("BLE: " + receivedData);
//       pCharacteristic->notify();
//     }
//   }
// };

// void BLE_inputManager(String input) {
//   bool time_out =
//       BLE_Input != "" && (10 < ((millis() / 1000) - BLE_last_receive_time));
//   if (input.indexOf("#") != -1 || time_out) {
//     // means string is now fetched completely
//     if (!isIn(input, "_nil_"))
//       BLE_Input += input.substring(0, input.indexOf("#"));
//     println("Executing (BLE input): {" + BLE_Input + "}");
//     inputManager(removeNewline(BLE_Input), 1);
//     BLE_Input = "";
//     BLE_last_receive_time = 0;
//   } else if (input.length() > 0 && !isIn(input, "_nil_")) {
//     if (BLE_Input == "")
//       BLE_last_receive_time = (millis() / 1000);
//     BLE_Input += input;
//     if (BLE_Input.length() > 50) {
//       println("BLE input is getting to large , here is the current string ("
//       +
//               BLE_Input + ") flushing it...");
//       BLE_Input = "";
//     }
//   } else {
//     if (!isIn(input, "_nil_"))
//       rise("Unexpected flow in BLE input manager", "324");
//   }
// }

//! * # # # # # # # # # # # # * !

void setup() {
  SerialMon.begin(115200);
  Println("####  Setup ####");
  // initBLE();
  Println("After BLE init");
  Wire.begin(I2C_SDA, I2C_SCL);
  bool isOk = setPowerBoostKeepOn(1);
  println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));
  // Set modem reset, enable, power pins
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  println("Initializing modem...");
  delay(3000);
  modem.restart();
  delay(2000);
  modem.sendAT(GF("+CLIP=1"));
  delay(300);
  modem.sendAT(GF("+CMGF=1"));
  delay(2000);

  Println("Before Display functionality");
  //`...............................
  delay(2000);
  if (display_working) {
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      Serial.println(F("SSD1306 allocation failed"));
      display_working = false;
    } else {
      delay(300);
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 0);
      // Display static text
      display.println("Boot Code:  " + String(random(99)));
      display.display();
      delay(500);
    }
  }
  Println("After display functionality");
  delay(500);
  dht.begin(); // Initialize the DHT11 sensor
  delay(1000);
  Println("Before wifi connection");
  if (wifi_working) {
    WiFi.begin(ssid, password);
    println("Connecting");
    int i = 0;
    while (WiFi.status() != WL_CONNECTED) {
      if (i > 10) {
        println("\n!!!---Timeout: Unable to connect to WiFi---!!!");
        wifi_working = false;
        break;
      }
      delay(500);
      print(".");
      i++;
    }
    if (i < 10) {
      println("");
      print("Connected to WiFi network with IP Address: ");
      Serial.println(WiFi.localIP());
      timeClient.begin();
      timeClient.setTimeOffset(18000);
    }
  }
  delay(500);
  Println("after wifi connection");
  syncSPIFFS(); // use it to update global variables from SPIFFS
  Println("After SPIFFS sync");
  pinMode(LED, OUTPUT);
  delay(100);
  if (wifi_working) {
    myServer.begin();
    initThingSpeak();
  }
  Println("\nAfter ThingSpeak");
  //`...............................
  Println("Leaving setup with seconds: " + String(millis() / 1000));
  sms_allowed = hasPackage();
  Println("before syncSPIFFS: ");
  Println("after syncSPIFFS: ");
  updateTime();
  Println("after time request");
  thingsboard_enabled ? initThingsBoard() : alert("Thingsboard is not enabled");
  SEND_MSG_ON_REBOOT ? sendSMS("Device rebooted at: " + getRTC_Time())
                     : Println("");
  askOrangPi("send bypass key");
}

void loop() {
  // handleAPIS();
  Println("in loop");
  // if (BLE_Input != "")
  //   BLE_inputManager("_nil_");
  // client.loop(); // ensure that the MQTT client remains responsive
  // if (deviceConnected) {
  //   if (!oldDeviceConnected) {
  //     Serial.println("Connected to device");
  //     oldDeviceConnected = true;
  //   }
  // } else {
  //   if (oldDeviceConnected) {
  //     Serial.println("Disconnected from device");
  //     Delay(500);
  //     pServer->startAdvertising(); // restart advertising
  //     Serial.println("Restart advertising");
  //     oldDeviceConnected = false;
  //   }
  // }

  if (pi_incoming.length() >= 1) {
    String tempStr = pi_incoming;
    pi_incoming = "";
    inputManager(tempStr, 4);
  }

  Delay(100);
  if (SerialMon.available()) {
    String command = SerialMon.readString();
    inputManager(command, 2);
  }
  Println("after serial test");
  if (SerialAT.available() > 0)
    Serial.println(getResponse());
  Println("after AT check");
  Delay(100);
  //`..................................
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  Println("after DHT reading");
  if (display_working) {
    char temperatureStr[5];
    dtostrf(temperature, 4, 1, temperatureStr);
    Println("after DHT conversion");

    line_1 = String(temperatureStr) + " C " + String(humidity) + " % " +
             thingSpeakLastUpdate();
    line_2 = last_line + " " + end_value;

    Println("before lcd call");
    Delay(100);
    lcdPrint();
  }
  Println("after lcd update");
  wait(100);
  //`..................................

  // #----------------------------------
  wait(1000);
  Println("loop end");
}

void println(String str) {
  SerialMon.println(str);
  BLE_Output = str;
  // receivedData = BLE_Output.c_str();
  // pCharacteristic->setValue("BLE: " + receivedData);
  // pCharacteristic->notify();
}

void Println(String str) {
  if (DEBUGGING) {
    Serial.println(str);
  }
}

void Print(String str) {
  if (DEBUGGING) {
    Serial.print(str);
  }
}

void Println(int debuggerID, String str) {
  if (DEBUGGING) {
    println(str);
  } else if (allowed_debugging[debuggerID - 1]) {
    println(str);
  } else if (debuggerID < 0 || debuggerID > DEBUGGING_OPTIONS) {
    Serial.println("Debugger is not defined for this string: " + str);
  }
}

void print(String str) {
  SerialMon.print(str);
  BLE_Output = str;
  // receivedData = BLE_Output.c_str();
  // pCharacteristic->setValue("BLE: " + receivedData);
  // pCharacteristic->notify();
}

void say(String str) { SerialAT.println(str); }

void giveMissedCall() {
  say("ATD+ " + MY_NUMBER + ";");
  updateSerial();
  // Delay(20000);            // wait for 20 seconds...
  // say("ATH"); // hang up
  // updateSerial();
}

void hangUp() { say("AT+CHUP"); }

void call(String number) {
  say("ATD+ " + number + ";");
  updateSerial();
}

void sendSMS(String sms) {
  sendSMS(sms, MY_NUMBER);
}

void sendSMS(String sms, String number) {
  if (!sms_allowed) {
    println("SMS sending is not allowed");
    return;
  }

  if (modem.sendSMS(number, sms)) {
    println("sending: [" + sms + "] to: " + number);
    saveItOrangePi("Message sent to: {" + number + "}, message: [" + sms + "]");
  } else {
    println("SMS failed to send");
    println("\n!send{" + sms + "}!\n");
  }

  Delay(500);
}

void updateSerial() {
  Delay(100);
  while (SerialMon.available()) {
    SerialAT.write(SerialMon.read());
  }
  while (SerialAT.available()) {
    SerialMon.write(SerialAT.read());
  }
}

void moduleManager() {
  // this function will check if there is any unread message or not
  // store response of AT+CMGL="ALL" in a string 1st
  say("AT+CMGL=\"ALL\"");
  String response = getResponse();
  println("********************\n" + response + "\n********************");
  String lastMessage;
  int messageNumber;
  getLastMessageAndIndex(response, lastMessage, messageNumber);

  println("Last CMGL number: " + String(messageNumber));
  println("Last message: " + lastMessage);
}

void getLastMessageAndIndex(String response, String &lastMessage,
                            int &messageNumber) {
  // Find the last occurrence of "+CMGL:" in the response
  int lastCmglIndex = response.lastIndexOf("CMGL:");

  // Find the next occurrence of "+CMGL:" after the last occurrence
  int nextCmglIndex = response.indexOf("+CMGL:", lastCmglIndex + 1);
  // Extract the last message and its CMGL number
  String lastCmgl = response.substring(lastCmglIndex, nextCmglIndex);
  int commaIndex = response.indexOf(",", lastCmglIndex);

  messageNumber =
      response
          .substring(lastCmglIndex + 6, response.indexOf(",", lastCmglIndex))
          .toInt();

  // Extract the message content
  int messageStartIndex = lastCmgl.lastIndexOf("\"") + 3;
  lastMessage = lastCmgl.substring(messageStartIndex);
}

String getResponse() {
  Println(3, "entering getResponse");
  Delay(100);
  String response = "";
  if ((SerialAT.available() > 0)) {
    response += SerialAT.readString();
  }
  Println(3, "After while loop in get response");
  if (response.indexOf("+CMTI:") != -1 || pending_test_message) { // new message
    Println(3, "new message content :{" + response + "}");
    int newMessageNumber;
    String senderNumber, _message_;
    messages_in_inbox++;
    if (pending_test_message) {
      // pending_test_message = false;
      newMessageNumber = demo_message_index.toInt();
      senderNumber = demo_message_number;
      _message_ = demo_message_content;
    } else {
      newMessageNumber = getNewMessageNumber(response);
      senderNumber = getMobileNumberOfMsg(String(newMessageNumber), true);
      _message_ = (removeNewline(removeOk(readMessage(newMessageNumber))));
    }
    if (!isIn(senderNumber, "3374888420")) {
      // if message is not send by module it self
      String messageState = "";
      bool authenticSender = senderIsAuthentic(senderNumber, _message_);
      if (authenticSender)
        messageState = executeCommand(_message_);
      else
        println("Unauthorize sender, message {" + _message_ + "} not executed");
      println("New message [ " + _message_ + "]");
      if (isIn(messageState, "<executed>"))
        deleteMessage(newMessageNumber, _message_, senderNumber);
      else { // message is not executed
        if (!companyMsg(senderNumber)) {
          sendSMS("<Unable to execute sms no. {" + String(newMessageNumber) +
                  "} message: > [ " + removeNewline(_message_) +
                  " ] from: " + senderNumber);
          authenticSender
              ? toOrangePi("untrained_message: _[_" + removeNewline(_message_) +
                           (pending_test_message ? "isDemo" : "") +
                           "_]_ from: _{_" + senderNumber + "_}_ index: _(_" +
                           String(newMessageNumber) + "_)_")
              : log("skipping unauthorized message from: " + senderNumber);
        } else {
          Println(3, "Company message received deleting it...");
          if (isValidString(_message_)) {
            sendSMS("<Company message received sms no. {" +
                    String(newMessageNumber) + "} message: > [ " + _message_ +
                    " ] from: " + senderNumber + ". deleting it...");
            if (newPackageSubscribed(_message_))
              updatePackageSubscribedDate();
          } else
            print("Message not send as it marked as HEX message");
          if (sms_allowed)
            deleteMessage(newMessageNumber, _message_, senderNumber);
        }
      }
    } else {
      Println(3, "skipping message from: " + senderNumber);
      deleteMessage(newMessageNumber, _message_, senderNumber);
    }
    pending_test_message = false;
  } else if (response.indexOf("+CLIP:") != -1) { // miss call
    //+CLIP: "03354888420",161,"",0,"",0
    hangUp();
    delay(500);
    String callerNumber = fetchDetails(response, "\"", 1);
    String temp_str = "Missed call from: " + callerNumber;
    if (callerNumber.length() > 10 &&
        (callerNumber.startsWith("+92") || callerNumber.startsWith("03"))) {
      delay(200);
      sendSMS("AOA, this is " + OWNER_NAME +
                  "'s secondary number. This message is auto-generated, as I "
                  "am currently unable to take calls on this device. For "
                  "urgent matters, please contact me at " +
                  MY_NUMBER + ". Thank you for your understanding.",
              callerNumber);
      temp_str += " asked him to contact you.";
    } else {
      temp_str += " unable to ask him to contact you!";
    }
    sendSMS(temp_str);

    // println(temp_str);
  }
  Println(3, "Leaving response function with response [" + response + "]");
  return response;
}

int getNewMessageNumber(String response) {
  int message_number =
      response.substring(response.lastIndexOf(",") + 1, -1).toInt();
  return message_number;
}

String readMessage(int index) { // read the message of given index
  say("AT+CMGR=" + String(index));
  String tempStr = getResponse();
  Println(3, "message i read: " + tempStr);
  tempStr = tempStr.substring(tempStr.lastIndexOf("\"") + 2);
  Println("message i convert in stage 1: " + tempStr);
  // remove "\n" from start of string
  tempStr = tempStr.substring(tempStr.indexOf("\n") + 1);
  Println("message i convert in stage 2: " + tempStr);
  return tempStr;
}

void deleteMessage(int index, String message, String mobileNumber) {
  String message_details =
      (" message [" + message + "] send by: {" + mobileNumber + "}");
  deleteMessage(index) ? saveItOrangePi("Module is deleting " + message_details)
                       : log("Unable to delete " + message_details);
}

bool deleteMessage(int index) {
  if (index == 1) {
    println("you are not allowed to delete message of index 1");
    return false;
  }
  if (!sms_allowed && index > 0) {
    println(
        "Unable to delete message because message_sending is not allowed, if "
        "you really want to delete message then send index in negative number");
    return false;
  } else if (index < 0) {
    index *= -1;
  }
  if (pending_test_message)
    return true;
  say("AT+CMGD=" + String(index));
  println(getResponse());
  messages_in_inbox--;
  arrangeStack();
  return true;
}

String removeOk(String str) {
  if (str.lastIndexOf("OK") != -1)
    return str.substring(0, str.lastIndexOf("OK") - 2);
  else
    return str;
}

String removeEnter(String str) {
  str.trim();
  return str;
}

String executeCommand(String str) {
  ret_string = "";
  Println(3, "working on msg: " + str);
  inputManager(str, 3);
  if (ret_string.indexOf("<executed>") == -1) { // command is not executed
    println("-> Module is not trained to execute this command ! <-");
  }
  return ret_string;
}

String updateBatteryStatus() {
  say("AT+CBC");
  return getResponse();
}

void updateBatteryParameters(String response) {
  // get +CBC: 0,81,4049 in response
  battery_percentage =
      response.substring(response.indexOf(",") + 1, response.lastIndexOf(","))
          .toInt();
  int milliBatteryVoltage =
      response.substring(response.lastIndexOf(",") + 1, -1).toInt();
  battery_voltage =
      milliBatteryVoltage / pow(10, (String(milliBatteryVoltage).length() - 1));
}

bool timeOut(unsigned int sec, unsigned int entrySec) {
  if ((millis() / 1000) - entrySec > sec)
    return true;
  else
    return false;
}

void forwardMessage(int index) {
  String message = removeOk(readMessage(index));
  if (messageExists(index))
    sendSMS(message);
  else
    sendSMS("No message found at index: " + String(index));
}

String getMobileNumberOfMsg(String index, bool newMessage) {
  say("AT+CMGR=" + index);        // ask to send the message at index
  String tempStr = getResponse(); // read message at index and save it
  if (newMessage) {
    setTime(tempStr);
    if ((tempStr.indexOf("3374888420") != -1 && index.toInt() != 1) ||
        tempStr.indexOf("setTime") != -1)
      bool temp_bool = deleteMessage(
          index.toInt()); // delete message because it was just to set time
  }
  //+CMGR: "REC READ","+923354888420","","23/07/22,01:02:28+20"
  return fetchDetails(tempStr, ",", 2);
}

int totalUnreadMessages() {
  say("AT+CMGL=\"ALL\"");
  String response = getResponse();
  int count = 0;
  for (int i = 0; i < response.length(); i++) {
    if (response.substring(i, i + 5) == "+CMGL")
      count++;
  }
  return count;
}

void terminateLastMessage() {
  println("\n\t*** terminator job start ***\n");
  current_target_index = getLastIndexToTerminate();
  if (current_target_index == firstMessageIndex() || current_target_index <= 1)
    return;
  println("work index: " + String(current_target_index));
  String _message_ = (removeOk(readMessage(current_target_index)));
  String mobileNumber =
      getMobileNumberOfMsg(String(current_target_index), true);
  if (senderIsAuthentic(mobileNumber, _message_))
    _message_ = executeCommand(_message_);
  else
    println("Terminator has skiped message: {" + String(current_target_index) +
            "} Because it is not supposed to be executed #771");
  println("Last message [ " + _message_ + "]");
  if (_message_.indexOf("<executed>") != -1) {
    deleteMessage(current_target_index, _message_, mobileNumber);
    println("Message {" + String(current_target_index) + "} deleted");
  } else { // if the message don't execute
    saveItOrangePi("#863 Deleting message: {" + _message_ + "} from: [" +
                   mobileNumber + "]");
    bool temp_status = deleteMessage(-1 * current_target_index);
    saveItOrangePi("Message: (" + String(current_target_index) + ") was " +
                   (temp_status ? "successfully" : "not") + " deleted");
  }
  println("\n\t*** terminator job end ***\n");
}

bool checkStack(int messageNumber) {
  if (getIndex(messageNumber) == -1) {
    for (int i = 0; i < MAX_MESSAGES; i++)
      if (messageStack[i] != 0) {
        messageStack[i] = messageNumber;
        return true;
      }
    println("\n#Error 784\n");
    addError("784");
    return false;
  } else {
    return false;
  }
}

int getIndex(int messageNumber) {
  for (int i = 0; i < MAX_MESSAGES; i++)
    if (messageStack[i] == messageNumber)
      return i;
  return -1;
}

void arrangeStack() {
  for (int i = 0; i < MAX_MESSAGES; i++)
    if (messageStack[i] == 0)
      for (int j = i + 1; j < MAX_MESSAGES; j++)
        if (messageStack[j] != 0) {
          messageStack[i] = messageStack[j];
          messageStack[j] = 0;
          break;
        }
}

void deleteIndexFromStack(int messageNumber) {
  messageStack[getIndex(messageNumber)] = 0;
  arrangeStack();
}

int getLastIndexToTerminate() {
  arrangeStack();
  if (current_target_index == 0) {
    // mean this function is running first time lets read all messages and get
    // the last message number
    messageStack[0] = lastMessageIndex();
    return messageStack[0];
  } else {
    int targetedIndex = 0;
    for (; targetedIndex < MAX_MESSAGES; targetedIndex++)
      if (messageStack[targetedIndex + 1] == 0)
        break;
    // here we got the index where we now have to store the message number
    messageStack[targetedIndex] =
        getMessageNumberBefore(messageStack[targetedIndex - 1]);
    return messageStack[targetedIndex];
  }
}

int lastMessageIndex() {
  say("AT+CMGL=\"ALL\"");
  String response = getResponse();
  String tempStr = response.substring(response.lastIndexOf("+CMGL:"), -1);
  int targetNumber =
      tempStr.substring(tempStr.indexOf("CMGL:") + 6, tempStr.indexOf(","))
          .toInt();
  return targetNumber;
}

int getMessageNumberBefore(int messageNumber) {
  say("AT+CMGL=\"ALL\"");
  String response = getResponse();
  String responseBeforeThatIndex = response.substring(
      0, response.indexOf("+CMGL: " + String(messageNumber)));
  String tempStr = responseBeforeThatIndex.substring(
      responseBeforeThatIndex.lastIndexOf("+CMGL:"),
      responseBeforeThatIndex.lastIndexOf("\""));
  int targetValue =
      tempStr.substring(tempStr.indexOf("CMGL: ") + 6, tempStr.indexOf(","))
          .toInt();
  return targetValue;
}

int firstMessageIndex() {
  say("AT+CMGL=\"ALL\"");
  String response = getResponse();
  int targetValue =
      response
          .substring(response.lastIndexOf("CMGL:") + 6, response.indexOf(","))
          .toInt();
  return targetValue;
}

String sendAllMessagesWithIndex() {
  String tempStr = "";
  int end_ = lastMessageIndex();
  int start_ = firstMessageIndex();
  for (int i = start_; i <= end_; i++) {
    if (messageExists(i)) {
      tempStr = String(i) + ": " + readMessage(i);
      sendSMS(tempStr);
    }
  }
  return tempStr;
}

bool messageExists(int index) {
  say("AT+CMGL=\"ALL\"");
  String response = getResponse();
  if (response.indexOf("+CMGL: " + String(index)) != -1)
    return true;
  else
    return false;
}

//`..................................
void connect_wifi() {
  if (String(ssid).indexOf("skip") != -1 && wifi_working) {
    return;
  }
  WiFi.begin(ssid, password); // Connect to Wi-Fi
  for (int i = 0; !wifiConnected(); i++) {
    if (i > 10) {
      println("Timeout: Unable to connect to WiFi");
      break;
    }
    Delay(500);
  }
  if (wifiConnected()) {
    println("Wi-Fi connected successfully");
  } else {
    digitalWrite(LED, LOW);
  }
}

void reconnect() {
  int i = 0;
  while (!client.connected() && i++ < 5) { // try for 10 seconds
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client", "ts53yrn2f09kpcignf8t", NULL)) {
      Serial.println("connected");
      mqtt_connected = false;
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      mqtt_connected = false;
      wait(2000);
    }
  }
}

void updateThingSpeak(float temperature, int humidity) {
  if (humidity < 110) {
    ThingSpeak.setField(1, temperature); // Set temperature value
    ThingSpeak.setField(2, humidity);    // Set humidity value
    Println(4, "After setting up fields");

    int updateStatus = ThingSpeak.writeFields(ts_channel_id, ts_api_key);
    if (updateStatus == 200) {
      Println(4, "ThingSpeak update successful");
      successMsg();
    } else {
      Println(4, "Error updating ThingSpeak. Status: " + String(updateStatus));
      errorMsg();
    }
  } else {
    digitalWrite(LED, LOW);
  }
}

void successMsg() {
  // set curser to first row, first last column and print "tick symbol"
  digitalWrite(LED, HIGH);
  end_value = "+";
  last_update = (millis() / 1000);
}

void errorMsg() {
  // set curser to first row, first last column and print "tick symbol"
  digitalWrite(LED, LOW);
  end_value = "-";
  connect_wifi();
}

bool wifiConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  } else {
    return false;
  }
}

void lcdPrint() {
  Println(2, "entering lcd function");
  display.clearDisplay();
  Println(2, "second line of lcd function");
  display.setTextSize(1);
  display.setCursor(0, 0);
  // Display static text
  Println(2, "before checking wifi status lcd function");
  drawWifiSymbol(wifiConnected());
  Println(2, "after checking wifi status lcd function");
  if (sms_allowed)
    display.print("_A");
  else
    display.print("_N");
  if (messages_in_inbox > 1) {
    display.print(String(messages_in_inbox));
    display.print(" ");
  }
  display.print(battery_percentage);
  display.print("%");
  display.print(" ");
  display.print(String(myRTC.hour) + ":" + String(myRTC.minutes) + ":" +
                String(myRTC.seconds));

  display.setCursor(0, 17);
  display.print(line_1);

  display.setCursor(0, 32);
  display.print(line_2);

  if (error_codes.length() > 0) {
    display.setCursor(0, 47);
    display.print(error_codes);
  }

  display.display();
  Delay(1000);
  Println(2, "leaving lcd function");
}

String thingSpeakLastUpdate() {
  Println("entering time function");
  unsigned int sec = (millis() / 1000) - last_update;
  if (sec < 60) {
    return (String(sec) + " s");
  } else if ((sec >= 60) && (sec < 3600)) { // deal one hour
    return (String(sec / 60) + " m " + String(sec % 60) + " s");
  } else if ((sec >= 3600) && (sec < 86400)) { // deal one day
    return (" " + String(sec / 3600) + " h " + String((sec % 3600) / 60) +
            " m");
  } else if ((sec >= 86400) && (sec < 604800)) { // deal one week
    return (String(sec / 86400) + " d " + String((sec % 86400) / 3600) + " h ");
  } else {
    println("Issue spotted sec value: " + String(sec));
    sendSMS("Got problem in time function time overflow (or module runs for "
            "more than a week want to reboot send #reboot sms)");
    println("rebooting module [thingSpeakLastUpdate()] ...");
    return String(-1);
  }
  return "X";
}

void drawWifiSymbol(bool isConnected) {
  if (!isConnected) {
    if (!wifi_working)
      display.print("D");
    else
      display.print("X");
  } else {
    display.print("C");
  }
}
//`..................................

bool changeDetector(int newValue, int previousValue, int margin) {
  if ((newValue >= previousValue - margin) &&
      (newValue <= previousValue + margin)) {
    return false;
  } else {
    return true;
  }
}

String getVariablesValues() {
  return String(
      (String(display_working ? "Display on" : "Display off") + ", " +
       String(ultraSoundWorking ? "ultraSound on" : "ultraSound off") + ", " +
       String(wifi_working ? "wifi on" : "wifi off")) +
      " Termination time: " + String(termination_time) +
      "sms allowed: " + String(sms_allowed));
}

void updateVariablesValues(String str) {
  int newValues = findOccurrences(str, "<");
  if (newValues == 0) {
    println("No new value to be update");
    println("response received to work on: [" + str + "]");
    return;
  } else {
    println("Updating " + String(newValues) + " values");
    if (str.indexOf("ultra") != -1) {
      int response = fetchNumber(getCompleteString(str, "ultra"));
      if (response == 0) {
        ultraSoundWorking = false;
      } else if (response == 1) {
        ultraSoundWorking = true;
      }
    }
    if (str.indexOf("display") != -1) {
      int response = fetchNumber(getCompleteString(str, "display"));
      if (response == 0) {
        display_working = false;
      } else if (response == 1) {
        display_working = true;
      }
    }
    if (str.indexOf("sms") != -1) {
      int response = fetchNumber(getCompleteString(str, "sms"));
      if (response == 0) {
        sms_allowed = false;
      } else if (response == 1) {
        sms_allowed = true;
      }
    }
    if (str.indexOf("wifi") != -1) {
      int response = fetchNumber(getCompleteString(str, "wifi"));
      if (response == 0) {
        wifi_working = false;
      } else if (response == 1) {
        wifi_working = true;
        if (thingspeak_enabled && wifiConnected()) {
          Println(4, "\nThinkSpeak initializing in loop...\n");
          ThingSpeak.begin(espClient); // Initialize ThingSpeak
        }
      }
    }
    if (str.indexOf("termination") != -1) {
      termination_time = fetchNumber(getCompleteString(str, "termination"));
    }
    // if (str.indexOf("battery") != -1) {
    //   batteryChargeTime = fetchNumber(getCompleteString(str, "battery"));
    // }
  }
  println("msg: " + str + "\nAfter  update: ");
  println(getVariablesValues());
}

int findOccurrences(String str, String target) {
  int occurrences = 0;
  int index = -1;
  while ((index = str.indexOf(target, index + 1)) != -1) {
    occurrences++;
  }
  return occurrences;
}

void wait(unsigned int miliSeconds) {
  // replace Delay function with this function
  //  most important task will be executed here
  myRTC.milliSeconds += miliSeconds;
  bool condition = false;
  Println("Entering wait function...");
  for (int i = 0; (miliSeconds > (i * 5)); i++) {
    if ((millis() / 1000) % termination_time == 0 &&
        termination_time > 0) // after every 5 minutes
    {
      Println(3, "before termination");
      terminateLastMessage();
      Println(3, "after termination");
      condition = true;
      if (myRTC.date == 0 && sms_allowed) {
        println("Time is not updated yet updating it");
        updateTime() ? alert("Time updated successfully #1148")
                     : alert("Time not updated #1164");
      }
    }
    if (getMint() > last_ts_update_time) {
      // jobs which have to be execute after every 5 minutes
      if (wifiConnected()) {
        if (wifi_working && (thingspeak_enabled || thingsboard_enabled)) {
          if (thingspeak_enabled && (thingspeak_turn || !thingsboard_enabled)) {
            Println(4, "Before Temperature Update");
            updateThingSpeak(temperature, humidity);
            last_ts_update_time = getMint() + UPDATE_THING_SPEAK_TH_AFTER;
            Println(4, "After temperature update");
          }
          if (display_working) {
            updateBatteryParameters(updateBatteryStatus());
          }
          if (thingsboard_enabled &&
              (!thingspeak_turn || !thingspeak_enabled)) {
            Delay(3000);
            i += 3000;
            Println(7, "Before MQTT update");
            updateMQTT((int)temperature, humidity);
            Println(7, "After MQTT update");
          }
          thingspeak_turn = !thingspeak_turn;
        }
      } else if (thingspeak_enabled) {
        connect_wifi();
        if (wifiConnected()) {
          wifi_working = true;
          ThingSpeak.begin(espClient);            // Initialize ThingSpeak
          ThingSpeak.setField(4, random(52, 99)); // set any random value.
        }
      }
    }
    if ((millis() / 1000) > debug_for && DEBUGGING &&
        (millis() / 1000) < (debug_for + (debug_for / 2))) {
      // it will disable debugging after some time defined in "debug_for"
      // variable, because in this duration every function got chance to execute
      // at least once, so if user want to enable it then he just need to type
      // debug in serial terminal
      Println("Disabling DEBUGGING...");
      DEBUGGING = false;
    }
    if (((millis() / 1000) > (debug_for - 30)) &&
        (millis() / 1000) < (debug_for - 27)) {
      // it will just run at first time when system booted
      Println(3, "\nBefore updating values from message 1\n");
      updateVariablesValues(readMessage(1));
      Println(3, "\nValues are updated from message 1\n");
      messages_in_inbox = totalUnreadMessages();
      Delay(1000);
      i += 1000;
      condition = true;
    }
    if (condition) {
      Delay(1000);
      condition = false;
      i += 1000;
    } else
      Delay(5);
  }
  Println("leaving wait function...");
  updateRTC();
}

void setTime(String timeOfMessage) {
  // +CMGL: 1,"REC READ","+923374888420","","23/08/14,17:21:05+20"
  rtc = fetchDetails(timeOfMessage, "\"" + String(myRTC.year) + "/", "\"", 1);
  println(
      "\n+++++++++++++++++++++++++++++++++++++++++++\n Fetched data from: " +
      timeOfMessage + "\nFinal data: " + rtc +
      "\n+++++++++++++++++++++++++++++++++++++++++++\n");
  // Final data: 23/08/26,05:38:34+20
  setTime();
}

void setTime() { // this function will set myRTC struct using rtc string
  // Final data: 23/08/26,05:38:34+20
  int tempDate = myRTC.date;
  String datePart = rtc.substring(0, rtc.indexOf(",")); // 23/08/26
  Println("fetched date: " + datePart);
  myRTC.year = datePart.substring(0, datePart.indexOf("/")).toInt(); // 23
  String date_ = (datePart.substring(datePart.indexOf("/") + 1));    // 08/26
  myRTC.month = date_.substring(0, datePart.indexOf("/")).toInt();   // 08
  myRTC.date = date_.substring(datePart.indexOf("/") + 1).toInt();   // 26

  String timePart = rtc.substring(rtc.indexOf(",") + 1);       // 05:38:34+20
  String time_ = timePart.substring(0, timePart.indexOf("+")); // 05:38:34
  myRTC.hour = time_.substring(0, time_.indexOf(":")).toInt(); // 05
  myRTC.minutes =
      time_.substring(time_.indexOf(":") + 1, time_.lastIndexOf(":"))
          .toInt();                                                    // 38
  myRTC.seconds = time_.substring(time_.lastIndexOf(":") + 1).toInt(); // 34
  println("myRTC updated");
  println("--------------------------------\n");
  println("Hour: " + String(myRTC.hour) + " Minutes: " + String(myRTC.minutes) +
          " Seconds: " + String(myRTC.seconds) + " Day: " + String(myRTC.date) +
          " Month: " + String(myRTC.month));
  println("--------------------------------");
  if (tempDate == 0 && myRTC.date != 0 && ((millis() / 1000) < 30)) {
    // it will run if system is rebooted and time is set within 30 seconds
    String bootMessage = "System rebooted, Time stamp is: [ " +
                         String(myRTC.hour) + ":" + String(myRTC.minutes) +
                         ":" + String(myRTC.seconds) + " ] " +
                         String(myRTC.date) + "/" + String(myRTC.month);
    sendSMS(bootMessage);
  }
  sms_allowed = hasPackage();
}

void Delay(int milliseconds) {
  for (int i = 0; i < milliseconds; i += 10) {
    // most important task will be executed here
    delay(10);
    handleAPIS();
    if (pi_incoming.length() >= 1) {
      String tempStr = pi_incoming;
      pi_incoming = "";
      inputManager(tempStr, 4);
    }
  }

  myRTC.milliSeconds += milliseconds;
  updateRTC();
}

void updateRTC() {
  if (myRTC.milliSeconds > 1000) {
    myRTC.milliSeconds -= 1000;
    myRTC.seconds++;
  }
  if (myRTC.seconds > 59) {
    myRTC.seconds -= 60;
    myRTC.minutes++;
  }
  if (myRTC.minutes > 59) {
    myRTC.minutes -= 60;
    myRTC.hour++;
  }
  if (myRTC.hour > 23) {
    myRTC.hour -= 24;
    myRTC.date++;
    updateTime();
  }
  // still the loop hole is present for month increment.
}

void deleteMessages(String deleteCommand) {
  bool temp_bool;
  deleteMessages(deleteCommand, &temp_bool);
}

void deleteMessages(String deleteCommand, bool *message_deleted) {
  // let say deleteCommand = "#delete 3,4,5"
  int num = 0;
  String numHolder = "";
  String targetCommand =
      deleteCommand.substring(deleteCommand.indexOf("delete") + 7, -1);
  // targetCommand = "3,4,5"
  // check if the there is a number or empty space or not other wise print error
  // message
  bool condition_ = true;
  for (int pointer_ = 0; condition_; pointer_++)
    if (targetCommand[pointer_] != ' ')
      if ((targetCommand[pointer_] >= '0' && targetCommand[pointer_] <= '9') ||
          targetCommand[pointer_] == '-')
        condition_ = false;
      else {
        alert("Unable to delete messages {" + targetCommand + "} #1322");
        *message_deleted = false;
        return;
      }

  for (int i = 0; i < targetCommand.length(); i++) {
    if (targetCommand[i] == ',' || targetCommand[i] == ' ') {
      if (isNum(numHolder)) {
        deleteMessage(numHolder.toInt())
            ? println("$Deleting message number: " + numHolder)
            : alert("#1388 Message not deleted index: " + numHolder);
        numHolder = "";
      } else {
        println("Error in delete command");
        println("numHolder: " + numHolder);
      }
    } else {
      numHolder += targetCommand[i];
    }
  }
  if (numHolder.length() > 0) {
    deleteMessage(numHolder.toInt())
        ? println("$$Deleting message number: " + numHolder)
        : alert("#1401 Message not deleted index: " + numHolder);
    numHolder = "";
    *message_deleted = true;
  }
  *message_deleted = true;
}

String fetchDetails(String str, String begin_end, int padding) {
  // str is the string to fetch data using begin_end character or string and
  // padding remove the data around the required data if padding is 1 then it
  // will only remove the begin_end string/character
  String beginOfTarget = str.substring(str.indexOf(begin_end) + 1, -1);
  return beginOfTarget.substring(padding - 1, beginOfTarget.indexOf(begin_end) -
                                                  (padding - 1));
}

String fetchDetails(String str, String begin, String end, int padding) {
  // str is the string to fetch data using begin and end character or string
  // and padding remove the data around the required data if padding is 1 then
  // it will only remove the begin and end string/character
  String beginOfTarget = str.substring(str.indexOf(begin) + 1, -1);
  Println("beginOfTarget: " + beginOfTarget);
  return beginOfTarget.substring(padding - 1,
                                 beginOfTarget.indexOf(end) - (padding - 1));
}

// void initBLE() {
//   BLEDevice::init("TTGO T-Call BLE");
//   pServer = BLEDevice::createServer();
//   pServer->setCallbacks(new MyServerCallbacks());

//   BLEService *pService = pServer->createService(
//       BLEUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b")); // Custom service
//       UUID
//   pCharacteristic = pService->createCharacteristic(
//       BLEUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8"), // Custom
//                                                        // characteristic UUID
//       BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
//   pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

//   pService->start();

//   BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
//   pAdvertising->addServiceUUID(pService->getUUID());
//   pAdvertising->setScanResponse(true);
//   pAdvertising->setMinPreferred(0x06); // sets the minimum advertising
//   interval BLEDevice::startAdvertising();

//   Serial.println("Waiting for Bluetooth connection...");
// }

bool isNum(String num) {
  if (num.toInt() > -9999 && num.toInt() < 9999)
    return true;
  else
    return false;
}

String getCompleteString(String str, String target) {
  //" #setting <ultra sound alerts 0> <display 1>" , "ultra"
  // return "ultra sound alerts 0"
  String tempStr = "";
  int i = str.indexOf(target);
  if (i == -1) {
    Println("String is empty" + str);
    return tempStr;
  } else if (str.length() <= 20) { // to avoid spiffs on terminal
    Println(7, "@--> Working on: " + str + " finding: " + target);
  }
  for (; i < str.length(); i++) {
    if ((str[i] >= '0' && str[i] <= '9') || (str[i] >= 'a' && str[i] <= 'z') ||
        (str[i] >= 'A' && str[i] <= 'Z') || (str[i] == ' ') ||
        (str[i] == '.') && (str[i] != '>') && (str[i] != '\n') ||
        (str[i] == ':') || (str[i] == '=') || (str[i] == '_') ||
        (str[i] == '"'))
      tempStr += str[i];
    else {
      Println(7, "Returning (complete string) 1: " + tempStr);
      return tempStr;
    }
  }
  Println(7, "Returning (complete string) 2: " + tempStr);
  return tempStr;
}

String fetchNumber(String str, char charToInclude) {
  String number = "";
  (millis() / 1000) > 10 ? Println(7, "Fetching number from: " + str)
                         : Println("");
  bool numberStart = false;
  for (int i = 0; i < str.length(); i++) {
    if (str[i] >= '0' && str[i] <= '9' ||
        (str[i] == charToInclude && numberStart)) {
      number += str[i];
      numberStart = true;
    } else if (numberStart) {
      (millis() / 1000) > 10
          ? Println(7, "Returning (fetchNumber) 1: " + number)
          : Println("");
      return number;
    }
  }
  (millis() / 1000) > 10 ? Println(7, "Returning (fetchNumber) 2: " + number)
                         : Println("");
  return number;
}

int fetchNumber(String str) {
  // chargeFor 30
  //   println("fetchNumber function called with: " + str);
  String number = "";
  bool numberStart = false;
  for (int i = 0; i < str.length(); i++) {
    if (str[i] >= '0' && str[i] <= '9') {
      number += str[i];
      numberStart = true;
    } else if (numberStart) {
      //   println("Returning (fetchNumber) 1: " + number);
      return number.toInt();
    }
  }
  //   println("Returning (fetchNumber) 2: " + number);
  return number.toInt();
}

void inputManager(String command, int inputFrom) {
  // when you ant to ad new command just try to add as above as you can and make
  // sure you dont make any conflict with pre use strings.
  println("Working on: " + command);
  //  inputFrom tell who's this function user 1:BLE, 2:Serial, 3:sms or 4:
  //  orange-pi

  if (isIn(command, "demo message")) {
    //$ demo message (2)[time?]{+923354888444}
    int index_open_bracket = command.indexOf("(");
    int index_close_bracket = command.indexOf(")");

    if (index_open_bracket != -1 && index_close_bracket != -1) {
      // Extracting demo_message_index
      demo_message_index =
          command.substring(index_open_bracket + 1, index_close_bracket);

      // Find indexes of curly braces
      int index_open_curly = command.indexOf("[");
      int index_close_curly = command.indexOf("]");

      if (index_open_curly != -1 && index_close_curly != -1) {
        // Extracting demo_message_content
        demo_message_content =
            command.substring(index_open_curly + 1, index_close_curly);

        // Extracting demo_message_number
        demo_message_number = command.substring(
            index_close_curly + 2,
            command.length() - 1); // Adding 2 to exclude the curly brace and
                                   // the following character
      }
    }
    println("demo_message_index: " + demo_message_index);
    println("demo_message_number: " + demo_message_number);
    println("demo_message_content: " + demo_message_content);
    pending_test_message = true;
    getResponse();
  } else if (isIn(command, "bypass key:")) {
    setBypassKey(command);
  } else if (isIn(command, "smsTo")) {
    String strSms =
        command.substring(command.indexOf("[") + 1, command.indexOf("]"));
    String strNumber =
        command.substring(command.indexOf("{") + 1, command.indexOf("}"));
    sendSMS(strSms, strNumber);
    inputFrom == 3 ? command += "<executed>" : "";
  } else if (isIn(command, "say to orange pi", "say to orangepi")) {
    toOrangePi(command.substring(command.indexOf("say to orange") + 16, -1));
  } else if (isIn(command, "test orange pi logs")) {
    saveItOrangePi(command.substring(command.indexOf("test orange") + 19, -1));
  } else if (isIn(command, "switch") &&
             (isIn(command, "off") || isIn(command, "on"))) {
    relayControllerManager(command);
    inputFrom == 3 ? command += "<executed>" : "";
  } else if ((command.indexOf("check sms sending") != -1) ||
             (command.indexOf("sms sending?") != -1)) {
    sms_allowed = hasPackage();
    println(String("SMS sending is ") +
            (sms_allowed ? "allowed!" : "not allowed!"));
    inputFrom == 3 ? command += "<executed>" : "";
  } else if (command.indexOf("py_time:") != -1) {
    println("***Received time from terminal setting up time...");
    rtc = command.substring(command.indexOf("py_time:") + 8, -1);
    println("Fetching time from: <" + rtc + ">");
    setTime();
    rtc = "";
  } else if (command.indexOf("hay ttgo-tcall!") !=
             -1) { // message from orangePi
    println("`````````````````````````````````");
    println("Message from Orange Pi:");
    println(command);
    // it will receive command like this {hay ttgo-tcall! Fajir 5:42 AM}
    // and i want to upload Fajir 5:42 AM in last_line variable
    if (command.indexOf("AM") != -1 || command.indexOf("PM") != -1)
      last_line =
          command.substring(command.indexOf("!") + 1, command.indexOf("}"));
    println("`````````````````````````````````");
    if (command.indexOf("by pass key:") != -1) {
      // {hay ttgo-tcall! by pass key: QWERTY}
      println("Bypass key received");
      byPass_key_from_orangePi =
          command.substring(command.indexOf(":") + 1, command.indexOf("}"));
      println("Bypass key: " + byPass_key_from_orangePi);
    }

  } else if (command.indexOf("value of:") != -1) {
    String varName =
        getVariableName(command.substring(command.indexOf(":")), ":");
    String targetValue = getFileVariableValue(varName);
    println("Value of: " + varName + " is: " + targetValue);
    if (command.indexOf("to") != -1) {
      String newValue = command.substring(command.indexOf("to") + 2, -1);
      println("Updating value to: " + newValue);
      updateSPIFFS(varName, newValue);
    }
    syncSPIFFS();
    Println(7, "\t\t ###leaving else part #### \n");
  } else if (command.indexOf("time?") != -1) {
    println(getRTC_Time());
    inputFrom == 3 ? command += "<executed>" : "";
  } else if (command.indexOf("debug:") != -1) {
    // this part will enable/disable debugging, and print debugging status
    int index = fetchNumber(getCompleteString(command, "debug:"));
    if (index < DEBUGGING_OPTIONS && index > -1) {
      allowed_debugging[index] ? allowed_debugging[index] = 0
                               : allowed_debugging[index] = 1;
      updateDebugger(index, allowed_debugging[index]);
    } else {
      print("Error in debug command!\t");
      println("Unable to make changes at index: " + String(index));
    }
    println("\nUpdated debugging status: ");
    Serial.println(
        "Debugging: " + String(allowed_debugging[0] ? "0: Wifi" : " ") +
        String(allowed_debugging[1] ? ", 1->LCD" : " ") +
        String(allowed_debugging[2] ? ", 2->SIM800L" : " ") +
        String(allowed_debugging[3] ? ", 3->ThingSpeak" : " ") +
        String(allowed_debugging[4] ? ", 4->Whatsapp" : " ") +
        String(allowed_debugging[5] ? ", 5->BLE" : " ") +
        String(allowed_debugging[6] ? ", 6->SPIFFS" : " ")) +
        String(allowed_debugging[7] ? ", 7->Orange pi" : " ");
    inputFrom == 3 ? command += "<executed>" : "";
  } else if ((command.indexOf("debug") != -1) &&
             (command.indexOf("option") != -1)) {
    println("Here the the debugging index:\n\t-> Wifi: 0, LCD: 1, SIM800L "
            ": 2, ThingSpeak: 3, Whatsapp: 4, BLE: 5, SPIFFS: 6, Orange pi "
            ": 7");
    inputFrom == 3 ? command += "<executed>" : "";
  } else if (command.indexOf("debug?") != -1) {
    println("Here is the debugging status: ");
    Serial.println(
        "Debugging: " + String(allowed_debugging[0] ? "0: Wifi" : " ") +
        String(allowed_debugging[1] ? ", 1->LCD " : " ") +
        String(allowed_debugging[2] ? ", 2->SIM800L " : " ") +
        String(allowed_debugging[3] ? ", 3->ThingSpeak " : " ") +
        String(allowed_debugging[4] ? ", 4->Whatsapp " : " ") +
        String(allowed_debugging[5] ? ", 5->BLE " : " ") +
        String(allowed_debugging[6] ? ", 6->SPIFFS " : " ") +
        String(allowed_debugging[7] ? ", 7->Orange pi " : " "));
  } else if (command.indexOf("#setting") != -1) {
    // updateVariablesValues(command);
    // deleteMessage(1);
    // Delay(500);
    // sendSMS(command, "+923374888420");
    // command += " <executed>"; TODO remove this block
  } else if (command.indexOf("callTo") != -1) {
    call(command.substring(command.indexOf("{") + 1, command.indexOf("}")));
    inputFrom == 3 ? command += "<executed>" : "";
  } else if (command.indexOf("_call") != -1) {
    println("Calling " + String(MY_NUMBER));
    giveMissedCall();
    inputFrom == 3 ? command += "<executed>" : "";
  } else if (command.indexOf("sms") != -1) {
    // fetch sms from input string, sample-> sms: msg here
    String sms = command.substring(command.indexOf("sms") + 4);
    println("Sending SMS: " + sms + " to: " + String(MY_NUMBER));
    sendSMS(sms);
    inputFrom == 3 ? command += "<executed>" : "";
  } else if (command.indexOf("all") != -1) {
    println("Reading all messages");
    moduleManager();
    inputFrom == 3 ? command += "<executed>" : "";
  } else if (command.indexOf("battery") != -1) {
    println(updateBatteryStatus());
    if (inputFrom == 3) {
      updateBatteryParameters(updateBatteryStatus());
      sendSMS("Battery percentage: " + String(battery_percentage) +
              "\nBattery voltage: " + String(battery_voltage));
      command += " <executed>";
    }
  } else if (command.indexOf("lastBefore") != -1) {
    println("Index before <" + command.substring(11, -1) + "> is: " +
            String(getMessageNumberBefore(command.substring(11, -1).toInt())));
  } else if (command.indexOf("forward") != -1) {
    println("Forwarding message of index: " +
            fetchNumber(getCompleteString(command, "forward")));
    forwardMessage(fetchNumber(getCompleteString(command, "forward")));
    inputFrom == 3 ? command += "<executed>" : "";
  } else if (command.indexOf("delete") != -1) { // to delete message
    bool messages_deleted;
    deleteMessages(command, &messages_deleted);
    messages_deleted ? inputFrom == 3 ? command += "<executed>" : "" : "";
  } else if (command.indexOf("terminator") != -1) {
    terminateLastMessage();
    inputFrom == 3 ? command += "<executed>" : "";
  } else if (command.indexOf("hangUp") != -1) {
    say("AT+CHUP");
    inputFrom == 3 ? command += "<executed>" : "";
  } else if (command.indexOf("debug") != -1) {
    DEBUGGING ? DEBUGGING = false : DEBUGGING = true;
    Delay(50);
    inputFrom == 3 ? command += "<executed>" : "";
    println(String("Debugging: ") + (DEBUGGING ? "Enabled" : "Disabled"));
    String DValue = (DEBUGGING ? "1" : "0");
    updateSPIFFS("DEBUGGING", DValue);
  } else if (command.indexOf("status") != -1) {
    println(getVariablesValues());
  } else if (command.indexOf("update") != -1) {
    updateVariablesValues(readMessage(
        (command.substring(command.indexOf("update") + 7, -1)).toInt()));
    inputFrom == 3 ? command += "<executed>" : "";
  } else if (command.indexOf("reboot") != -1) {
    println("Rebooting...");
    modem.restart();
    inputFrom == 3 ? command += "<executed>" : "";
  } else if (command.indexOf("updateTime") != -1) {
    println("Updating time");
    // sendSMS("#setTime", "+923374888420");
    updateTime() ? alert("Time updated successfully #1550")
                 : sendSMS("#setTime", "+923374888420");
    inputFrom == 3 ? command += "<executed>" : "";
  } else if (command.indexOf("whatsapp") != -1) {
    if (command.length() <= 10) {
      Println(5, "Sending test message from esp32");
      sendWhatsappMsg("test_message_from_esp32");
    } else {
      Println(5, "-> [" + String(command.length()) + "]Sending message: " +
                     command.substring(command.indexOf("whatsapp") + 9, -1));
      sendWhatsappMsg(command.substring(command.indexOf("whatsapp") + 9, -1));
    }
    inputFrom == 3 ? command += "<executed>" : "";
  } else if (command.indexOf("readSPIFFS") != -1) {
    println("Data in SPIFFS: " + readSPIFFS());
  } else if (command.indexOf("setTime") != -1) {
    String tempStr = "+CMGL: 1,\"REC "
                     "READ\",\"+923374888420\",\"\",\"23/08/14,17:21:05+20\"";
    setTime(tempStr);
  } else if (command.indexOf("read") != -1) {
    int targetMsg = fetchNumber(command);
    if (messageExists(targetMsg))
      println("Message: " + readMessage(targetMsg));
    else
      println("Message not Exists");
  } else if (command.indexOf("my_ip") != -1 || command.indexOf("my ip") != -1) {
    askOrangPi("send ip");
    // TODO: after upgrading to API work this part was affected
    if (inputFrom == 3) {
      command += "<executed>";
      // sendSMS(response);
    }
  } else if (command.indexOf("enable") != -1) {
    if (command.indexOf("thingspeak") != -1 ||
        command.indexOf("thingSpeak") != -1) {
      set(&thingspeak_enabled, "thingspeak_enabled", true);
      initThingSpeak();
    } else if (command.indexOf("thingsboard") != -1 ||
               command.indexOf("thingsBoard") != -1) {
      set(&thingsboard_enabled, "thingsboard_enabled", true);
      initThingsBoard();
    } else if (isIn(command, "message_saving_mode") ||
               isIn(command, "message saving mode")) {
      set(&message_saving_mode, "message_saving_mode", true);
    } else {
      println("You can only enable these services: "
              "\n\t->thingspeak\n\t->thingsboard\n\t->message_saving_mode");
    }
  } else if (command.indexOf("disable") != -1) {
    if (command.indexOf("thingspeak") != -1 ||
        command.indexOf("thingSpeak") != -1) {
      set(&thingspeak_enabled, "thingspeak_enabled", false);
    } else if (command.indexOf("thingsboard") != -1 ||
               command.indexOf("thingsBoard") != -1) {
      set(&thingsboard_enabled, "thingsboard_enabled", false);
    } else if (isIn(command, "message_saving_mode") ||
               isIn(command, "message saving mode")) {
      set(&message_saving_mode, "message_saving_mode", false);
    } else {
      println("You can only disable these services: "
              "\n\t->thingspeak\n\t->thingsboard\n\t->message_saving_mode");
    }
  }
  // TODO: help command should return all executable commands
  // TODO: Add documentation for these commands
  else {
    println("Executing: " + command);
    if (inputFrom == 3) {
      command += "<not executed>";
    } else
      say(command);
  }
  ret_string = command;
}

void set(bool *var, String varName, bool varValue) {
  *var = varValue;
  updateSPIFFS(varName, varValue ? "1" : "0");
}

bool companyMsg(String mobileNumber) {
  int loopCount = getTotalWordsCountInString(company_numbers);
  for (int i = 0; i < loopCount; i++)
    if (isIn(mobileNumber, getWordInString(company_numbers, i)))
      return true;

  return false;
}

void sendWhatsappMsg(String message) {
  if (myRTC.date == 0) {
    println(
        "myRTC not updated yet so wait for it to avoid unexpected behaviour");
    updateTime() ? sendWhatsappMsg(message)
                 : println("Unable send message, time not updated");
  } else if (!wifiConnected()) {
    println("Wifi not connected unable to send whatsapp message");
    return;
  }
  Println(5, "########_______________________________#########");
  HTTPClient http;
  String serverPath = getServerPath(getHTTPString(message));
  Println(5, "Working on this HTTP: \n->{" + serverPath + "}");
  Delay(3000);
  http.begin(serverPath);
  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    print("HTTP Response code: ");
    println(String(httpResponseCode));
    String payload = http.getString();
    println(payload);
    updateWhatsappMessageCounter();
  } else {
    print("Error code: ");
    println(String(httpResponseCode));
  }
  http.end();
  Println(5, "########_______________________________#########");
}

String getServerPath(String message) {
  String whatsappNumber;
  String api;
  if (whatsapp_message_number == -1) {
    whatsapp_message_number = getMessagesCounter();
    if (whatsapp_message_number < 0 && whatsapp_message_number > 100) {
      println("Unexpected behaviour in getMessagesCounter()");
      addError("1608");
    }
    whatsappNumber = whatsapp_numbers[whatsapp_message_number / 25];
    api = API[whatsapp_message_number / 25];

  } else {
    whatsappNumber = whatsapp_numbers[whatsapp_message_number / 25];
    api = API[whatsapp_message_number / 25];
    Println(5, "Using " + whatsappNumber +
                   "for whatsapp message: " + String(whatsapp_message_number));
  }
  String serverPath = server + whatsappNumber + getHTTPString(message) + api;
  Println(5, "returning mobile number: " + String(whatsappNumber));
  Println(5, "returning api: " + String(api));
  return serverPath;
}

int getMessagesCounter() {
  if (!thingspeak_enabled) {
    return -1;
  }
  int todayMessages = -1;
  int lastUpdateOfThingSpeakMessageCounter =
      getThingSpeakFieldData(TS_MSG_SEND_DATE_FIELD);
  if (lastUpdateOfThingSpeakMessageCounter != myRTC.date) {
    Println(5, "Day changed initiating new counter");
    setThingSpeakFieldData(TS_MSG_SEND_DATE_FIELD, myRTC.date);
    setThingSpeakFieldData(TS_MSG_COUNTER_FIELD, 1);
    writeThingSpeakData();
    todayMessages = 1;
  } else {
    todayMessages = getThingSpeakFieldData(TS_MSG_COUNTER_FIELD);
    Println(5, "Else: Fetched data: " + String(todayMessages));
  }
  Println(5, "Returning Counter: " + String(todayMessages));
  return todayMessages;
}

int getThingSpeakFieldData(int fieldNumber) {
  if (thingspeak_enabled) {
    int data = ThingSpeak.readIntField(ts_channel_id, fieldNumber);
    int statusCode = ThingSpeak.getLastReadStatus();
    if (statusCode == 200) {
      Println(5, "Data fetched from field: " + String(fieldNumber));
    } else {
      Println(5,
              "Problem reading channel. HTTP error code " + String(statusCode));
      data = -1;
    }
    return data;
  }
}

void updateWhatsappMessageCounter() {
  setThingSpeakFieldData(TS_MSG_SEND_DATE_FIELD, myRTC.date);
  setThingSpeakFieldData(TS_MSG_COUNTER_FIELD, ++whatsapp_message_number);
  writeThingSpeakData();
}

void setThingSpeakFieldData(int field, int data) {
  if (thingspeak_enabled)
    ThingSpeak.setField(field, data);
}

void writeThingSpeakData() {
  if (thingspeak_enabled) {
    int updateStatus = ThingSpeak.writeFields(ts_channel_id, ts_api_key);
    if (updateStatus == 200) {
      Println(4, "ThingSpeak update successfully...");
    } else {
      Println(4, "Error updating ThingSpeak. Status: " + String(updateStatus));
    }
  }
}

unsigned int getMint() { return ((millis() / 1000) / 60); }

String getHTTPString(String message) {
  Println(5, "\n@ -> received message string:{" + message + "}");
  String tempStr = "";
  for (int i = 0; i < message.length(); i++) {
    if (message[i] == ' ' || message[i] == '\n' || message[i] == '\r')
      tempStr += "_";
    else if ((message[i] >= '0' && message[i] <= '9') ||
             (message[i] >= 'A' && message[i] <= 'Z') ||
             (message[i] >= 'a' && message[i] <= 'z') || (message[i]) == '_' ||
             (message[i]) == '@' || (message[i]) == '%' ||
             (message[i]) == '*' || (message[i]) == '-' || (message[i]) == '#')
      tempStr += message[i];
    else
      println("Invalid character {" + String(message[i]) +
              "} in whatsapp message ignoring it");
  }
  Println(5, "@ -> returning message string:{" + tempStr + "}");
  return tempStr;
  // TODO:remove all other characters which can cause any issue in http request
}

bool newPackageSubscribed(String str) {
  if (str.indexOf("10000 SMS with 30day validity") != -1)
    return true;
  return false;
}

String readSPIFFS() {
  String tempStr = "";
  if (!SPIFFS.begin(true)) {
    println("An Error has occurred while mounting SPIFFS");
    Delay(100);
    return "";
  }

  File file = SPIFFS.open(CONFIG_FILE);
  if (!file) {
    println("Failed to open file for reading");
    Delay(100);
    return "";
  }
  Println(7, "Reading file ...");
  Println(7, "File Content:");
  while (file.available()) {
    char character = file.read();
    tempStr += character; // Append the character to the string
  }
  Println(7, tempStr);
  file.close();

  return tempStr;
}

String getFirstLine(String str) {
  String newStr = "";
  for (int i = 0; i < str.length(); i++) {
    if (str[i] != '\n' || str[i] != '\r')
      newStr += str[i];
    else
      break;
  }
  return newStr;
}

String getVariableName(String str, String startFrom) {
  String newStr = "";
  str = getFirstLine(str);
  bool found = false;
  for (int i = str.indexOf(startFrom) + startFrom.length(); i < str.length();
       i++) {
    if ((str[i] >= 'a' && str[i] <= 'z') || (str[i] >= 'A' && str[i] <= 'Z') ||
        (found && str[i] >= '0' && str[i] <= '9') || (str[i] == '_')) {
      newStr += str[i];
      found = true;
    } else if (found) {
      break;
    }
  }
  return newStr;
}

String getFileVariableValue(String varName) {
  return getFileVariableValue(varName, false);
}

String getFileVariableValue(String varName, bool createNew) {
  String targetLine = getCompleteString(readSPIFFS(), varName);
  Println(7, "Trying to fetch data from line: " + targetLine);
  if (targetLine.indexOf(varName) == -1 && !createNew) {
    Println(7, "Variable not found in file returning -1");
    return "-1";
  } else if (targetLine.indexOf(varName) == -1 && createNew) {
    Println(7, "Variable not found in file creating new & returning 0");
    updateSPIFFS(varName, "0");
    return "0";
  }
  String targetValue = targetLine.substring(varName.length(), -1);
  Println(7, "Trying to fetch data from: " + targetValue);
  int firstQuote = targetLine.indexOf('"');
  int secondQuote = targetLine.indexOf('"', firstQuote + 1);

  if (firstQuote != -1 && secondQuote != -1) {
    String quote_string =
        "\"" + targetLine.substring(firstQuote + 1, secondQuote) + "\"";
    Println(7, "returning quote_string: {" + quote_string + "}");
    return quote_string;
  } else {
    String variableValue = fetchNumber(targetValue, '.');
    Println(7, "Returning int value of {" + varName + "}: " + variableValue);
    return variableValue;
  }
}

void updateSPIFFS(String variableName, String newValue) {
  if (!SPIFFS.begin()) {
    println("Failed to mount file system");
    return;
  }

  String previousValue = getFileVariableValue(variableName);
  Println(7, "@ Replacing previous value: ->" + previousValue +
                 "<- with new value: ->" + newValue + "<-");
  // Open the file for reading
  File file = SPIFFS.open(CONFIG_FILE, "r");
  if (!file) {
    println("Failed to open file for reading");
    return;
  }

  // Create a temporary buffer to store the modified content
  String updatedContent = "";
  bool valueReplaced = false;
  // Read and modify the content line by line
  while (file.available()) {
    String line = file.readStringUntil('\n');
    if (line.indexOf(variableName) != -1) {
      line = variableName + ": " + newValue;
      valueReplaced = true;
    }
    updatedContent += line + "\n";
  }

  // Close the file
  file.close();

  Println(7, "Updated content: " + updatedContent);

  // Reopen the file for writing, which will erase the previous content
  file = SPIFFS.open(CONFIG_FILE, "w");
  if (!file) {
    println("Failed to open file for writing");
    return;
  }

  if (!valueReplaced) {
    println("Variable not found in file, creating new variable");
    updatedContent += ("\n" + variableName + ": " + newValue);
  }

  // Write the modified content back to the file
  file.print(updatedContent);
  file.close();
  Println(7, "Variable {" + variableName + "} has been updated.");
}

bool hasPackage() {
  int expiryDate = 0, expiryMonth = 0, expiryYear = 0;
  if (myRTC.date == 0)
    updateTime()
        ? "time updated in package inspector"
        : "unable to update package inspector as time in not updated as "
          "expected";
  if (myRTC.date != 0) { // if time is initialized
    if (package_expiry_date == 0)
      package_expiry_date = getFileVariableValue("packageExpiryDate").toInt();
    println("Package expiry date fetched from file: " +
            String(package_expiry_date));
    if (package_expiry_date <= 0) {
      rise("packageExpiryDate is not even initialized in SPIFFS", "2065");
      return false;
    }
  } else {
    rise("Unable to update time for hasPackage function, value of myRTC "
         "variables: {" +
             getRTC_Time() +
             "} package_expiry_date: " + String(package_expiry_date),
         "2080");
    println("Set Time First!");
    return false;
  }
  setField_MonthAndDate(&package_expiry_date, &expiryMonth, &expiryDate,
                        &expiryYear);
  if ((myRTC.year <= expiryYear) &&
      ((myRTC.month <= expiryMonth) ||
       (myRTC.month == expiryMonth && myRTC.date <= expiryDate)) &&
      myRTC.month != 0) {
    println("\n\t $$$ Package is valid myRTC month: " + String(myRTC.month) +
            " expiryMonth: " + String(expiryMonth) + ", myRTC date: " +
            String(myRTC.date) + " expiryDate: " + String(expiryDate) +
            ", myRTC year: " + String(myRTC.year) +
            " expiryYear: " + String(expiryYear) + "\n\n");
    return true;
  } else {
    println("\n\t !!! Package is not valid myRTC month: " +
            String(myRTC.month) + " expiryMonth: " + String(expiryMonth) +
            ", myRTC date: " + String(myRTC.date) + " expiryDate: " +
            String(expiryDate) + ", myRTC year: " + String(myRTC.year) +
            " expiryYear: " + String(expiryYear) + "\n\n");
    return false;
  }
}

void setField_MonthAndDate(int *field, int *month, int *date, int *year) {
  if (*field > 0 && *month == 0 && *date == 0) {
    *month = (*field / 100) % 100;
    *date = *field % 100;
    *year = *field / 10000;
    Println(7, "From field " + String(*field) + " => Month: " + String(*month) +
                   " Date: " + String(*date) + " Year: " + String(*year));
  } else {
    rise("Error in setField_MonthAndDate function! here is the "
         "data function received: field: " +
             String(*field) + " month: " + String(*month) +
             " date: " + String(*date) + " year: " + String(*year),
         "2108");
  }
}

void syncSPIFFS() {
  updateDebugger();
  DEBUGGING =
      getFileVariableValue("DEBUGGING", true).toInt() == 1 ? true : false;
  thingspeak_enabled =
      getFileVariableValue("thingspeak_enabled", true).toInt() == 1 ? true
                                                                    : false;
  thingsboard_enabled =
      getFileVariableValue("thingsboard_enabled", true).toInt() == 1 ? true
                                                                     : false;
  message_saving_mode =
      getFileVariableValue("message_saving_mode", true).toInt() == 1 ? true
                                                                     : false;
  company_numbers = remove_quotes(getFileVariableValue("COMPANY_NUMBERS"));
}

void updateDebugger() {
  allowed_debugging[0] = getFileVariableValue("WIFI_debug", true).toInt();
  allowed_debugging[1] = getFileVariableValue("LCD_debug", true).toInt();
  allowed_debugging[2] = getFileVariableValue("SIM800L_debug", true).toInt();
  allowed_debugging[3] = getFileVariableValue("THINGSPEAK_debug", true).toInt();
  allowed_debugging[4] = getFileVariableValue("WHATSAPP_debug", true).toInt();
  allowed_debugging[5] = getFileVariableValue("BLE_debug", true).toInt();
  allowed_debugging[6] = getFileVariableValue("SPIFFS_debug", true).toInt();
  allowed_debugging[7] = getFileVariableValue("OrangePi_debug", true).toInt();
}

void updateDebugger(int index, int value) {
  String varName = (index == 0   ? "WIFI_debug"
                    : index == 1 ? "LCD_debug"
                    : index == 2 ? "SIM800L_debug"
                    : index == 3 ? "THINGSPEAK_debug"
                    : index == 4 ? "WHATSAPP_debug"
                    : index == 5 ? "BLE_debug"
                    : index == 6 ? "SPIFFS_debug"
                    : index == 7 ? "OrangePi_debug"
                                 : "error");
  if (varName == "error") {
    println("Error in updateDebugger function");
    return;
  } else {
    Println("Updating value of: " + varName + " to: " + String(value));
    updateSPIFFS(varName, String(value));
  }
}

void updateMQTT(int temperature_, int humidity_) {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  client.publish("v1/devices/me/telemetry",
                 ("{temperature:" + String(temperature_) +
                  ",humidity:" + String(humidity_) + "}")
                     .c_str());
  Println(8, "MQTT updated");
}

void toOrangePi(String str) {
  // println("{hay orange-pi! " + removeNewline(str) + "}");
  myServer.sendDataToPythonServer("{hay orange-pi! " + removeNewline(str) +
                                  "}");
}

void saveItOrangePi(String str) {
  String logs_for_pi =
      "{hay orange-pi! [#SaveIt]:\n"
      "---------------------------------------------------------->>\n "
      "TimeStamp: " +
      String(millis() / 1000) + "\nMessage:\n" + str +
      "\n<<----------------------------------------------------------\n "
      "end_of_file }";
  myServer.sendDataToPythonServer(logs_for_pi);
}

void askOrangPi(String str) {
  toOrangePi(str + "?");
  wait(500);
}

String replyOfOrangePi() {
  unsigned int startTime = millis() / 1000;
  String reply = "";
  while (
      (!(reply.indexOf("hay ttgo-tcall!") != -1 && reply.indexOf("}") != -1)) &&
      (millis() / 1000) - startTime < ORANGEPI_RESPONSE_WAIT_TIME) {
    // if (SerialMon.available()) {
    if (pi_incoming.length() > 0) {
      reply += pi_incoming;
    }
  }
  println("Reply of Orange Pi: {" + reply + "}");
  return reply;
}

bool updateTime() {
  askTime();
  String piResponse = replyOfOrangePi();
  if (piResponse.length() > 0) {
    if (piResponse.indexOf("py_time:") != -1) {
      println("***Received time from terminal setting up time...@");
      rtc = piResponse.substring(piResponse.indexOf("py_time:") + 8, -1);
      println("@2 Fetching time from: <" + rtc + ">");
      setTime();
    } else {
      Println(8, "Unable to execute pi response: [" + piResponse + "]");
      return false;
    }
    Println(8, "Time updated successfully");
    return true;
  } else {
    println("Failed to update time from orange pi trying to update online");
    if (wifi_working) {
      int i = 100;
      while (!timeClient.update() && i > 0) {
        timeClient.forceUpdate();
        i -= 10;
        delay(100);
      }
      // formattedDate = timeClient.getFormattedDate();
      // updateOnlineTime(formattedDate);
      // TODO: this part is not working on new machine
      println("Online updated time: " + getRTC_Time());
      if (myRTC.month == 0)
        return false;
      else
        return true;
    }
    return false;
  }
}

void askTime() {
  // this will ask for updated time from orange pi
  Println(8, "Asking time from Orange Pi");
  toOrangePi("send time");
}

void alert(String msg) {
  println("\n@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
  println("Alert: " + msg);
  println("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
}

String removeNewline(String str) {
  str.replace("\n", " ");
  return str;
}

void initThingsBoard() {
  println("\nThingsBoard initializing...\n");
  client.setServer(mqtt_server, 1883);
}

void initThingSpeak() {
  println("\nThinkSpeak initializing...\n");
  ThingSpeak.begin(espClient); // Initialize ThingSpeak
  delay(500);
  ThingSpeak.setField(4, random(1, 50)); // set any random value.
}

void updatePackageSubscribedDate() {
  if (myRTC.month == 0) { // should not execute
    alert("#2268 Module time is not updated so trying to update it");
    updateTime();
    updatePackageSubscribedDate(3);
  } else {
    updatePackageSubscribedDate(3);
  }
}

void updatePackageSubscribedDate(int retries) {
  if (myRTC.month == 0 && retries > 0) {
    updatePackageSubscribedDate(retries - 1);
    println("something bad happens retry count: " + retries);
  } else if (myRTC.month != 0) {
    print("Using module's time, Month: " + String(myRTC.month) +
          ", Day: " + String(myRTC.date) + "to -> ");
    int updatedDate = getNePackageDate();

    // 240207 -> till 7th feb 2024
    log("New string for new package is: " + String(updatedDate));
    println("!!!!!!!!!!***!!!!!!!!!!\n");
    updateSPIFFS("packageExpiryDate", String(updatedDate));
    sms_allowed = hasPackage();
  } else {
    rise("Package update error" + getRTC_Time() + " reties: " + String(retries),
         "2288");
  }
}

int getNePackageDate() {
  int updated_date = (myRTC.year * 10000) +
                     (myRTC.month + 1 < 13 ? (myRTC.month + 1) * 100 : 100);
  int coming_month_days = getMonthDaysCount(
      myRTC.month + 1, myRTC.month + 1 < 13 ? myRTC.year : myRTC.year + 1);
  updated_date +=
      (coming_month_days < myRTC.date ? coming_month_days : myRTC.date);
  log("Updating package_expiry_date to: " + String(updated_date));
  return updated_date;
}

int getMonthDaysCount(int month, int year) {
  if (month == 2) {
    if (year % 4 == 0)
      return 29;
    else
      return 28;
  } else if (month == 4 || month == 6 || month == 9 || month == 11)
    return 30;
  else
    return 31;
}

bool senderIsAuthentic(String number, String message) {
  if (companyMsg(number))
    return false;
  if (isIn(String(AUTHENTIC_NUMBERS), number))
    return true;
  else if (number.length() > 10) {
    if (isIn(message, BYPASS_KEY))
      return true;

    byPass_key_from_orangePi.length() <= 0
        ? print("module ask bypass key from orange-pi")
        : print("module already have initialized with bypass key");
    askOrangPi("send bypass key");
    return false;

    return isIn(message, byPass_key_from_orangePi);
    if (byPass_key_from_orangePi.length() > 0) {
      sendSMS("Unauthorized number: {" + number +
              "} tried to access your device if he is "
              "authorized then ask him to use this key in his message: " +
              byPass_key_from_orangePi);
    } else {
      rise("unexpected error occur, unable to deal with authentic message",
           "2240");
      sendSMS("Error #2240 unable to manage bypass key");
    }
  }
  return false;
}

void setBypassKey(String tempStr) {
  byPass_key_from_orangePi =
      tempStr.substring(tempStr.indexOf(":") + 1, tempStr.indexOf("}"));
  println("Bypass key: " + byPass_key_from_orangePi);
}

void relayControllerManager(String switchNumber, String time) {
  String newRequest = relayControllerURL + switchNumber + "=" + time;
  println("Final URL: [" + newRequest + "]");
  HTTPClient localRequest;
  localRequest.begin(newRequest);

  int httpResponseCode = localRequest.GET();
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String payload = localRequest.getString();
    Serial.println("Response payload: " + payload);
    sendSMS("Request sent for switch: " + switchNumber + " time: " + time +
            " minutes");
  } else {
    Serial.print("Error in HTTP GET request. HTTP Response code: ");
    Serial.println(httpResponseCode);
    sendSMS("Request failed for switch: " + switchNumber + " time: " + time +
            " minutes");
  }

  localRequest.end();
}

void relayControllerManager(String input) {
  // input should be like this:
  // on switch 1 for 10 (10 is time in minutes)
  // or
  // on/off switch 1
  String switchNumber =
      String(fetchNumber(input.substring(input.indexOf("switch") + 7, -1)));
  String time = "";
  if (input.indexOf("for") != -1) {
    time = String(fetchNumber(input.substring(input.indexOf("for") + 4, -1)));
  }
  if (input.indexOf("on") != -1) {
    if (time.length() > 0) {
      relayControllerManager(switchNumber, time);
    } else { // common case
      relayControllerManager(switchNumber, "998");
    }
  } else if (input.indexOf("off") != -1) {
    relayControllerManager(switchNumber, "-1");
  }
}

void addError(String errorCode) {
  if (!isIn(error_codes, errorCode)) {
    error_codes += " " + errorCode;
    saveItOrangePi("New error added error code {" + errorCode + "}");
    println("new error code added: " + errorCode);
  } else {
    saveItOrangePi("Error occurred again error code {" + errorCode + "}");
    println("Error code already added: " + errorCode);
  }
}

bool isIn(String mainString, String toFind) {
  return mainString.indexOf(toFind) != -1;
}

bool isIn(String mainString, String toFind_1, String toFind_2) {
  return (isIn(mainString, toFind_1) || isIn(mainString, toFind_2));
}

bool isIn(String mainString, String toFind_1, String toFind_2,
          String toFind_3) {
  return (isIn(mainString, toFind_1) || isIn(mainString, toFind_2) ||
          isIn(mainString, toFind_3));
}

void log(String tempMessage) {
  alert(tempMessage);
  saveItOrangePi(tempMessage);
}

void rise(String tempMessage, String errorCode) {
  log(tempMessage);
  addError(errorCode);
}

String getRTC_Time() {
  return (String(myRTC.hour) + ":" + String(myRTC.minutes) + ":" +
          String(myRTC.seconds) + " ___ " + String(myRTC.month) + "/" +
          String(myRTC.date) + "/" + String(myRTC.year));
}

void updateOnlineTime(String formattedDate) {
  myRTC.year = formattedDate.substring(0, 4).toInt() % 100;
  myRTC.month = formattedDate.substring(5, 7).toInt();
  myRTC.date = formattedDate.substring(8, 10).toInt();
  myRTC.hour = formattedDate.substring(11, 13).toInt();
  myRTC.minutes = formattedDate.substring(14, 16).toInt();
  myRTC.seconds = formattedDate.substring(17, 19).toInt();
}

String remove_quotes(String input) {
  // Find the positions of the double quotes
  int firstQuote = input.indexOf('"');
  int secondQuote = input.indexOf('"', firstQuote + 1);

  // Check if both quotes are found
  if (firstQuote != -1 && secondQuote != -1) {
    // Extract the data between the quotes
    return input.substring(firstQuote + 1, secondQuote);
  } else {
    // Return the original string if quotes are not found
    return input;
  }
}

bool isValidString(String tempStr) {
  int validWords = 0, inValidWords = 0;
  int totalWordsInString = getTotalWordsCountInString(tempStr);
  for (int i = 0; i < totalWordsInString; i++)
    isValidWord(getWordInString(tempStr, i)) ? validWords += 1
                                             : inValidWords += 1;
  return validWords > inValidWords;
}

String getWordInString(String str, int index) {
  /* if user send this string:
  ("this is test message from phone",3)
  then this function ill return this string: "message" */
  int found = 0;
  int startIndex = 0;
  int endIndex = -1;

  for (int i = 0; i <= str.length(); i++) {
    if (str[i] == ' ' || i == str.length()) {
      found++;
      if (found == index + 1) {
        endIndex = i - 1;
        break;
      }
      startIndex = i + 1;
    }
  }

  if (endIndex == -1) {
    return ""; // or handle out of bounds appropriately
  } else {
    return str.substring(startIndex, endIndex + 1);
  }
}

int getTotalWordsCountInString(String str) {
  // Return 3 for string "this is test"
  int wordCount = 0;
  bool inWord = false;

  for (int i = 0; i < str.length(); i++) {
    if (str[i] != ' ' && !inWord) {
      inWord = true;
      wordCount++;
    } else if (str[i] == ' ') {
      inWord = false;
    }
  }

  return wordCount;
}

bool isValidWord(String tempStr) {
  // if the word is not the HEX
  if (tempStr.length() > 10)
    return false;
  bool ret = false;
  for (int i = 0; i <= tempStr.length(); i++)
    if (!((tempStr[i] >= 'A' && tempStr[i] <= 'F') ||
          (tempStr[i] >= '0' && tempStr[i] <= '9')))
      ret = true;
    else
      return false;
  return ret;
}

bool isInteger(String tempStr) {
  if (tempStr.length() > 10)
    return false;
  bool ret = false;
  for (int i = 0; i <= tempStr.length(); i++)
    if ((tempStr[i] >= '0' && tempStr[i] <= '9'))
      ret = true;
    else
      return false;
  return ret;
}

void handleAPIS() {
  myServer.handleClient();

  // Receive data from the Python server
  String receivedData = myServer.getLastReceivedData();
  if (receivedData.length() > 0) {
    // Parse the received JSON data
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, receivedData);

    if (error) {
      Serial.print("Failed to parse JSON: ");
      Serial.println(error.c_str());
    } else {
      // Extract the "data" field
      String data = doc["data"];
      Serial.println("Extracted data from Python server: " + data);
      pi_incoming += data;
      // inputManager(data, 4);
    }

    // Clear the received data after processing
    myServer.clearLastReceivedData();
  }
}