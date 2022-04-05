/*
    Video: https://www.youtube.com/watch?v=oCMOYS71NIU
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updated by chegewara

   Create a BLE server that, once we receive a connection, will send periodic notifications.
   The service advertises itself as: 4fafc201-1fb5-459e-8fcc-c5c9c331914b
   And has a characteristic of: beb5483e-36e1-4688-b7f5-ea07361b26a8

   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create a BLE Service
   3. Create a BLE Characteristic on the Service
   4. Create a BLE Descriptor on the characteristic
   5. Start the service.
   6. Start advertising.

   A connect hander associated with the server starts a background task that performs notification
   every couple of seconds.
*/
#include <TimeLib.h>
#define Threshold 40 
touch_pad_t touchPin;

bool isOn = false;

volatile int interruptCounter;
int totalInterruptCounter;
int minutes;

int timeActive;

hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
 
void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  interruptCounter++;
  portEXIT_CRITICAL_ISR(&timerMux);
}

void callback(){
  //placeholder callback function
}

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Default set up
#define startTime // need to make this a continuous loop
//#undef  startTime
#define bleServerName "Strings attached"

// variables needed

String translate = "";
double dateValue;
float startTimeValue = 2;
float endTimeValue = 3;

bool deviceConnected = false;
bool oldDeviceConnected = false;

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b" // Currently using
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8" // still in use but need to switch

#define dateCharacteristicUUID  "c84b5feb-a12f-45bb-a3ff-a6adce24f69e"
#define startTimeCharacteristicUUID "2eec8c1a-4bc6-4c3d-ae54-4c498640b4f2"
#define endTimeCharacteristicUUID "b50dd1fb-fec7-46b0-8b3f-c34265cd01f1"

// Defining start/endTime Characteristiscs and Descriptors
#ifdef startTime
  BLECharacteristic startTimeCharacteristics("2eec8c1a-4bc6-4c3d-ae54-4c498640b4f2",BLECharacteristic::PROPERTY_NOTIFY);
  BLEDescriptor startTimeDescriptor(BLEUUID((uint16_t)0x2902));
#else
  BLECharacteristic endTimeCharacteristics("b50dd1fb-fec7-46b0-8b3f-c34265cd01f1",BLECharacteristic::PROPERTY_NOTIFY);
  BLEDescriptor endTimeDescriptor(BLEUUID((uint16_t)0x2902));
#endif

// Defining date Characteristic and Descriptor
BLECharacteristic dateCharacteristics("c84b5feb-a12f-45bb-a3ff-a6adce24f69e",BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor dateDescriptor(BLEUUID((uint16_t)0x2902));




// uint32_t value = 48; // 48 == ascii ZERO ;; 57 == ascii 9 ;; 53 == ascii 5

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};



void setup() {
  Serial.begin(115200);
  Serial.println("BLE Notify (Server) Program");

  setTime(1,0,0,10,2,2022);

  // Create BLE server name
  BLEDevice::init(bleServerName);
  // Creating server with callbacks
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  // Create server with service UUID and ch
  BLEService *stringService = pServer->createService(SERVICE_UUID);


  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000000, true);
  timerAlarmEnable(timer);

  touchAttachInterrupt(T3, callback, Threshold);
  esp_sleep_enable_touchpad_wakeup();

  

  // Start/End 
  #ifdef startTime
    stringService->addCharacteristic(&startTimeCharacteristics);
    startTimeDescriptor.setValue("Start Time: ");
    startTimeCharacteristics.addDescriptor(&startTimeDescriptor);
  #else
    stringService->addCharacteristic(&endTimeCharacteristics);
    endTimeDescriptor.setValue("End Time: ");
    endTimeCharacteristics.addDescriptor(&endTimeDescriptor);
  #endif 

  // Date
  stringService->addCharacteristic(&dateCharacteristics);
  dateDescriptor.setValue("Date: ");
  dateCharacteristics.addDescriptor(new BLE2902());
  
//  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
//                      CHARACTERISTIC_UUID,
//                      BLECharacteristic::PROPERTY_NOTIFY |
//                      BLECharacteristic::PROPERTY_INDICATE
//                    );


  
  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
//  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  stringService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x06);  // set value to 0x00 to not advertise this parameter
  pAdvertising->setMinPreferred(0x12);
  
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
    // notify changed value
    
    if (deviceConnected) {
       
//      #ifdef startTime
//        static char startTimeActive[6];
//        dtostrf(startTimeValue, 6, 0, startTimeActive);
//        //Set temperature Characteristic value and notify connected client
//        startTimeCharacteristics.setValue("hellohellohellohellohellohello");
//        startTimeCharacteristics.notify();
//        Serial.print("Start Time: ");
//        Serial.print(startTimeValue);
//
//        startTimeValue++;
//        
//      #else
//        static char endTimeActive[6];
//        dtostrf(endTimeValue, 6, 0, endTimeActive);
//        //Set temperature Characteristic value and notify connected client
//        endTimeCharacteristics.setValue(endTimeActive);
//        endTimeCharacteristics.notify();
//        Serial.print("End Time: ");
//        Serial.print(endTimeValue);
//      #endif

      
      static char dateActive[20];
      String fMonth = "";
      String fDay = "";
      String fHr = "";
      String fMin = "";
      String fSec = "";
      
      if(month() < 10 ){
        fMonth = "0" + String(month());
      } else {
        fMonth = String(month());
      }
      if(day() < 10){
        fDay = "0" + String(day());
      } else{
        fDay = String(day());  
      }
      if(hour() < 10){
        fHr = "0" + String(hour());
      } else{
        fHr = String(hour());  
      }
      if(minute() < 10){
        fMin = "0" + String(minute());
      } else{
        fMin = String(minute());  
      }
      if(second() < 10){
        fSec = "0" + String(second());
      } else{
        fSec = String(second());  
      }
      
      translate = String(year()) + fMonth + fDay + fHr + fMin + fSec;
      dateValue = translate.toDouble();
      dtostrf(dateValue, 20, 0, dateActive);
      // ntofiy

      dateCharacteristics.setValue(dateActive);
      dateCharacteristics.notify();   
      Serial.println(String(year()) + " " + fMonth + " " + fDay + " " + fHr + " " + fMin + " " + fSec);
      Serial.println(dateActive);
      
      // in liue of touch pad sending data
      delay(5000);
        
    }
}
