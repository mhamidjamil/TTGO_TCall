//$ last work 26/July/23 [12:15 AM]
// # version 4.5.4

//`===================================
#include <DHT.h>
#include <ThingSpeak.h>
#include <WiFi.h>
#include <Wire.h>
#include <random>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Initialize the DHT11 sensor
#define DHTPIN 33 // Change the pin if necessary
DHT dht(DHTPIN, DHT11);

// ThingSpeak parameters
const char *ssid = "Archer 73";
const char *password = "Archer@73_102#";
const unsigned long channelID = 2201589;
const char *apiKey = "M6GKK40AB3Q7YE42";

unsigned long updateInterval = 2 * 60;
unsigned long previousUpdateTime = 0;
unsigned int last_update = 0; // in seconds
WiFiClient client;

String END_VALUES = "  ";
String line_1 = "Temp: 00.0 C";
String line_2 = "                ";

void connect_wifi();
void lcd_print();
const int LED = 13;
//`===================================

const char simPIN[] = "";

String MOBILE_No = "+923354888420";

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800   // Modem is SIM800
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb

#include <TinyGsmClient.h>
#include <Wire.h>

// TTGO T-Call pins
#define MODEM_RST 5
#define MODEM_PWKEY 4
#define MODEM_POWER_ON 23
#define MODEM_TX 27
#define MODEM_RX 26
#define I2C_SDA 21
#define I2C_SCL 22
#define DISPLAY_POWER_PIN 32
#define ALERT_PIN 12

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

//`...............................
String receivedMessage = ""; // Global variable to store received SMS message

String updateBatteryStatus();
double batteryVoltage = 0;
int batteryPercentage = 0;
// another variable to store time in millis
unsigned long time_ = 0;
int messages_in_inbox = 0;
//`...............................
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
  //`...............................
  pinMode(DISPLAY_POWER_PIN, OUTPUT);
  digitalWrite(DISPLAY_POWER_PIN, HIGH);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  delay(2000);

  pinMode(DISPLAY_POWER_PIN, OUTPUT);
  digitalWrite(DISPLAY_POWER_PIN, HIGH);
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  // Display static text
  display.println("Hello, world!" + String(random(99)));
  display.display();

  dht.begin(); // Initialize the DHT11 sensor
  connect_wifi();
  pinMode(LED, OUTPUT);
  ThingSpeak.begin(client); // Initialize ThingSpeak
  ThingSpeak.setField(4, random(10, 31));
  END_VALUES.setCharAt(1, '#');
  messages_in_inbox = totalUnreadMessages();
  //`...............................
}

void loop() {
  if (SerialMon.available()) {
    String command = SerialMon.readString();
    if (command.indexOf("call") != -1) {
      println("Calling " + String(MOBILE_No));
      giveMissedCall();
    } else if (command.indexOf("sms") != -1) {
      // fetch sms from input string, sample-> sms : msg here
      String sms = command.substring(command.indexOf("sms") + 6);
      println("Sending SMS : " + sms + " to : " + String(MOBILE_No));
      sendSMS(sms);
    } else if (command.indexOf("all") != -1) {
      println("Reading all messages");
      moduleManager();
    } else if (command.indexOf("battery") != -1) {
      println(updateBatteryStatus());
    } else if (command.indexOf("t1") != -1) {
      Oled(0);
    } else if (command.indexOf("read") != -1) {
      // say i recived read 2, so it will read message of index 2
      println(
          readMessage(command.substring(command.indexOf("read") + 5).toInt()));
    } else if (command.indexOf("delete") != -1) { // to delete message
      println("Deleting message number : " +
              String(command.substring(command.indexOf("delete") + 7)));
      deleteMessage(command.substring(command.indexOf("delete") + 7).toInt());
    } else {
      println("Executing: " + command);
      say(command);
    }
  }
  if (SerialAT.available())
    println(getResponse());
  if (millis() - time_ > 10000) {
    time_ = millis();
    // get all messages index number and execute them
  }
  //`..................................
  float temperature = dht.readTemperature();
  int humidity = dht.readHumidity();

  char temperatureStr[5];
  dtostrf(temperature, 4, 1, temperatureStr);

  line_1 =
      line_1.substring(0, 6) + String(temperatureStr) + " C  " + END_VALUES;

  line_2 = "Hu: " + String(humidity) + " % /" + get_time();
  lcd_print();
  if (((millis() / 1000) - previousUpdateTime) >= updateInterval) {
    previousUpdateTime = (millis() / 1000);
    updateThingSpeak(temperature, humidity);
    messages_in_inbox = totalUnreadMessages();
  }
  //`..................................
}
void println(String str) { SerialMon.println(str); }
void print(String str) { SerialMon.print(str); }
void say(String str) { SerialAT.println(str); }

void giveMissedCall() {
  say("ATD+ " + MOBILE_No + ";");
  updateSerial();
  // delay(20000);            // wait for 20 seconds...
  // say("ATH"); // hang up
  // updateSerial();
}

void sendSMS(String sms) {
  if (modem.sendSMS(MOBILE_No, sms)) {
    println(sms);
  } else {
    println("SMS failed to send");
  }
  delay(500);
}

void updateSerial() {
  delay(500);
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
  String response = "";
  unsigned int entrySec = millis() / 1000;
  int timeoutSec = 3;
  while (SerialAT.available() || (!((response.indexOf("OK") != -1) ||
                                    (response.indexOf("ERROR") != -1)))) {
    response += SerialAT.readString();
    if (timeOut(timeoutSec, entrySec) && !(SerialAT.available() > 0)) {
      println("******\tTimeout\t******");
      break;
    } else if (SerialAT.available()) {
      timeoutSec = 1;
    }
  }
  if (response.indexOf("+CMTI:") != -1) {
    int newMessageNumber = getNewMessageNumber(response);
    String temp_str = executeCommand(removeOk(readMessage(newMessageNumber)));
    println("New message [ " + temp_str + "]");
    if (temp_str.indexOf("<executed>") != -1)
      deleteMessage(newMessageNumber);
    else {
      sendSMS("<Unable to execute sms no. {" + String(newMessageNumber) +
              "} message : > [ " +
              temp_str.substring(0, temp_str.indexOf(" <not executed>")) +
              " ] from : " + getNumberOfMessage(newMessageNumber));
      //! store in struct PM <pending work>.
    }
  } else if (response.indexOf("+CLIP:") != -1) {
    //+CLIP: "03354888420",161,"",0,"",0
    String temp_str = "Missed call from : " + fetchDetails(response, "\"", 1);
    sendSMS(temp_str);
    // println(temp_str);
  }
  return response;
}

int getNewMessageNumber(String response) {
  return response.substring(response.lastIndexOf(",") + 1, -1).toInt();
}

String readMessage(int index) { // read the message of given index
  say("AT+CMGR=" + String(index));
  String tempStr = getResponse();
  return tempStr.substring(tempStr.lastIndexOf("\"") + 2);
}

void deleteMessage(int index) {
  say("AT+CMGD=" + String(index));
  println(getResponse());
}

String removeOk(String str) {
  if (str.lastIndexOf("OK") != -1)
    return str.substring(0, str.lastIndexOf("OK") - 2);
  else
    return str;
}

String executeCommand(String str) {
  //~ additional commands will be executed here so define new sms commands here
  if (str.indexOf("#call") != -1) {
    giveMissedCall();
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
  } else if (str.indexOf("#display") != -1) {
    str += " <executed>";
    (str.indexOf("on") != -1 ? Oled(1) : Oled(0));
  } else if (str.indexOf("#on") != -1) {
    str += " <executed>";
    int switchNumber = str.substring(str.indexOf("#on") + 3).toInt();
    digitalWrite(switchNumber, HIGH);
  } else if (str.indexOf("#off") != -1) {
    str += " <executed>";
    int switchNumber = str.substring(str.indexOf("#off") + 4).toInt();
    digitalWrite(switchNumber, LOW);
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

bool timeOut(int sec, unsigned int entrySec) {
  if ((millis() / 1000) - entrySec > sec)
    return true;
  else
    return false;
}

void forwardMessage(int index) {
  String message = removeOk(readMessage(index));
  if (message.length() > 2)
    sendSMS(message);
  else
    sendSMS("No message found at index : " + String(index));
}

String getNumberOfMessage(int index) {
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
//`..................................

void updateThingSpeak(float temperature, int humidity) {
  ThingSpeak.setField(1, temperature); // Set temperature value
  ThingSpeak.setField(2, humidity);    // Set humidity value

  int updateStatus = ThingSpeak.writeFields(channelID, apiKey);
  if (updateStatus == 200) {
    println("ThingSpeak update successful");
    SUCCESS_MSG();
  } else {
    print("Error updating ThingSpeak. Status: ");
    println(String(updateStatus));
    ERROR_MSG();
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
  connect_wifi();
}

void connect_wifi() {
  WiFi.begin(ssid, password); // Connect to Wi-Fi
  int i = 0;
  while (!wifi_connected()) {
    if (i > 10) {
      println("Timeout: Unable to connect to WiFi");
      break;
    }
    delay(500);
    i++;
    END_VALUES.setCharAt(0, '?');
    lcd_print();
    delay(500);
    END_VALUES.setCharAt(0, ' ');
  }
  println("Wi-Fi connected successfully");
  if (wifi_connected()) {
    END_VALUES.setCharAt(0, '*');
    lcd_print();
  } else {
    END_VALUES.setCharAt(0, '!');
    lcd_print();
    digitalWrite(LED, LOW);
  }
}

bool wifi_connected() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  } else {
    return false;
  }
}

void lcd_print() {
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 0);
  // Display static text
  drawWifiSymbol(wifi_connected());
  display.print("   ");
  display.print(String(messages_in_inbox));

  display.setCursor(0, 20);
  display.print(line_1);

  display.setCursor(0, 40);
  display.print(line_2);

  display.display();
  delay(1000);
}

String get_time() {
  unsigned int sec = (millis() / 1000) - last_update;
  if (sec < 60) {
    return (String(sec) + " s");
  } else if ((sec >= 60) && (sec < 3600)) {
    return (String(sec / 60) + "m" + String(sec % 60) + "s");
  } else {
    println("Issue spotted sec value: " + String(sec));
    connect_wifi();
    return String(-1);
  }
  return "X";
}

void Oled(int status) {
  if (status == 1) {
    digitalWrite(DISPLAY_POWER_PIN, HIGH);
  } else if (status == 0) {
    digitalWrite(DISPLAY_POWER_PIN, LOW);
  }
}

const unsigned char wifiSymbol[] PROGMEM = {B00000000, B01111110, B10000001,
                                            B01111100, B10000010, B00111000,
                                            B01000100, B00010000};

const unsigned char questionMark[] PROGMEM = {B00111000, B01000100, B10000010,
                                              B00000100, B00001000, B00010000,
                                              B00010000, B0001000};

void drawWifiSymbol(bool isConnected) {

  if (!isConnected) {
    display.setCursor(0, 0);
    display.print("X");
  } else
    display.drawBitmap(0, 0, wifiSymbol, 8, 8, WHITE);
}
//`..................................