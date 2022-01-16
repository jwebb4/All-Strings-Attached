// Uisng pins 0,14,& 13 for the touch sensors
// Using pin 15 for the microphone
// Unknown for LED's on visual indicators

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Time.h>

//---------------------------------------------------------------------------------------------------------------------------------

// variables

#define SERVICE_UUID        "0d687f7e-9056-4a76-a8a7-dff88255d369"
#define CHARACTERISTIC_UUID "49c1198e-914a-4dcb-84e0-4a203e921a7c"
#define x_sensor_pin 0
#define y_sensor_pin 13
#define z_sensor_pin 14
#define nx_sensor_pin 0
#define ny_sensor_pin 0
#define nz_sensor_pin 0

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;

uint32_t timePlayed = 0;
uint32_t BUTTON_PIN_BITMASK = 0x6001; //2^0 + 2^13 + 2^14 in Hex
uint8_t sensor = 0;
uint8_t x_tilt = 0;
uint8_t y_tilt = 0;
uint8_t z_tilt = 0;
uint8_t nx_tilt = 0;
uint8_t ny_tilt = 0;
uint8_t nz_tilt = 0;
int x_lastState = 0;
int y_lastState = 0;
int z_lastState = 0;
int nx_lastState = 0;
int ny_lastState = 0;
int nz_lastState = 0;
volatile int Interupt_Counter = 0;

RTC_DATA_ATTR bool first_use = true;
bool deviceConnected = false;
bool oldDeviceConnected = false;
bool AllSensorsChecked = 0;
bool SensorChanged = 0;

String Truetime;
//---------------------------------------------------------------------------------------------------------------------------------
// Functions
  
  // call back for device conectivity check
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      if  (first_use) {
        first_use = false;
      }
      deviceConnected = true;
      setTime(0, 0, 4, 5, 1, 2021);
    }

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

  // time interupt

  hw_timer_t* timer = NULL;

  void IRAM_ATTR onTimer() {
    Interupt_Counter++;
    Serial.println(String(Interupt_Counter));
    if (Interupt_Counter >= 300) {
        Serial.println("start deep sleep");
        esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK,ESP_EXT1_WAKEUP_ANY_HIGH);
        esp_deep_sleep_start();
    }
  }

  void SensorCheck() {
    AllSensorsChecked = 0;
    SensorChanged = 0;
    do {
      if (x_tilt != x_lastState){
        x_tilt = !x_tilt;
        SensorChanged = 1;
      }else if(y_tilt != y_lastState){
        y_tilt = !y_tilt;
        SensorChanged = 1;
      }else if(z_tilt != z_lastState){
        z_tilt = !y_tilt;
        SensorChanged = 1;
      }else if(nx_tilt != nx_lastState){
        nx_tilt = !nx_tilt;
        SensorChanged = 1;
      }else if(ny_tilt != ny_lastState){
        ny_tilt = !ny_tilt;
        SensorChanged = 1;
      }else if(nz_tilt != nz_lastState){
        nz_tilt = !nz_tilt;
        SensorChanged = 1;
      }else {
        AllSensorsChecked = 1;
      }
    }while (AllSensorsChecked == 1);
  
    x_lastState = x_tilt;
    y_lastState = y_tilt;
    z_lastState = z_tilt;
    nx_lastState = nx_tilt;
    ny_lastState = ny_tilt;
    nz_lastState = nz_tilt;

    if(SensorChanged){
      WakeupChange();
    }
  }

  void WakeupChange() { // initlize all wakeup sensors
    BUTTON_PIN_BITMASK = 0x0;
    if (!x_lastState) {
      BUTTON_PIN_BITMASK += pow(2,x_sensor_pin);
    }
    if (!y_lastState) {
      BUTTON_PIN_BITMASK += pow(2,y_sensor_pin);
    }
    if (!z_lastState) {
      BUTTON_PIN_BITMASK += pow(2,z_sensor_pin);
    }
    if (!nx_lastState) {
      BUTTON_PIN_BITMASK += pow(2,nx_sensor_pin);
    }
    if (!ny_lastState) {
      BUTTON_PIN_BITMASK += pow(2,ny_sensor_pin);
    }
    if (!nz_lastState) {
      BUTTON_PIN_BITMASK += pow(2,nz_sensor_pin);
    }
    esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK,ESP_EXT1_WAKEUP_ANY_HIGH);
  }
//---------------------------------------------------------------------------------------------------------------------------------
  
void setup() {
  Serial.begin(115200);
  Truetime = getDate(true);
  Serial.println(Truetime);
  // timer setup
  timer = timerBegin(0,80,true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer,1000000,true);
  
  // Create the BLE Device
  BLEDevice::init("Strings attached");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  // Create a BLE Descriptor
  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();

  //initialize sensors
  x_tilt = !digitalRead(x_sensor_pin); // 0x1
  y_tilt = digitalRead(y_sensor_pin); // 0x2000
  z_tilt = digitalRead(z_sensor_pin); // 0x4000
  nx_tilt = digitalRead(nx_sensor_pin); // 0x1
  ny_tilt = digitalRead(ny_sensor_pin); // 0x2000
  nz_tilt = digitalRead(nz_sensor_pin); // 0x4000  
  
  x_lastState = x_tilt;
  y_lastState = y_tilt;
  z_lastState = z_tilt;
  nx_lastState = nx_tilt;
  ny_lastState = ny_tilt;
  nz_lastState = nz_tilt;
  
}

//---------------------------------------------------------------------------------------------------------------------------------

void loop() {
    x_tilt = !digitalRead(x_sensor_pin); // 0x1
    y_tilt = digitalRead(y_sensor_pin); // 0x2000
    z_tilt = digitalRead(z_sensor_pin); // 0x4000
    nx_tilt = digitalRead(nx_sensor_pin); // 0x1
    ny_tilt = digitalRead(ny_sensor_pin); // 0x2000
    nz_tilt = digitalRead(nz_sensor_pin); // 0x4000  
    
    if (deviceConnected) {
      if (oldDeviceConnected) {
        timerAlarmEnable(timer);
        if (!sensor) {
          timerAlarmDisable(timer);
          Interupt_Counter = 0;
          Serial.println("deep sleep reset");
          timerAlarmEnable(timer);
        }
    
        pCharacteristic->setValue((uint8_t*)&timePlayed, 4);
        timePlayed++;
        delay(1000); 
        
        if (timePlayed % 30 == 1) {
          pCharacteristic->notify();
        }

        SensorCheck();
        
      } else { // connecting
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
        pCharacteristic->notify();
      }
    }
    if (!deviceConnected) {
      // disconnecting
      if (oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
      } else {
         
      }
    }

}
