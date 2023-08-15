//$ 4/JUNE/2023 3:25 PM
//% code to perform specific actions on BLE input

//* code can now receive and send data to BLE device

// #.....................................................
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

void inputManager(String input);
class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
      receivedData = value;
      Serial.print("Received from BLE: ");
      Serial.println(receivedData.c_str());
      inputManager(String(receivedData.c_str()));

      // Print the received data back to the BLE connection
      pCharacteristic->setValue("BLE: " + receivedData);
      pCharacteristic->notify();
    }
  }
};
void setup() {
  Serial.begin(115200);

  BLEDevice::init("TTGO T-Call BLE");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(
      BLEUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b")); // Custom service UUID
  pCharacteristic = pService->createCharacteristic(
      BLEUUID(
          "beb5483e-36e1-4688-b7f5-ea07361b26a8"), // Custom characteristic UUID
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

void loop() {
  if (deviceConnected) {
    if (!oldDeviceConnected) {
      Serial.println("Connected to device");
      oldDeviceConnected = true;
    }
  } else {
    if (oldDeviceConnected) {
      Serial.println("Disconnected from device");
      oldDeviceConnected = false;
    }
  }

  delay(100);
}
void inputManager(String input) {
  if (input == "test") {
    Serial.println("defined word: test");
  } else if (input.indexOf("#run") != -1) {
    Serial.println("HID function will be call here");
  } else {
    Serial.println("undefined word : " + input);
  }
}