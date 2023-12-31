//$ last work 26/August/23 [10:39 PM]
// # version 5.2.4 // failed
// # Release Note : RTC functionality is added completely
// but not tested it completely

//! ultrasound values are not accurate

//` All messages are fetched more then 5 times in 1st 2 minutes :FIX_IT

//` use display last row as a debugging process.

//` what if ran out of balance of package ? add this check so module will
// not send any message if there is no package is subscribed

#include "arduino_secrets.h"

const char simPIN[] = "";

String MOBILE_No = MY_Number;

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800   // Modem is SIM800
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb

#include <TinyGsmClient.h>
#include <Wire.h>
//`===================================
#include <WiFi.h>
#include <random>

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

#define IP5306_ADDR 0x75
#define IP5306_REG_SYS_CTL0 0x00

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

WiFiClient client;

double batteryVoltage = 0;
int batteryPercentage = 0;
// another variable to store time in millis
int messages_in_inbox = 0;
int messageStack[MAX_MESSAGES] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int currentTargetIndex = 0;

bool DEBUGGING = true;

int terminationTime = 60 * 5; // 5 minutes
String rtc = "";              // real Time Clock
bool wifiWorking = true;
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
void setTime(); // TODO: update time in real time
String fetchDetails(String str, String begin_end, int padding);
void updateRTC();
void Delay(int milliSeconds);
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
  delay(2000);
  modem.sendAT(GF("+CLIP=1"));
  delay(300);
  modem.sendAT(GF("+CMGF=1"));
  delay(2000);
  Println("Before wifi connection");

  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  delay(500);
  Println("after wifi connection");
  Println("Leaving setup with seconds : " + String(millis() / 1000));
  delay(2000);
}

void loop() {
  Println("in loop");
  Delay(100);
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

      sendSMS(sms);
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
    } else {
      println("Executing: " + command);
      say(command);
    }
  }
  Println("after serial test");
  if (SerialAT.available() > 0)
    Serial.println(getResponse());
  Println("after AT check");
  Delay(100);

  wait(100);
  Println("loop end");
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
  // Delay(20000);            // wait for 20 seconds...
  // say("ATH"); // hang up
  // updateSerial();
}

void call(String number) {
  say("ATD+ " + number + ";");
  updateSerial();
}

void sendSMS(String sms) {
  if (modem.sendSMS(MOBILE_No, sms)) {
    println("$send{" + sms + "}");
  } else {
    println("SMS failed to send");
    println("\n!send{" + sms + "}!\n");
  }
  Delay(500);
}

void sendSMS(String sms, String number) {
  if (modem.sendSMS(number, sms)) {
    println("sending : [" + sms + "] to : " + String(number));
  } else {
    println("SMS failed to send");
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
  Println("entering getResponse");
  Delay(100);
  String response = "";
  if ((SerialAT.available() > 0)) {
    response += SerialAT.readString();
  }
  Println("After while loop in get response");
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
        sendSMS("<Unable to execute sms no. {" + String(newMessageNumber) +
                "} message : > [ " +
                temp_str.substring(0, temp_str.indexOf(" <not executed>")) +
                " ] from : " + senderNumber);
      }
    }
  } else if (response.indexOf("+CLIP:") != -1) {
    //+CLIP: "03354888420",161,"",0,"",0
    String temp_str = "Missed call from : " + fetchDetails(response, "\"", 1);
    sendSMS(temp_str);
    // println(temp_str);
  }
  Println("Leaving response function with response [" + response + "]");
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
    sendSMS("Battery percentage : " + String(batteryPercentage) +
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
    if (!checkStack(currentTargetIndex)) {
      sendSMS("Unable to execute sms no. {" + String(currentTargetIndex) +
              "} message : [ " +
              temp_str.substring(0, temp_str.indexOf(" <not executed>")) +
              " ] from : " + getMobileNumberOfMsg(String(currentTargetIndex)) +
              ", what to do ?");
      Delay(2000);
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
    return;
  }
  WiFi.begin(ssid, password); // Connect to Wi-Fi
  for (int i = 0; !wifi_connected(); i++) {
    if (i > 10) {
      println("Timeout: Unable to connect to WiFi");
      break;
    }
    Delay(500);
  }
  if (wifi_connected()) {
    println("Wi-Fi connected successfully");
  } else {
  }
}

bool wifi_connected() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  } else {
    return false;
  }
}

String getVariablesValues() {
  return String((String(wifiWorking ? "wifi on" : "wifi off")) +
                " Termination time : " + String(terminationTime));
}

void updateVariablesValues(String str) {
  int newValues = findOccurrences(str, "<");
  if (newValues == 0) {
    println("No new value to be update");
    println("response recived to work on : [" + str + "]");
    return;
  } else {
    println("Updating " + String(newValues) + " values");
    if (str.indexOf("wifi") != -1) {
      String forWifi = str.substring(str.indexOf("<wifi connectivity") + 19,
                                     str.indexOf("<wifi connectivity") + 21);
      if (forWifi.indexOf("0") != -1) {
        wifiWorking = false;
      } else if (forWifi.indexOf("1") != -1) {
        wifiWorking = true;
      }
    }
    if (str.indexOf("termination") != -1) {
      String forTerminator =
          str.substring(str.indexOf("<termination time") + 18,
                        str.indexOf("<termination time") + 20);
      print("\nTermination time updated: " + String(terminationTime) + " -> ");
      terminationTime = forTerminator.toInt() * 60;
      println(String(terminationTime));
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
      println("before termination");
      terminateLastMessage();
      println("after termination");
      condition = true;
    }
    if ((millis() / 1000) > 130 && DEBUGGING) {
      Println("Disabling DEBUGGING...");
      DEBUGGING = false;
    }
    if (((millis() / 1000) > 100) && (millis() / 1000) < 103) {
      // it will just run at first time when system booted
      Println("\nBefore updating values from message 1\n");
      updateVariablesValues(readMessage(1));
      Println("\nValues are updated from message 1\n");
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
          " Seconds : " + (RTC.seconds) + " Day : " + String(RTC.date) +
          " Month : " + (RTC.month));
  println("--------------------------------");
}

void Delay(int milliseconds) {
  delay(milliseconds);
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
  // str is the string to fetch data using begin and end character or string and
  // padding remove the data around the required data if padding is 1 then it
  // will only remove the begin and end string/character
  String beginOfTarget = str.substring(str.indexOf(begin) + 1, -1);
  Println("beginOfTarget : " + beginOfTarget);
  return beginOfTarget.substring(padding - 1,
                                 beginOfTarget.indexOf(end) - (padding - 1));
}
