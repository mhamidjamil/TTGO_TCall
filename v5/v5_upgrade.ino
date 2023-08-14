//$ last work 14/August/23 [6:50 PM]
// # version 5.1.5
// variables position adjusted and more debug information added
const char simPIN[] = "";

String MOBILE_No = "+923354888420";

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
#define ALERT_PIN 12
#define LED 13

#define echoPin 2
#define trigPin 15

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
const char *ssid = "Archer 73";
const char *password = "Archer@73_102#";
const unsigned long channelID = 2201589;
const char *apiKey = "Q3TSTOM87EUBNOAE";

unsigned long updateInterval = 2 * 60;
unsigned long previousUpdateTime = 0;
unsigned int last_update = 0; // in seconds
WiFiClient client;

String END_VALUES = "  ";
String line_1 = "Temp: 00.0 C";
String line_2 = "                ";

String receivedMessage = ""; // Global variable to store received SMS message
double batteryVoltage = 0;
int batteryPercentage = 0;
// another variable to store time in millis
int messages_in_inbox = 0;
byte batteryUpdateAfter = 1; // 1 mean 2 minutes
int messageStack[MAX_MESSAGES] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int currentTargetIndex = 0;

const unsigned char wifiSymbol[] PROGMEM = {B00000000, B01111110, B10000001,
                                            B01111100, B10000010, B00111000,
                                            B01000100, B00010000};

const unsigned char questionMark[] PROGMEM = {B00111000, B01000100, B10000010,
                                              B00000100, B00001000, B00010000,
                                              B00010000, B0001000};

long duration;    // variable for the duration of sound wave travel
int distance = 0; // variable for the distance measurement

float temperature;
int humidity;

bool DEBUGGING = true;
bool ultraSoundWorking = false;
bool wifiWorking = false;
bool displayWorking = true;

// # ......... functions > .......
void println(String str);
void Println(String str);
void print(String str);
void say(String str);
void giveMissedCall();
void call(String number);
void sendSms(String sms);
void sendSMS(String sms, String number);
void updateSerial();
void moduleManager();
void getLastMessageAndIndex(String response, String &lastMessage,
                            int &messageNumber);
String getResponse();
String simResponse();
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
String fetchDetails(String str, String begin_end, int padding);
int totalUnreadMessages();
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
String get_time();
void drawWifiSymbol(bool isConnected);
void update_distance();
bool change_Detector(int newValue, int previousValue, int margin);
String getVariablesValues();
void updateVariablesValues(String str);
int findOccurrences(String str, String target);
void wait(unsigned int seconds);
// # ......... < functions .......

//! * # # # # # # # # # # # # * !

void setup() {
  SerialMon.begin(115200);
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
  modem.sendAT(GF("+CLIP=1"));
  modem.sendAT(GF("+CMGF=1"));
  pinMode(ALERT_PIN, OUTPUT);
  delay(1000);

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
      display.println("Hello, world!  " + String(random(99)));
      display.display();
      delay(500);
    }
  }
  dht.begin(); // Initialize the DHT11 sensor
  Println("before wifi connection");

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
    }
    if (i < 10) {
      println("");
      print("Connected to WiFi network with IP Address: ");
      println(String(WiFi.localIP()));
    }
  }

  Println("after wifi connection");
  pinMode(LED, OUTPUT);
  delay(100);
  if (ThingSpeakEnable && wifiWorking) {
    Println("\nThinkSpeak initializing...\n");
    ThingSpeak.begin(client); // Initialize ThingSpeak
  }
  Println("\nAfter ThingSpeak");
  if (wifiWorking) {
    END_VALUES.setCharAt(1, '#');
    messages_in_inbox = totalUnreadMessages();
    updateBatteryParameters(updateBatteryStatus());
  }
  //`...............................
  // #-------------------------------
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an OUTPUT
  pinMode(echoPin, INPUT);  // Sets the echoPin as an INPUT
  Println("leaving setup");
}

void loop() {
  Println("in loop");
  delay(100);
  if (SerialMon.available()) {
    String command = SerialMon.readString();
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

      sendSms(sms);
    } else if (command.indexOf("all") != -1) {
      println("Reading all messages");
      moduleManager();
    } else if (command.indexOf("battery") != -1) {
      println(updateBatteryStatus());
    } else if (command.indexOf("lastBefore") != -1) {
      println(
          "Index before <" + command.substring(11, -1) + "> is : " +
          String(getMessageNumberBefore(command.substring(11, -1).toInt())));
    } else if (command.indexOf("read") != -1) {
      if (messageExists(command.substring(command.indexOf("read") + 5).toInt()))
        println("Message: " +
                readMessage(
                    command.substring(command.indexOf("read") + 5).toInt()));
      else
        println("Message not Exists");
    } else if (command.indexOf("delete") != -1) { // to delete message
      println("Deleting message number : " +
              String(command.substring(command.indexOf("delete") + 7)));
      deleteMessage(command.substring(command.indexOf("delete") + 7).toInt());
    } else if (command.indexOf("terminator") != -1) {
      terminateLastMessage();
    } else if (command.indexOf("hangUp") != -1) {
      say("AT+CHUP");
    } else if (command.indexOf("debug") != -1) {
      DEBUGGING ? DEBUGGING = false : DEBUGGING = true;
      delay(50);
      println(String("Debugging : ") + (DEBUGGING ? "Enabled" : "Disabled"));
    } else if (command.indexOf("status") != -1) {
      println(getVariablesValues());
    } else if (command.indexOf("update") != -1) {
      updateVariablesValues(readMessage(
          (command.substring(command.indexOf("update") + 7, -1)).toInt()));
    } else if (command.indexOf("reboot") != -1) {
      println("Rebooting...");
      modem.restart();
    } else {
      println("Executing: " + command);
      say(command);
    }
  }
  Println("after serial test");
  if (SerialAT.available() > 0)
    Serial.println(getResponse());
  Println("after AT check");
  delay(100);
  //`..................................
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  Println("after DHT reading");
  char temperatureStr[5];
  dtostrf(temperature, 4, 1, temperatureStr);
  Println("after DHT conversion");

  line_1 =
      line_1.substring(0, 6) + String(temperatureStr) + " C  " + END_VALUES;
  Println("after line 1");

  line_2 = "Hu: " + String(humidity) + " % / " + get_time();
  Println("before lcd update");
  delay(100);
  if (displayWorking)
    lcd_print();
  Println("after lcd update");

  if (ThingSpeakEnable && wifiWorking && wifi_connected() &&
      (((millis() / 1000) - previousUpdateTime) >= updateInterval)) {
    delay(100);
    previousUpdateTime = (millis() / 1000);
    Println("before thingspeak update");
    updateThingSpeak(temperature, humidity);
    Println("After thingspeak update");
  }

  if (displayWorking) {
    messages_in_inbox = totalUnreadMessages();
    delay(100);
    if (batteryUpdateAfter >= 5) {
      updateBatteryParameters(updateBatteryStatus());
      batteryUpdateAfter = 0;
    } else {
      batteryUpdateAfter++;
    }
  }
  wait(100);
  Println("after millis condition");
  //`..................................

  // #----------------------------------
  int previousValue = distance;
  update_distance();
  Println("After distance update");
  wait(100);
  int newValue = distance;
  Println("checking distance status");
  if (change_Detector(abs(newValue), abs(previousValue), 2)) {
    wait(100);
    if (distance < 0) {
      println("Distance  : " + String(abs(distance)) + " inches");
      String temp_msg =
          "Motion detected by sensor new value : " + String(abs(newValue)) +
          " previous value : " + String(abs(previousValue));
      if (ultraSoundWorking) {
        sendSms(temp_msg);
      }
      distance *= -1;
    } else {
      println("Distance  : " + String(abs(distance)) + " inches (ignored)");
      distance *= -1;
      // println("*__________*");
    }
  }
  wait(1000);
  Println("loop end");
  if ((millis() / 1000) > 130 && DEBUGGING) {
    println("disabling DEBUGGING");
    DEBUGGING = false;
  } else if (((millis() / 1000) > 100) && (millis() / 1000) < 105) {
    wait(5000);
    updateVariablesValues(readMessage(1));
  }
}

void println(String str) { SerialMon.println(str); }
void Println(String str) {
  if (DEBUGGING) {
    Serial.println(str);
  }
}

void print(String str) { SerialMon.print(str); }
void say(String str) { SerialAT.println(str); }

void giveMissedCall() {
  say("ATD+ " + MOBILE_No + ";");
  updateSerial();
  // delay(20000);            // wait for 20 seconds...
  // say("ATH"); // hang up
  // updateSerial();
}

void call(String number) {
  say("ATD+ " + number + ";");
  updateSerial();
}

void sendSms(String sms) {
  if (modem.sendSMS(MOBILE_No, sms)) {
    println("$send{" + sms + "}");
  } else {
    println("SMS failed to send");
    println("\n!send{" + sms + "}!\n");
  }
  delay(500);
}

void sendSMS(String sms, String number) {
  if (modem.sendSMS(number, sms)) {
    println("sending : [" + sms + "] to : " + String(number));
  } else {
    println("SMS failed to send");
  }
  delay(500);
}

void updateSerial() {
  delay(100);
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
  Println("entering getResponse");
  delay(100);
  String response = "";
  if ((SerialAT.available() > 0)) {
    Println("reading serial data");
    response += SerialAT.readString();
  }
  Println("After while loop in get response");
  if (response.indexOf("+CMTI:") != -1) {
    int newMessageNumber = getNewMessageNumber(response);
    String senderNumber = getMobileNumberOfMsg(newMessageNumber);
    if (senderNumber.indexOf("3374888420") == -1) {
      String temp_str = executeCommand(removeOk(readMessage(newMessageNumber)));
      println("New message [ " + temp_str + "]");
      if (temp_str.indexOf("<executed>") != -1)
        deleteMessage(newMessageNumber);
      else {
        sendSms("<Unable to execute sms no. {" + String(newMessageNumber) +
                "} message : > [ " +
                temp_str.substring(0, temp_str.indexOf(" <not executed>")) +
                " ] from : " + senderNumber);
      }
    }
  } else if (response.indexOf("+CLIP:") != -1) {
    //+CLIP: "03354888420",161,"",0,"",0
    String temp_str = "Missed call from : " + fetchDetails(response, "\"", 1);
    sendSms(temp_str);
    // println(temp_str);
  }
  Println("Leaving response function with response [" + response + "]");
  return response;
}

String simResponse() { //! function will be deleted in version 6
  Println("entering simResponse");
  // replica of getResponse function, #the old get response function
  delay(100);
  String response = "";
  unsigned int entrySec = (millis() / 1000);
  unsigned int timeoutSec = 3;
  if ((SerialAT.available() > 0)) {
    Println("reading serial data simResponse");
    while ((SerialAT.available() > 0) ||
           (!((response.indexOf("OK") != -1) ||
              (response.indexOf("ERROR") != -1)))) {
      response += SerialAT.readString();
      if (timeOut(timeoutSec, entrySec) && !(SerialAT.available() > 0)) {
        println(String("******\tTimeout\t******"));
        break;
      } else if (SerialAT.available()) {
        timeoutSec = 1;
      }
    }
  }
  Println("After while loop in simResponse");
  if (response.indexOf("+CMTI:") != -1) {
    int newMessageNumber = getNewMessageNumber(response);
    String senderNumber = getMobileNumberOfMsg(newMessageNumber);
    if (senderNumber.indexOf("3374888420") == -1) {
      String temp_str = executeCommand(removeOk(readMessage(newMessageNumber)));
      println("New message [ " + temp_str + "]");
      if (temp_str.indexOf("<executed>") != -1)
        deleteMessage(newMessageNumber);
      else {
        sendSms("<Unable to execute sms no. {" + String(newMessageNumber) +
                "} message : > [ " +
                temp_str.substring(0, temp_str.indexOf(" <not executed>")) +
                " ] from : " + senderNumber);
      }
    }
  } else if (response.indexOf("+CLIP:") != -1) {
    //+CLIP: "03354888420",161,"",0,"",0
    String temp_str = "Missed call from : " + fetchDetails(response, "\"", 1);
    sendSms(temp_str);
    // println(temp_str);
  }
  Println("Leaving simResponse function with response [" + response + "]");
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
  Println("message i read : " + tempStr);
  tempStr = tempStr.substring(tempStr.lastIndexOf("\"") + 2);
  Println("message i convert in stage 1 : " + tempStr);
  // remove "\n" from start of string
  tempStr = tempStr.substring(tempStr.indexOf("\n") + 1);
  Println("message i convert in stage 2 : " + tempStr);
  return tempStr;
}

void deleteMessage(int index) {
  say("AT+CMGD=" + String(index));
  println(getResponse());
  arrangeStack();
}

String removeOk(String str) {
  if (str.lastIndexOf("OK") != -1)
    return str.substring(0, str.lastIndexOf("OK") - 2);
  else
    return str;
}

String executeCommand(String str) {
  //~ additional commands will be executed here so define new sms commands here
  if (str.indexOf("<executed>") != -1 || str.indexOf("<not executed>") != -1) {
    println("-> Already executed <-");
    return str;
  } else if (str.indexOf("#call") != -1) {
    giveMissedCall();
    str += " <executed>";
  } else if (str.indexOf("#callTo") != -1) {
    call(str.substring(str.indexOf("{") + 1, str.indexOf("}")));
    str += " <executed>";
  } else if (str.indexOf("#battery") != -1) {
    updateBatteryParameters(updateBatteryStatus());
    sendSms("Battery percentage : " + String(batteryPercentage) +
            "\nBattery voltage : " + String(batteryVoltage));
    str += " <executed>";
  } else if (str.indexOf("#delete") != -1) {
    // user will send #delete 2, so it will delete message of index 2
    println("Deleting message of index : " +
            str.substring(str.indexOf("#delete") + 8).toInt());
    deleteMessage(str.substring(str.indexOf("#delete") + 8).toInt());
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
    sendSms("#call\n#callTo{number}\n#battery\n#delete index \n#forward index"

            "\n#display on/"
            "off \n#on pin\n#off pin\n#reboot\n#smsTo[sms]{number}\n#"
            "terminateNext\n#allMsg\n#help");
    str += " <executed>";
  } else if (str.indexOf("#setting") != -1) {
    updateVariablesValues(str);
    deleteMessage(1);
    delay(500);
    sendSMS(str, "+923374888420");
    str += " <executed>";
  } else if (str.indexOf("#room") != -1) {
    String temp_str = "temperature : " + String(temperature) +
                      "\nhumidity : " + String(humidity);
    sendSms(temp_str);
  } else {
    println("-> Module is not trained to execute this command ! <-");
    str += " <not executed>";
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
    sendSms(message);
  else
    sendSms("No message found at index : " + String(index));
}

String getMobileNumberOfMsg(int index) {
  say("AT+CMGR=" + String(index));
  String tempStr = getResponse();
  //+CMGR: "REC READ","+923354888420","","23/07/22,01:02:28+20"
  return fetchDetails(tempStr, ",", 2);
}

String fetchDetails(String str, String begin_end, int padding) {
  String beginOfTarget = str.substring(str.indexOf(begin_end) + 1, -1);
  return beginOfTarget.substring(padding - 1, beginOfTarget.indexOf(begin_end) -
                                                  (padding - 1));
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
  if (currentTargetIndex == firstMessageIndex() || currentTargetIndex == 1)
    return;
  println("work index : " + String(currentTargetIndex));
  String temp_str = executeCommand(removeOk(readMessage(currentTargetIndex)));
  println("Last message [ " + temp_str + "]");
  if (temp_str.indexOf("<executed>") != -1) {
    deleteMessage(currentTargetIndex);
    println("Message {" + String(currentTargetIndex) + "} deleted");
  } else { // if the message don't execute
    if (!checkStack(currentTargetIndex)) {
      sendSms("Unable to execute sms no. {" + String(currentTargetIndex) +
              "} message : [ " +
              temp_str.substring(0, temp_str.indexOf(" <not executed>")) +
              " ] from : " + getMobileNumberOfMsg(currentTargetIndex) +
              ", what to do ?");
      delay(2000);
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
    println("\n#Error 495\n");
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
      if (modem.sendSMS(MOBILE_No, tempStr)) {
        println("!send{" + tempStr + "}");
      } else {
        println("SMS failed to send");
      }
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
    delay(500);
    END_VALUES.setCharAt(0, '?');
    delay(500);
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
  Println("entering thingspeak function");
  ThingSpeak.setField(1, temperature); // Set temperature value
  ThingSpeak.setField(2, humidity);    // Set humidity value
  Println("After setting up fields");

  int updateStatus = ThingSpeak.writeFields(channelID, apiKey);
  if (updateStatus == 200) {
    println("ThingSpeak update successful");
    SUCCESS_MSG();
  } else {
    print("Error updating ThingSpeak. Status: ");
    println(String(updateStatus));
    ERROR_MSG();
  }
  Println("leaving thingspeak function");
}

void SUCCESS_MSG() {
  // set curser to first row, first last column and print "tick symbol"
  digitalWrite(LED, HIGH);
  END_VALUES.setCharAt(1, '+');
  last_update = (millis() / 1000);
  if (updateInterval > 2 * 60)
    updateInterval = 2 * 60;
}

void ERROR_MSG() {
  // set curser to first row, first last column and print "tick symbol"
  digitalWrite(LED, LOW);
  END_VALUES.setCharAt(1, '-');
  if (updateInterval < 60 * 30)
    updateInterval *= 2;
  else
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
  Println("entering lcd function");
  display.clearDisplay();
  Println("second line of lcd function");
  display.setTextSize(1);
  display.setCursor(0, 0);
  // Display static text
  Println("before checking wifi status lcd function");
  drawWifiSymbol(wifi_connected());
  Println("after checking wifi status lcd function");
  display.print("   ");
  display.print(String(messages_in_inbox));
  display.print("   ");
  display.print(batteryPercentage);
  display.print("%");
  display.print("   ");
  display.print(batteryVoltage);
  display.print("V");

  display.setCursor(0, 20);
  display.print(line_1);

  display.setCursor(0, 40);
  display.print(line_2);

  display.display();
  wait(1000);
  Println("leaving lcd function");
}

String get_time() {
  Println("entering time function");
  unsigned int sec = (millis() / 1000) - last_update;
  if (sec < 60) {
    return (String(sec) + " s");
  } else if ((sec >= 60) && (sec < 3600)) { // deal one hour
    return (String(sec / 60) + " m " + String(sec % 60) + " s");
  } else if ((sec >= 3600) && (sec < 86400)) { // deal one day
    return (" " + String(sec / 3600) + " h " + String((sec % 3600) / 60) +
            " m   " + String(sec % 60) + " s");
  } else if ((sec >= 86400) && (sec < 604800)) { // deal one week
    return (String(sec / 86400) + " d " + String((sec % 86400) / 3600) + " h " +
            String((sec % 3600) / 60) + " m " + String(sec % 60) + " s");
  } else {
    println("Issue spotted sec value: " + String(sec));
    sendSms("Got problem in time function time overflow (or module runs for "
            "more than a week want to reboot send #reboot sms)");
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
  } else
    display.drawBitmap(0, 0, wifiSymbol, 8, 8, WHITE);
}
//`..................................

// #---------------------------------

void update_distance() {
  Println("entered into update_distance function");
  digitalWrite(trigPin, LOW);
  delay(50);
  digitalWrite(trigPin, HIGH);
  delay(50);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  int tempDistance = duration * 0.034 / 2;
  if (tempDistance < 1000) { // ignore unwanted readings
    if (distance > 0) {
      distance = tempDistance;
    } else {
      distance = tempDistance;
      distance *= -1;
    }
  }
  Println("Leaving update_distance function");
}

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
       String(wifiWorking ? "wifi on" : "wifi off")));
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
      String forUltraSound =
          str.substring(str.indexOf("<ultra sound alerts") + 20,
                        str.indexOf("<ultra sound alerts") + 22);
      if (forUltraSound.indexOf("0") != -1) {
        ultraSoundWorking = false;
      } else if (forUltraSound.indexOf("1") != -1) {
        ultraSoundWorking = true;
      }
    }
    if (str.indexOf("display") != -1) {
      String forDisplay = str.substring(str.indexOf("<display") + 9,
                                        str.indexOf("<display") + 11);
      if (forDisplay.indexOf("0") != -1) {
        displayWorking = false;
      } else if (forDisplay.indexOf("1") != -1) {
        displayWorking = true;
      }
    }
    if (str.indexOf("wifi") != -1) {
      String forWifi = str.substring(str.indexOf("<wifi connectivity") + 19,
                                     str.indexOf("<wifi connectivity") + 21);
      if (forWifi.indexOf("0") != -1) {
        wifiWorking = false;
      } else if (forWifi.indexOf("1") != -1) {
        wifiWorking = true;
        if (ThingSpeakEnable && wifi_connected()) {
          Println("\nThinkSpeak initializing in loop...\n");
          ThingSpeak.begin(client); // Initialize ThingSpeak
        }
      }
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

void wait(unsigned int seconds) { // most important task will be executed here
  for (int i = 0; (seconds > (i * 5)); i++) {
    if ((millis() / 1000) % 300 == 0) // after every 5 minutes
    {
      terminateLastMessage();
      delay(1000);
    }
    delay(5);
  }
}
