//$ last work 10/Sep/23 [08:12 PM]
// # version 5.3.8
// # Release Note : Whatsapp messages implemented
// # Module will now optimize all 4 Whatsapp API's

#include "arduino_secrets.h"

const char simPIN[] = "";

String MOBILE_No = MY_Number;

#include <HTTPClient.h>

String NUMBER[4] = {WHATSAPP_NUMBER_1, WHATSAPP_NUMBER_2, WHATSAPP_NUMBER_3,
                    WHATSAPP_NUMBER_4};

String API[4] = {WHATSAPP_API_1, WHATSAPP_API_2, WHATSAPP_API_3,
                 WHATSAPP_API_4};

// https://api.callmebot.com/whatsapp.php?phone=+923354888420&text=This+is+a+test&apikey=518125

String server = "https://api.callmebot.com/whatsapp.php?phone=";

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800   // Modem is SIM800
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb

#include <TinyGsmClient.h>
#include <Wire.h>
//`===================================
#include <DHT.h>
#include <ThingSpeak.h>
#include <WiFi.h>
#include <random>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

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
#define LED 13

#define WAPDA_STATE 15 // Check if wapda is on or off
// Magenta  R1:
#define BATTERY_PAIR_SELECTOR 32 // Select which battery pair is to be charge
// Blue R2-A:
#define BATTERY_CHARGER 14 // Battery should charge or not
// Green R2-B:
#define POWER_OUTPUT_SELECTOR 0 // Router input source selector

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

#define ThingSpeakEnable true
#define MAX_MESSAGES 15
#define YEAR 23

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

// ThingSpeak parameters
const char *ssid = MY_SSID;
const char *password = MY_PASSWORD;
const unsigned long channelID = MY_CHANNEL_ID;
const char *apiKey = THINGSPEAK_API;

int messagesCounterID = 5;
int lastMessageUpdateID = 6;
int whatsappMessageNumber = -1;
unsigned int lastTsUpdate = 2; // will update when time reaches
#define UPDATE_THING_SPEAK_TH_AFTER 5
unsigned int last_update = 0; // in minutes
WiFiClient client;

String END_VALUES = "  ";
String line_1 = "Temp: 00.0 C";
String line_2 = "                ";

String receivedMessage = ""; // Global variable to store received SMS message
double batteryVoltage = 0;
int batteryPercentage = 0;
// another variable to store time in millis
int messages_in_inbox = 0;
int messageStack[MAX_MESSAGES] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int currentTargetIndex = 0;

long duration;    // variable for the duration of sound wave travel
int distance = 0; // variable for the distance measurement

float temperature;
int humidity;

bool DEBUGGING = false;
// (debuggerID == 0)     // debugging WIFI functionality
// (debuggerID == 1) // debugging LCD functionality
// (debuggerID == 2) // debugging SIM800L functionality
// (debuggerID == 3) // debugging thingSpeak functionality
// (debuggerID == 4) // debugging Whatsapp functionality
// (debuggerID == 5) // debugging blynk functionality

int allowedDebugging[7] = {0, 0, 0, 1,
                           1, 0, 0}; // 0 means not allowed 1 means allowed

int debugFor = 130; // mentioned in seconds
bool ultraSoundWorking = false;
bool wifiWorking = true;
bool displayWorking = true;
bool smsAllowed = true;
bool timeVersion = false;

String messageTemplate = "#setting <ultra sound alerts 0> <display 1> <wifi "
                         "connectivity 1> <sms 1> <termination time 300> "
                         "<battery charge time 1800>";

int terminationTime = 60 * 5; // 5 minutes
String rtc = "";              // real Time Clock
String BLE_Input = "";
String BLE_Output = "";

int batteryChargeTime = 30;   // 30 minutes
unsigned int wapdaInTime = 0; // when wapda is on it store the time stamp.
bool batteriesCharged = false;

String errorCodes = "";
String chargingStatus = "";
// these error codes will be moving in the last row of lcd

struct RTC {
  // Final data : 23/08/26,05:38:34+20
  int milliSeconds = 0;
  int month = 0;
  int date = 0;
  int hour = 0;    // hour will be 05
  int minutes = 0; // minutes will be 38
  int seconds = 0; // seconds will be 34
} RTC;

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
void deleteMessage(int index);
String removeOk(String str);
String executeCommand(String str);
String updateBatteryStatus();
void updateBatteryParameters(String response);
bool timeOut(unsigned int sec, unsigned int entrySec);
void forwardMessage(int index);
String getMobileNumberOfMsg(int index);
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
void SUCCESS_MSG();
void ERROR_MSG();
bool wifi_connected();
void lcd_print();
String thingSpeakLastUpdate();
void drawWifiSymbol(bool isConnected);
void update_distance();
bool change_Detector(int newValue, int previousValue, int margin);
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
void inputManager(String input, int inputFrom);
void initBLE();
void BLE_inputManager(String input);
bool isNum(String num);
bool wapdaOn();
void backup(String state);
void chargeBatteries(bool charge);
String getCompleteString(String str, String target);
int fetchNumber(String str);
bool companyMsg(String);
// # ......... < functions .......
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
std::string receivedData = "";

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) { deviceConnected = true; }

  void onDisconnect(BLEServer *pServer) { deviceConnected = false; }
};

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
      receivedData = value;
      Serial.print("Received from BLE: ");
      Serial.println(receivedData.c_str());
      BLE_inputManager(String(receivedData.c_str()));

      // Print the received data back to the BLE connection
      receivedData = BLE_Output.c_str();
      pCharacteristic->setValue("BLE: " + receivedData);
      pCharacteristic->notify();
    }
  }
};

void BLE_inputManager(String input) {
  if (input.indexOf("#") != -1) { // means string is now fetched completely
    BLE_Input += input.substring(0, input.indexOf("#"));
    println("Executing (BLE input) : {" + BLE_Input + "}");
    inputManager(BLE_Input, 1);
    BLE_Input = "";
  } else {
    BLE_Input += input;
    if (BLE_Input.length() > 50) {
      println("BLE input is getting to large , here is the current string (" +
              BLE_Input + ") flushing it...");
      BLE_Input = "";
    }
  }
}

//! * # # # # # # # # # # # # * !

#define BLYNK_TEMPLATE_ID "TMPL6oAF8teFi"
#define BLYNK_TEMPLATE_NAME "ttgoTcall"
#define BLYNK_AUTH_TOKEN "V0GUIMD1pXGciOfg6LUB7Xd6rhS4iRPp"

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

#include <BlynkSimpleEsp32.h>
#include <WiFiClient.h>
WidgetTerminal terminal(V1);

BLYNK_WRITE(V1) {

  // if you type "Marco" into Terminal Widget - it will respond: "Polo:"
  if (String("Marco") == param.asStr()) {
    terminal.println("You said: 'Marco'");
    terminal.println("I said: 'Polo'");
  } else {

    // Send it back
    terminal.print("You said:");
    terminal.write(param.getBuffer(), param.getLength());
    terminal.println();
  }

  // Ensure everything is sent
  terminal.flush();
}

void setup() {
  SerialMon.begin(115200);
  initBLE();
  // Keep power when running from battery
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
  delay(3000);
  println("Initializing modem...");
  modem.restart();
  delay(2000);
  modem.sendAT(GF("+CLIP=1"));
  delay(300);
  modem.sendAT(GF("+CMGF=1"));
  delay(2000);

  Println("Before Display functionality");
  //`...............................
  delay(2000);
  if (displayWorking) {
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      Serial.println(F("SSD1306 allocation failed"));
      displayWorking = false;
    } else {
      delay(300);
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 0);
      // Display static text
      display.println("Boot Code :  " + String(random(99)));
      display.display();
      delay(500);
    }
  }
  Println("After display functionality");
  delay(500);

  Println(5, "before blynk init");
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, password);
  // You can also specify server:
  // Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass, "blynk.cloud", 80);
  // Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass, IPAddress(192,168,1,100), 8080);

  // Clear the terminal content
  terminal.clear();

  // This will print Blynk Software version to the Terminal Widget when
  // your hardware gets connected to Blynk Server
  terminal.println(F("Blynk v" BLYNK_VERSION ": Device started"));
  terminal.println(F("-------------"));
  terminal.println(F("Type 'Marco' and get a reply, or type"));
  terminal.println(F("anything else and get it printed back."));
  terminal.flush();
  Println(5, "After blynk init");

  dht.begin(); // Initialize the DHT11 sensor
  delay(1000);
  Println("Before wifi connection");
  if (wifiWorking) {
    WiFi.begin(ssid, password);
    println("Connecting");
    int i = 0;
    while (WiFi.status() != WL_CONNECTED) {
      if (i > 10) {
        println("Timeout: Unable to connect to WiFi");
        wifiWorking = false;
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
    }
  }
  delay(500);
  Println("after wifi connection");
  pinMode(LED, OUTPUT);
  delay(100);
  if (ThingSpeakEnable && wifiWorking) {
    Println("\nThinkSpeak initializing...\n");
    ThingSpeak.begin(client); // Initialize ThingSpeak
    delay(500);
    ThingSpeak.setField(4, random(1, 50)); // set any random value.
  }
  Println("\nAfter ThingSpeak");
  //`...............................
  Println("Leaving setup with seconds : " + String(millis() / 1000));

  pinMode(WAPDA_STATE, INPUT);            // Check if wapda is on or off
  pinMode(BATTERY_PAIR_SELECTOR, OUTPUT); // Pair selector
  pinMode(BATTERY_CHARGER, OUTPUT);       // Charge battery or not
  pinMode(POWER_OUTPUT_SELECTOR, OUTPUT); // router INPUT selector

  digitalWrite(BATTERY_PAIR_SELECTOR, HIGH);
  digitalWrite(BATTERY_CHARGER, HIGH);
  digitalWrite(POWER_OUTPUT_SELECTOR, HIGH);
}

void loop() {
  Blynk.run();
  Println("in loop");

  if (deviceConnected) {
    if (!oldDeviceConnected) {
      Serial.println("Connected to device");
      oldDeviceConnected = true;
    }
  } else {
    if (oldDeviceConnected) {
      Serial.println("Disconnected from device");
      Delay(500);
      pServer->startAdvertising(); // restart advertising
      Serial.println("Restart advertising");
      oldDeviceConnected = false;
    }
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
  if (displayWorking) {
    char temperatureStr[5];
    dtostrf(temperature, 4, 1, temperatureStr);
    Println("after DHT conversion");

    line_1 = line_1.substring(0, 6) + String(temperatureStr) + " C  " +
             END_VALUES + " " + chargingStatus;
    Println("after line 1");

    line_2 = "Hu: " + String(humidity) + " % / " + thingSpeakLastUpdate();
    Println("before lcd call");
    Delay(100);
    lcd_print();
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
  receivedData = BLE_Output.c_str();
  pCharacteristic->setValue("BLE: " + receivedData);
  pCharacteristic->notify();
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
  } else if (allowedDebugging[debuggerID - 1]) { // debugging WIFI functionality
    println(str);
  } else if (debuggerID < 0 || debuggerID > 7) {
    Serial.println("Debugger is not defined for this string : " + str);
  }
}

void print(String str) {
  SerialMon.print(str);
  BLE_Output = str;
  receivedData = BLE_Output.c_str();
  pCharacteristic->setValue("BLE: " + receivedData);
  pCharacteristic->notify();
}

void say(String str) { SerialAT.println(str); }

void giveMissedCall() {
  say("ATD+ " + MOBILE_No + ";");
  updateSerial();
  // Delay(20000);            // wait for 20 seconds...
  // say("ATH"); // hang up
  // updateSerial();
}

void call(String number) {
  say("ATD+ " + number + ";");
  updateSerial();
}

void sendSMS(String sms) {
  if (smsAllowed) {
    if (modem.sendSMS(MOBILE_No, sms)) {
      println("$send{" + sms + "}");
    } else {
      println("SMS failed to send");
      println("\n!send{" + sms + "}!\n");
    }
    Delay(500);
  } else {
    println("SMS sending is not allowed");
  }
}

void sendSMS(String sms, String number) {
  if (smsAllowed) {
    if (modem.sendSMS(number, sms)) {
      println("sending : [" + sms + "] to : " + String(number));
    } else {
      println("SMS failed to send");
    }
    Delay(500);
  } else {
    println("SMS sending is not allowed");
  }
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
  if (response.indexOf("+CMTI:") != -1) {
    int newMessageNumber = getNewMessageNumber(response);
    String senderNumber = getMobileNumberOfMsg(newMessageNumber);
    if (senderNumber.indexOf("3374888420") == -1) {
      // if message is not send by module it self
      String temp_str = executeCommand(removeOk(readMessage(newMessageNumber)));
      println("New message [ " + temp_str + "]");
      if (temp_str.indexOf("<executed>") != -1)
        deleteMessage(newMessageNumber);
      else {
        if (!companyMsg(senderNumber)) {
          sendSMS("<Unable to execute sms no. {" + String(newMessageNumber) +
                  "} message : > [ " +
                  temp_str.substring(0, temp_str.indexOf(" <not executed>")) +
                  " ] from : " + senderNumber);
        } else {
          sendSMS("<Unable to execute new sms no. {" +
                  String(newMessageNumber) + "} message : > [ " +
                  temp_str.substring(0, temp_str.indexOf(" <not executed>")) +
                  " ] from : " + senderNumber + ". deleting it...");
          deleteMessage(newMessageNumber);
        }
      }
    }
  } else if (response.indexOf("+CLIP:") != -1) {
    //+CLIP: "03354888420",161,"",0,"",0
    String temp_str = "Missed call from : " + fetchDetails(response, "\"", 1);
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
  Println(3, "message i read : " + tempStr);
  tempStr = tempStr.substring(tempStr.lastIndexOf("\"") + 2);
  Println(3, "message i convert in stage 1 : " + tempStr);
  // remove "\n" from start of string
  tempStr = tempStr.substring(tempStr.indexOf("\n") + 1);
  Println(3, "message i convert in stage 2 : " + tempStr);
  return tempStr;
}

void deleteMessage(int index) {
  if (index == 1) {
    println("you are not allowed to delete message of index 1");
    return;
  }
  say("AT+CMGD=" + String(index));
  println(getResponse());
  messages_in_inbox--;
  arrangeStack();
}

String removeOk(String str) {
  if (str.lastIndexOf("OK") != -1)
    return str.substring(0, str.lastIndexOf("OK") - 2);
  else
    return str;
}

String executeCommand(String str) {
  //~ additional commands will be executed here so define new sms commands
  // here
  if (str.indexOf("<executed>") != -1 || str.indexOf("<not executed>") != -1) {
    println("-> Already executed <-");
    return str;
  } else if (str.indexOf("#callTo") != -1) {
    call(str.substring(str.indexOf("{") + 1, str.indexOf("}")));
    str += " <executed>";
  } else if (str.indexOf("#call") != -1) {
    giveMissedCall();
    str += " <executed>";
  } else if (str.indexOf("#battery") != -1) {
    updateBatteryParameters(updateBatteryStatus());
    sendSMS("Battery percentage : " + String(batteryPercentage) +
            "\nBattery voltage : " + String(batteryVoltage));
    str += " <executed>";
  } else if (str.indexOf("#delete") != -1) {
    // user will send #delete 2, so it will delete message of index 2
    // println("Deleting message of index : " +
    //         str.substring(str.indexOf("#delete") + 8).toInt());
    // deleteMessage(str.substring(str.indexOf("#delete") + 8).toInt());
    deleteMessages(str);
    str += " <executed>";
  } else if (str.indexOf("#forward") != -1) {
    str += " <executed>";
    println("Forwarding message of index : " +
            str.substring(str.indexOf("#forward") + 9).toInt());
    forwardMessage(str.substring(str.indexOf("#forward") + 9).toInt());
  } else if (str.indexOf("#on") != -1) {
    str += " <executed>";
    int switchNumber = str.substring(str.indexOf("#on") + 3).toInt();
    digitalWrite(switchNumber, HIGH);
  } else if (str.indexOf("#off") != -1) {
    str += " <executed>";
    int switchNumber = str.substring(str.indexOf("#off") + 4).toInt();
    digitalWrite(switchNumber, LOW);
  } else if (str.indexOf("#reboot") != -1) {
    println("Rebooting...");
    str += " <executed>";
    modem.restart();
  } else if (str.indexOf("#smsTo") != -1) {
    // smsto [sms here] {number here}
    String strSms = str.substring(str.indexOf("[") + 1, str.indexOf("]"));
    String strNumber = str.substring(str.indexOf("{") + 1, str.indexOf("}"));
    sendSMS(strSms, strNumber);
    str += " <executed>";
  } else if (str.indexOf("#terminator") != -1) {
    str += " <executed>";
    terminateLastMessage();
  } else if (str.indexOf("#allMsg") != -1) {
    println("Reading and forwarding all messages..");
    sendAllMessagesWithIndex();
    str += " <executed>";
  } else if (str.indexOf("#help") != -1) {
    // send sms which includes all the trained commands of this module
    sendSMS("#call\n#callTo{number}\n#battery\n#delete index \n#forward index"

            "\n#display on/"
            "off \n#on pin\n#off pin\n#reboot\n#smsTo[sms]{number}\n#"
            "terminateNext\n#allMsg\n#help");
    str += " <executed>";
  } else if (str.indexOf("#setting") != -1) {
    updateVariablesValues(str);
    deleteMessage(1);
    Delay(500);
    sendSMS(str, "+923374888420");
    str += " <executed>";
  } else if (str.indexOf("#setTime") != -1) {
    str += " <executed>";
  } else if (str.indexOf("#room") != -1) {
    String temp_str = "temperature : " + String(temperature) +
                      "\nhumidity : " + String(humidity);
    sendSMS(temp_str);
  } else {
    println("-> Module is not trained to execute this command ! <-");
    str += " <not executed>";
    messages_in_inbox++;
  }
  return str;
}

String updateBatteryStatus() {
  say("AT+CBC");
  return getResponse();
}

void updateBatteryParameters(String response) {
  // get +CBC: 0,81,4049 in response
  batteryPercentage =
      response.substring(response.indexOf(",") + 1, response.lastIndexOf(","))
          .toInt();
  int milliBatteryVoltage =
      response.substring(response.lastIndexOf(",") + 1, -1).toInt();
  batteryVoltage =
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
    sendSMS("No message found at index : " + String(index));
}

String getMobileNumberOfMsg(int index) {
  // only call this function if want to deal with new message
  say("AT+CMGR=" + String(index));
  String tempStr = getResponse();
  setTime(tempStr);
  if ((tempStr.indexOf("3374888420") != -1 && index != 1) ||
      tempStr.indexOf("setTime") != -1)
    deleteMessage(index); // delete message because it was just to set time
  //+CMGR: "REC READ","+923354888420","","23/07/22,01:02:28+20"
  return fetchDetails(tempStr, ",", 2);
}

String getMobileNumberOfMsg(String index) {
  say("AT+CMGR=" + index);
  String tempStr = getResponse();
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
  currentTargetIndex = getLastIndexToTerminate();
  if (currentTargetIndex == firstMessageIndex() || currentTargetIndex <= 1)
    return;
  println("work index : " + String(currentTargetIndex));
  String temp_str = executeCommand(removeOk(readMessage(currentTargetIndex)));
  println("Last message [ " + temp_str + "]");
  if (temp_str.indexOf("<executed>") != -1) {
    deleteMessage(currentTargetIndex);
    println("Message {" + String(currentTargetIndex) + "} deleted");
  } else { // if the message don't execute
    String mobileNumber = getMobileNumberOfMsg(String(currentTargetIndex));
    if (!checkStack(currentTargetIndex)) {
      if (!companyMsg(mobileNumber)) {
        sendSMS("Unable to execute sms no. {" + String(currentTargetIndex) +
                "} message : [ " +
                temp_str.substring(0, temp_str.indexOf(" <not executed>")) +
                " ] from : " + mobileNumber + ", what to do ?");
        Delay(2000);
      } else {
        sendSMS("Unable to execute previous sms no. {" +
                String(currentTargetIndex) + "} message : [ " +
                temp_str.substring(0, temp_str.indexOf(" <not executed>")) +
                " ] from : " + mobileNumber + ". deleting it...");
        Delay(2000);
        deleteMessage(currentTargetIndex);
      }
    }
  }
}

bool checkStack(int messageNumber) {
  if (getIndex(messageNumber) == -1) {
    for (int i = 0; i < MAX_MESSAGES; i++)
      if (messageStack[i] != 0) {
        messageStack[i] = messageNumber;
        return true;
      }
    println("\n#Error 784\n");
    errorCodes += "784 ";
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
  if (currentTargetIndex == 0) {
    // mean this function is runing first time lets read all messages and get
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
      tempStr = String(i) + " : " + readMessage(i);
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
  if (String(ssid).indexOf("skip") != -1 && wifiWorking) {
    END_VALUES.setCharAt(0, 'Z');
    return;
  }
  WiFi.begin(ssid, password); // Connect to Wi-Fi
  for (int i = 0; !wifi_connected(); i++) {
    if (i > 10) {
      println("Timeout: Unable to connect to WiFi");
      break;
    }
    Delay(500);
    END_VALUES.setCharAt(0, '?');
    Delay(500);
    END_VALUES.setCharAt(0, ' ');
  }
  if (wifi_connected()) {
    END_VALUES.setCharAt(0, '*');
    println("Wi-Fi connected successfully");
  } else {
    END_VALUES.setCharAt(0, '!');
    digitalWrite(LED, LOW);
  }
}

void updateThingSpeak(float temperature, int humidity) {
  if (humidity < 110) {
    ThingSpeak.setField(1, temperature); // Set temperature value
    ThingSpeak.setField(2, humidity);    // Set humidity value
    Println(4, "After setting up fields");

    int updateStatus = ThingSpeak.writeFields(channelID, apiKey);
    if (updateStatus == 200) {
      Println(4, "ThingSpeak update successful");
      SUCCESS_MSG();
    } else {
      Println(4, "Error updating ThingSpeak. Status: " + String(updateStatus));
      ERROR_MSG();
    }
  }
}

void SUCCESS_MSG() {
  // set curser to first row, first last column and print "tick symbol"
  digitalWrite(LED, HIGH);
  END_VALUES.setCharAt(1, '+');
  last_update = (millis() / 1000);
}

void ERROR_MSG() {
  // set curser to first row, first last column and print "tick symbol"
  digitalWrite(LED, LOW);
  END_VALUES.setCharAt(1, '-');
  END_VALUES.setCharAt(1, 'e');
  connect_wifi();
}

bool wifi_connected() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  } else {
    return false;
  }
}

void lcd_print() {
  Println(2, "entering lcd function");
  display.clearDisplay();
  Println(2, "second line of lcd function");
  display.setTextSize(1);
  display.setCursor(0, 0);
  // Display static text
  Println(2, "before checking wifi status lcd function");
  drawWifiSymbol(wifi_connected());
  Println(2, "after checking wifi status lcd function");
  display.print(" ");
  if (messages_in_inbox > 1) {
    display.print(String(messages_in_inbox));
    display.print(" ");
  }
  display.print(batteryPercentage);
  display.print("%");
  display.print(" ");
  display.print(String(RTC.hour) + ":" + String(RTC.minutes) + ":" +
                String(RTC.seconds));
  display.print(" ");
  if (timeVersion) {
    if ((((millis() / 60000) - wapdaInTime) < (batteryChargeTime * 2))) {
      display.print(((((millis() / 1000)) / 60) - wapdaInTime));
    }
    timeVersion = false;
  } else {
    display.print("B:" + String(batteryChargeTime) + "," + wapdaInTime);
    timeVersion = true;
  }

  display.setCursor(0, 17);
  display.print(line_1);

  display.setCursor(0, 32);
  display.print(line_2);

  if (errorCodes.length() > 0) {
    display.setCursor(0, 47);
    display.print(errorCodes);
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
    display.setCursor(0, 0);
    if (!wifiWorking)
      display.print("D");
    else
      display.print("X");
  } else {
    display.setCursor(0, 0);
    display.print("C");
  }
}
//`..................................

bool change_Detector(int newValue, int previousValue, int margin) {
  if ((newValue >= previousValue - margin) &&
      (newValue <= previousValue + margin)) {
    return false;
  } else {
    return true;
  }
}

String getVariablesValues() {
  return String(
      (String(displayWorking ? "Display on" : "Display off") + ", " +
       String(ultraSoundWorking ? "ultraSound on" : "ultraSound off") + ", " +
       String(wifiWorking ? "wifi on" : "wifi off")) +
      " Termination time : " + String(terminationTime) +
      "sms allowed : " + String(smsAllowed) +
      " Battery charge time : " + String(batteryChargeTime));
}

void updateVariablesValues(String str) {
  int newValues = findOccurrences(str, "<");
  if (newValues == 0) {
    println("No new value to be update");
    println("response recived to work on : [" + str + "]");
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
        displayWorking = false;
      } else if (response == 1) {
        displayWorking = true;
      }
    }
    if (str.indexOf("sms") != -1) {
      int response = fetchNumber(getCompleteString(str, "sms"));
      if (response == 0) {
        smsAllowed = false;
      } else if (response == 1) {
        smsAllowed = true;
      }
    }
    if (str.indexOf("wifi") != -1) {
      int response = fetchNumber(getCompleteString(str, "wifi"));
      if (response == 0) {
        wifiWorking = false;
      } else if (response == 1) {
        wifiWorking = true;
        if (ThingSpeakEnable && wifi_connected()) {
          Println(4, "\nThinkSpeak initializing in loop...\n");
          ThingSpeak.begin(client); // Initialize ThingSpeak
        }
      }
    }
    if (str.indexOf("termination") != -1) {
      terminationTime = fetchNumber(getCompleteString(str, "termination"));
    }
    if (str.indexOf("battery") != -1) {
      batteryChargeTime = fetchNumber(getCompleteString(str, "battery"));
    }
  }
  println("msg : " + str + "\nAfter  update : ");
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
  RTC.milliSeconds += miliSeconds;
  bool condition = false;
  Println("Entering wait function...");
  for (int i = 0; (miliSeconds > (i * 5)); i++) {
    if ((millis() / 1000) % terminationTime == 0 &&
        terminationTime > 0) // after every 5 minutes
    {
      Println(3, "before termination");
      terminateLastMessage();
      Println(3, "after termination");
      condition = true;
    }
    if (displayWorking) {
      if ((millis() / 1000) % 100 == 0) {
        Println(2, "before display status work in wait function");
        messages_in_inbox =
            totalUnreadMessages(); // ! will be depreciated in future versions
        updateBatteryParameters(updateBatteryStatus());
        condition = true;
        Println(2, "after display status work in wait function");
      }
    }
    if (ThingSpeakEnable && (getMint() > lastTsUpdate)) {
      if (wifi_connected()) {
        if (wifiWorking) {
          Println(4, "Before Temperature Update");
          updateThingSpeak(temperature, humidity);
          lastTsUpdate = getMint() + UPDATE_THING_SPEAK_TH_AFTER;
          Println(4, "After temperature update");
          condition = true;
        }
      } else { // comment it if you don't want to connect to wifi
        connect_wifi();
        if (wifi_connected()) {
          wifiWorking = true;
          ThingSpeak.begin(client);               // Initialize ThingSpeak
          ThingSpeak.setField(4, random(52, 99)); // set any random value.
        }
      }
    }
    if ((millis() / 1000) > debugFor && DEBUGGING) {
      Println("Disabling DEBUGGING...");
      DEBUGGING = false;
    }
    if (((millis() / 1000) > (debugFor - 30)) &&
        (millis() / 1000) < (debugFor - 27)) {
      // it will just run at first time when system booted
      Println(3, "\nBefore updating values from message 1\n");
      updateVariablesValues(readMessage(1));
      Println(3, "\nValues are updated from message 1\n");
      Delay(1000);
      sendSMS("#setTime", "+923374888420");
      Delay(1000);
      i += 2000;
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
  rtc = fetchDetails(timeOfMessage, "\"23/", "\"", 1);
  println(
      "\n+++++++++++++++++++++++++++++++++++++++++++\n Fetched data from : " +
      timeOfMessage + "\nFinal data : " + rtc +
      "\n+++++++++++++++++++++++++++++++++++++++++++\n");
  // Final data : 23/08/26,05:38:34+20
  setTime();
}

void setTime() { // this function will set RTC struct using rtc string
  // Final data : 23/08/26,05:38:34+20
  int tempDate = RTC.date;
  String datePart = rtc.substring(0, rtc.indexOf(",")); // 23/08/26
  Println("fetched date : " + datePart);
  String date_ = (datePart.substring(datePart.indexOf("/") + 1)); // 08/26
  Println("fetched date_ : " + date_);
  RTC.month = date_.substring(0, datePart.indexOf("/")).toInt(); // 08
  RTC.date = date_.substring(datePart.indexOf("/") + 1).toInt(); // 26

  String timePart = rtc.substring(rtc.indexOf(",") + 1);       // 05:38:34+20
  String time_ = timePart.substring(0, timePart.indexOf("+")); // 05:38:34
  RTC.hour = time_.substring(0, time_.indexOf(":")).toInt();   // 05
  RTC.minutes = time_.substring(time_.indexOf(":") + 1, time_.lastIndexOf(":"))
                    .toInt();                                        // 38
  RTC.seconds = time_.substring(time_.lastIndexOf(":") + 1).toInt(); // 34
  println("RTC updated");
  println("--------------------------------\n");
  println("Hour : " + String(RTC.hour) + " Minutes : " + String(RTC.minutes) +
          " Seconds : " + String(RTC.seconds) + " Day : " + String(RTC.date) +
          " Month : " + String(RTC.month));
  println("--------------------------------");
  if (tempDate == 0 && RTC.date != 0) { // it should be run at fist time.
    String bootMessage =
        "System rebooted, Time stamp is : " + String(RTC.hour) + " : " +
        String(RTC.minutes) + " : " + String(RTC.seconds) + "_" +
        String(RTC.date) + "/" + String(RTC.month);
    sendSMS(bootMessage);
  }
}

void Delay(int milliseconds) {
  for (int i = 0; i < milliseconds; i++) {
    if (wapdaOn()) {   // charge batteries
      backup("WAPDA"); // shift backup to WAPDA
      if (wapdaInTime == 0) {
        wapdaInTime = ((millis() / 1000) / 60);
        println("Shifting router input Source to WAPDA");
      }
      chargeBatteries(true);
    } else {               // power on router from batteries
      backup("Batteries"); // shift backup to WAPDA
      if (wapdaInTime != 0) {
        println("Shifting router to Batteries");
        wapdaInTime = 0;
      }
      // convert milles to minutes>seconds
      chargeBatteries(false);
    }
    delay(1);
  }

  RTC.milliSeconds += milliseconds;
  updateRTC();
}

void updateRTC() {
  if (RTC.milliSeconds > 1000) {
    RTC.milliSeconds -= 1000;
    RTC.seconds++;
  }
  if (RTC.seconds > 59) {
    RTC.seconds -= 60;
    RTC.minutes++;
  }
  if (RTC.minutes > 59) {
    RTC.minutes -= 60;
    RTC.hour++;
  }
  if (RTC.hour > 23) {
    RTC.hour -= 24;
    RTC.date++;
  }
  // still the loop hole is present for month increment.
}

void deleteMessages(String deleteCommand) {
  // let say deleteCommand = "#delete 3,4,5"
  int num = 0;
  String numHolder = "";
  String targetCommand =
      deleteCommand.substring(deleteCommand.indexOf("delete") + 7, -1);
  // targetCommand = "3,4,5"
  for (int i = 0; i <= targetCommand.length(); i++) {
    {
      if (targetCommand[i] == ',' || targetCommand[i] == ' ') {
        if (isNum(numHolder)) {
          println("$Deleting message number : " + numHolder);
          deleteMessage(numHolder.toInt());
          numHolder = "";
        } else {
          println("Error in delete command");
          println("numHolder : " + numHolder);
        }
      } else {
        numHolder += targetCommand[i];
      }
    }
  }
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
  Println("beginOfTarget : " + beginOfTarget);
  return beginOfTarget.substring(padding - 1,
                                 beginOfTarget.indexOf(end) - (padding - 1));
}

void initBLE() {
  BLEDevice::init("TTGO T-Call BLE");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(
      BLEUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b")); // Custom service UUID
  pCharacteristic = pService->createCharacteristic(
      BLEUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8"), // Custom
                                                       // characteristic UUID
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(pService->getUUID());
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); // sets the minimum advertising interval
  BLEDevice::startAdvertising();

  Serial.println("Waiting for Bluetooth connection...");
}

bool isNum(String num) {
  if (num.toInt() > -9999 && num.toInt() < 9999)
    return true;
  else
    return false;
}

bool wapdaOn() {
  if (digitalRead(WAPDA_STATE) == HIGH)
    return true;
  else
    return false;
}

void backup(String state) {
  if (state == "WAPDA") {
    digitalWrite(POWER_OUTPUT_SELECTOR, LOW); // Relay module ON on low
  } else if (state == "Batteries") {
    digitalWrite(POWER_OUTPUT_SELECTOR, HIGH); // Relay module OFF on high
  } else {
    println("Error in backup function");
    errorCodes += "1367 ";
  }
}

void chargeBatteries(bool charge) {
  if (!batteriesCharged && charge) {
    if ((((millis() / 1000) / 60) - wapdaInTime) < (batteryChargeTime * 2)) {
      digitalWrite(BATTERY_CHARGER, LOW); // on
      if ((((millis() / 1000) / 60) - wapdaInTime) < batteryChargeTime) {
        // charge first pair of batteries
        digitalWrite(BATTERY_PAIR_SELECTOR, HIGH);
        chargingStatus = "C1";
      } else {
        // charge second pair of batteries
        digitalWrite(BATTERY_PAIR_SELECTOR, LOW);
        chargingStatus = "C2";
      }
    } else {
      digitalWrite(BATTERY_CHARGER, HIGH); // off
      digitalWrite(BATTERY_PAIR_SELECTOR, HIGH);
      chargingStatus = "NC";
    }
  } else if (!charge) {
    digitalWrite(BATTERY_CHARGER, HIGH); // on
    digitalWrite(BATTERY_PAIR_SELECTOR, HIGH);
    chargingStatus = "CC";
  } else {
    println("unexpected error at chargeBatteries function");
    errorCodes += "1385 ";
  }
}

String getCompleteString(String str, String target) {
  //" #setting <ultra sound alerts 0> <display 1>" , "ultra"
  // return "ultra sound alerts 0"
  String tempStr = "";
  int i = str.indexOf(target);
  if (i == -1) {
    println("String is empty" + str);
    return tempStr;
  }
  for (; i < str.length(); i++) {
    if ((str[i] >= '0' && str[i] <= '9') || (str[i] >= 'a' && str[i] <= 'z') ||
        (str[i] >= 'A' && str[i] <= 'Z') || (str[i] == ' '))
      tempStr += str[i];
    else {
      //   println("Returning (complete string) 1: " + tempStr);
      return tempStr;
    }
  }
  //   println("Returning (complete string) 2: " + tempStr);
  return tempStr;
}

int fetchNumber(String str) {
  // chargeFor 30
  //   println("fetchNumber function called with : " + str);
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
  // input from indicated from where the input is coming BLE, Serial or sms
  if (command.indexOf("smsTo") != -1) {
    String strSms =
        command.substring(command.indexOf("[") + 1, command.indexOf("]"));
    String strNumber =
        command.substring(command.indexOf("{") + 1, command.indexOf("}"));
    sendSMS(strSms, strNumber);
  } else if (command.indexOf("callTo") != -1) {
    call(command.substring(command.indexOf("{") + 1, command.indexOf("}")));
  } else if (command.indexOf("call") != -1) {
    println("Calling " + String(MOBILE_No));
    giveMissedCall();
  } else if (command.indexOf("sms") != -1) {
    // fetch sms from input string, sample-> sms : msg here
    String sms = command.substring(command.indexOf("sms") + 4);
    println("Sending SMS : " + sms + " to : " + String(MOBILE_No));
    sendSMS(sms);
  } else if (command.indexOf("all") != -1) {
    println("Reading all messages");
    moduleManager();
  } else if (command.indexOf("battery") != -1) {
    println(updateBatteryStatus());
  } else if (command.indexOf("lastBefore") != -1) {
    println("Index before <" + command.substring(11, -1) + "> is : " +
            String(getMessageNumberBefore(command.substring(11, -1).toInt())));
  } else if (command.indexOf("read") != -1) {
    int targetMsg = fetchNumber(command);
    if (messageExists(targetMsg))
      println("Message: " + readMessage(targetMsg));
    else
      println("Message not Exists");
  } else if (command.indexOf("delete") != -1) { // to delete message
    deleteMessages(command);
  } else if (command.indexOf("terminator") != -1) {
    terminateLastMessage();
  } else if (command.indexOf("hangUp") != -1) {
    say("AT+CHUP");
  } else if (command.indexOf("debug") != -1) {
    DEBUGGING ? DEBUGGING = false : DEBUGGING = true;
    Delay(50);
    println(String("Debugging : ") + (DEBUGGING ? "Enabled" : "Disabled"));
  } else if (command.indexOf("status") != -1) {
    println(getVariablesValues());
  } else if (command.indexOf("update") != -1) {
    updateVariablesValues(readMessage(
        (command.substring(command.indexOf("update") + 7, -1)).toInt()));
  } else if (command.indexOf("reboot") != -1) {
    println("Rebooting...");
    modem.restart();
  } else if (command.indexOf("time") != -1) {
    if (rtc.length() < 2) {
      String tempTime =
          "+CMGL: 1,\"REC "
          "READ\",\"+923374888420\",\"\",\"23/08/14,17:21:05+20\"";
      println("dummy time : [" + tempTime + "]");
      setTime(tempTime);
    }
    println("Time String : " + rtc);
    // println(Time.Date);
    // println(Time.hour);
    // println(Time.minutes);
  } else if (command.indexOf("!") != -1) {
    if (command.indexOf("r11") != -1) {
      digitalWrite(BATTERY_PAIR_SELECTOR, LOW);
      delay(1000);
    } else if (command.indexOf("r10") != -1) {
      digitalWrite(BATTERY_PAIR_SELECTOR, HIGH);
      delay(1000);
    } else if (command.indexOf("r2a1") != -1) {
      digitalWrite(BATTERY_CHARGER, LOW);
      delay(1000);
    } else if (command.indexOf("r2a0") != -1) {
      digitalWrite(BATTERY_CHARGER, HIGH);
      delay(1000);
    } else if (command.indexOf("r2b1") != -1) {
      digitalWrite(POWER_OUTPUT_SELECTOR, LOW);
      delay(1000);
    } else if (command.indexOf("r2b0") != -1) {
      digitalWrite(POWER_OUTPUT_SELECTOR, HIGH);
      delay(1000);
    }
  } else if (command.indexOf("switch") != -1) {
    if (command.indexOf("0") != -1)
      digitalWrite(BATTERY_PAIR_SELECTOR, HIGH);
    else if (command.indexOf("1") != -1)
      digitalWrite(BATTERY_PAIR_SELECTOR, LOW);
    else
      println("Error in switch command");
  } else if (command.indexOf("chargeFor") != -1) {
    batteryChargeTime = fetchNumber(getCompleteString(command, "chargeFor"));
    println("Battery charge time updated to : " + String(batteryChargeTime) +
            " minutes");
  } else if (command.indexOf("updateTime") != -1) {
    println("Updating time");
    sendSMS("#setTime", "+923374888420");
  } else if (command.indexOf("whatsapp") != -1) {
    sendWhatsappMsg("test_message_from_esp32");
  } else {
    println("Executing: " + command);
    say(command);
  }
}

bool companyMsg(String mobileNumber) {
  if (mobileNumber.indexOf("Telenor") != -1)
    return true;
  else if (mobileNumber.indexOf("Jazz") == -1 ||
           mobileNumber.indexOf("JAZZ") == -1)
    return true;
  else if (mobileNumber.indexOf("Zong") != -1)
    return true;
  else if (mobileNumber.indexOf("Ufone") != -1)
    return true;
  else if (mobileNumber.indexOf("Warid") != -1)
    return true;
  else
    return false;
}

void sendWhatsappMsg(String message) {
  if (RTC.date == 0) {
    println("RTC not updated yet so wait for it to avoid unexpected behaviour");
    return;
  }
  HTTPClient http;
  String serverPath = getServerPath(get_HTTP_string(message));
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
}

String getServerPath(String message) {
  String whatsappNumber;
  String api;
  if (whatsappMessageNumber == -1) {
    whatsappMessageNumber = getMessagesCounter();
    if (whatsappMessageNumber < 0 && whatsappMessageNumber > 100) {
      println("Unexpected behaviour in getMessagesCounter()");
      errorCodes += "1608";
    }
    whatsappNumber = NUMBER[whatsappMessageNumber / 25];
    api = API[whatsappMessageNumber / 25];

  } else {
    whatsappNumber = NUMBER[whatsappMessageNumber / 25];
    api = API[whatsappMessageNumber / 25];
    println("Using " + whatsappNumber +
            "for whatsapp message : " + String(whatsappMessageNumber));
  }
  String serverPath = server + whatsappNumber + get_HTTP_string(message) + api;
  Println(5, "returning mobile number : " + String(whatsappNumber));
  Println(5, "returning api : " + String(api));
  return serverPath;
}

int getMessagesCounter() {
  int todayMessages = -1;
  int lastUpdateOfThingSpeakMessageCounter =
      getThingSpeakFieldData(lastMessageUpdateID);
  if (lastUpdateOfThingSpeakMessageCounter != RTC.date) {
    Println(5, "Day changed initiating new counter");
    setThingSpeakFieldData(lastMessageUpdateID, RTC.date);
    setThingSpeakFieldData(messagesCounterID, 1);
    writeThingSpeakData();
    todayMessages = 1;
  } else {
    todayMessages = getThingSpeakFieldData(messagesCounterID);
    Println(5, "Else: Fetched data : " + String(todayMessages));
  }
  Println(5, "Returning Counter : " + String(todayMessages));
  return todayMessages;
}

int getThingSpeakFieldData(int fieldNumber) {
  int data = ThingSpeak.readIntField(channelID, fieldNumber);
  int statusCode = ThingSpeak.getLastReadStatus();
  if (statusCode == 200) {
    Println(5, "Data fetched from field : " + String(fieldNumber));
  } else {
    Println(5,
            "Problem reading channel. HTTP error code " + String(statusCode));
    data = -1;
  }
  return data;
}

void updateWhatsappMessageCounter() {
  setThingSpeakFieldData(lastMessageUpdateID, RTC.date);
  setThingSpeakFieldData(messagesCounterID, ++whatsappMessageNumber);
  writeThingSpeakData();
}

void setThingSpeakFieldData(int field, int data) {
  ThingSpeak.setField(field, data);
}

void writeThingSpeakData() {
  int updateStatus = ThingSpeak.writeFields(channelID, apiKey);
  if (updateStatus == 200) {
    Println(4, "ThingSpeak update successfully...");
  } else {
    Println(4, "Error updating ThingSpeak. Status: " + String(updateStatus));
  }
}

unsigned int getMint() { return ((millis() / 1000) / 60); }

String get_HTTP_string(String message) {
  String tempStr = "";
  for (int i = 0; i < message.length(); i++) {
    if (message[i] == ' ')
      tempStr += "_";
    else
      tempStr += message[i];
  }
  return tempStr;
}