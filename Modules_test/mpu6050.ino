#define TARGETED_NUMBER "+923354888420"

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

//$ ======================================================
#include <MPU6050_tockn.h>
#include <Wire.h>


MPU6050 mpu6050(Wire);

void setup() {
  SerialMon.begin(115200);

  // Keep power when running from battery
  Wire.begin(I2C_SDA, I2C_SCL);
  bool isOk = setPowerBoostKeepOn(1);
  SerialMon.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));

  // Set modem reset, enable, power pins
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  // Set GSM module baud rate and UART pins
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);
  SerialMon.println("Initializing modem...");
  modem.restart();
  //$============================================

  while (!mpu6050.begin(MPU6050_SCALE_2000DPS, MPU6050_RANGE_2G)) {
    Serial.println("Could not find a valid MPU6050 sensor, check wiring!");
    delay(500);
  }

  mpu6050.calibrateGyro();         // Calibrate gyro
  mpu6050.setThreshold(3);         // Set the threshold for detecting motion
  mpu6050.setGyroOffsets(0, 0, 0); // Set gyro offsets to zero

  Serial.println("MPU6050 is ready!");
}

// Calibrate MPU-6050
mpu.calibrateGyro();
mpu.setThreshold(3);

Serial.println("MPU6050 is ready!");
}

void loop() {
  if (SerialMon.available()) {
    char c = SerialMon.read();
    SerialAT.write(c);
  }

  if (SerialAT.available()) {
    char c = SerialAT.read();
    SerialMon.write(c);
  }
  mpu6050.update();

  float gyroX = mpu6050.getAngleX();
  float gyroY = mpu6050.getAngleY();
  float gyroZ = mpu6050.getAngleZ();

  Serial.print("Gyro (degrees/sec): ");
  Serial.print("X = ");
  Serial.print(gyroX);
  Serial.print(" | Y = ");
  Serial.print(gyroY);
  Serial.print(" | Z = ");
  Serial.println(gyroZ);

  delay(500); // Adjust the delay as needed
}
