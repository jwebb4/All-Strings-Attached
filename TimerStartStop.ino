/* Setup BLE library */
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

bool isReset = false;
bool isPrint = false;
bool isPause = false;
bool isResume = true;


class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();

      if (value.length() > 0) {
        Serial.println("*********");
        Serial.print("New value: ");
        for (int i = 0; i < value.length(); i++)
          Serial.print(value[i]);
        Serial.println();
        Serial.println("*********");
      }

      if(value == "r"){
        isReset = true;
        } 
      else {
        isReset = false;
        }
      if(value == "p"){
        isPrint = true;
        } 
      else {
        isPrint = false;
        }
      if(value == "resume"){
        isResume = true;
        isPause = false;
        } 
      if(value == "pause") {
        isResume = false;
        isPause = true;
        }

    }
};


/*Setup for timer*/
volatile int interruptCounter;
int totalInterruptCounter;
int hours;
int minutes;

hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
 
void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  interruptCounter++;
  portEXIT_CRITICAL_ISR(&timerMux);
 
}
 
void setup() {
 
  Serial.begin(9600);
 
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000000, true);
  timerAlarmEnable(timer);

  BLEDevice::init("le micro :)");
  BLEServer *pServer = BLEDevice::createServer();

  BLEService *pService = pServer->createService(SERVICE_UUID);

  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  pCharacteristic->setCallbacks(new MyCallbacks());

  pCharacteristic->setValue("Hello World");
  pService->start();

  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->start();
 
}
 
void loop() {
 
  if (interruptCounter > 0) {
 
    portENTER_CRITICAL(&timerMux);
    interruptCounter--;
    portEXIT_CRITICAL(&timerMux);
 
    totalInterruptCounter++;
    if(totalInterruptCounter > 59){
      minutes++;
      totalInterruptCounter = 0;
      if(minutes > 59){
        hours++;
        minutes = 0;
        }
      }

    if(isReset == true){
      hours = 0;
      minutes = 0;
      totalInterruptCounter = 0;
      Serial.println("Reset has occured");
      isReset = false;
      }
    
    if(isPrint == true){
      if(hours < 10){
        Serial.print("0");
        }
      Serial.print(hours);
      Serial.print(":");
      if(minutes < 10){
        Serial.print("0");
        }
      Serial.print(minutes);
      Serial.print(":");
      if(totalInterruptCounter < 10){
        Serial.print("0");
        }
      Serial.println(totalInterruptCounter);
      isPrint = false;
      }


  }
}
