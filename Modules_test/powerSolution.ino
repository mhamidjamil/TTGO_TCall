#define WAPDA_STATE 15 // Check if wapda is on or off
// Magenta  R1:
#define BATTERY_PAIR_SELECTOR 32 // Select which battery pair is to be charge
// Blue R2-A:
#define BATTERY_CHARGER 14 // Battery should charge or not
// Green R2-B:
#define POWER_OUTPUT_SELECTOR 0 // Router input source selector

// int batteryChargeTime = 30;   // 30 minutes
// unsigned int wapdaInTime = 0; // when wapda is on it store the time stamp.
// bool batteriesCharged = false;

String chargingStatus = "";

void setup() {
  SerialMon.begin(115200);
  pinMode(WAPDA_STATE, INPUT);            // Check if wapda is on or off
  pinMode(BATTERY_PAIR_SELECTOR, OUTPUT); // Pair selector
  pinMode(BATTERY_CHARGER, OUTPUT);       // Charge battery or not
  pinMode(POWER_OUTPUT_SELECTOR, OUTPUT); // router INPUT selector

  digitalWrite(BATTERY_PAIR_SELECTOR, HIGH);
  digitalWrite(BATTERY_CHARGER, HIGH);
  digitalWrite(POWER_OUTPUT_SELECTOR, HIGH);
}

void loop() { Delay(100); }

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