


#include <ESP32Time.h>

// Uisng pins 0,14,& 13 for the touch sensors
// Using pin 15 for the microphone
// Unknown for LED's on visual indicators

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

//---------------------------------------------------------------------------------------------------------------------------------

// variables

#define SERVICE_UUID        "0d687f7e-9056-4a76-a8a7-dff88255d369"
#define CHARACTERISTIC_UUID "49c1198e-914a-4dcb-84e0-4a203e921a7c"
#define x_sensor_pin 26   //D2
#define nx_sensor_pin 25 //D3
#define y_sensor_pin 34  //A3
#define ny_sensor_pin 35 //A2
#define z_sensor_pin 0   //D5
#define nz_sensor_pin 39  //A1

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;

ESP32Time rtc;

uint32_t timePlayed = 0;
unsigned long long BUTTON_PIN_BITMASK = 0x2000; //2^0 + 2^13 + 2^14 in Hex
int sensor = 0;
int x_tilt = 0;
int y_tilt = 0;
int z_tilt = 0;
int nx_tilt = 0;
int ny_tilt = 0;
int nz_tilt = 0;
int x_lastState = 0;
int y_lastState = 0;
int z_lastState = 0;
int nx_lastState = 0;
int ny_lastState = 0;
int nz_lastState = 0;
int BatteryLvl = 0;
int voltage = 0;
float SampledVoltage = 0;
float lastVoltage = 0;
unsigned long SumVoltage = 0;
int voltageSample = 0;

RTC_DATA_ATTR bool first_use = true;
bool deviceConnected = false;
bool oldDeviceConnected = false;
bool AllSensorsChecked = 0;
bool SensorChanged = 0;
bool redLightOn = 0;
bool yellowLightOn = 0;
bool greenLightOn = 0;
bool LowBat = 0;
bool isCharging = 0;


String Truetime = "";

//---------------------------------------------------------------------------------------------------------------------------------
// Functions

void voltageRead()
{
  voltage = analogRead(36);
  SumVoltage = SumVoltage + voltage;
  voltageSample++;
  if (voltageSample == 24){
    lastVoltage = SampledVoltage;
    SampledVoltage = SumVoltage/28205.1;
    SumVoltage = 0;
    voltageSample = 0;
  }
}


  // call back for device conectivity check
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      if  (first_use) {
        first_use = false;
      }
      deviceConnected = true;
      rtc.setTime(0, 0, 4, 5, 1, 2022);
    }

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

  // time interupt

volatile int Interupt_Counter = 0;
hw_timer_t* DeepSleepTimer = NULL;
portMUX_TYPE DeepSleepTimerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onDeepSleepTimer() {
  portENTER_CRITICAL_ISR(&DeepSleepTimerMux);
  Interupt_Counter++;

  if (Interupt_Counter == 20) {
      esp_deep_sleep_start();
  }
    portEXIT_CRITICAL_ISR(&DeepSleepTimerMux);
}

void SensorCheck() {
  AllSensorsChecked = 0;
  SensorChanged = 0;
  do {
    if (x_tilt != x_lastState){
      x_lastState = x_tilt;
      SensorChanged = 1;
    }else if(y_tilt > SampledVoltage*.75 && y_lastState < SampledVoltage*.35 || y_tilt < SampledVoltage*.35 && y_lastState > SampledVoltage*.75){
      y_lastState = y_tilt;
      SensorChanged = 1;
    }else if(z_tilt != z_lastState){
      z_lastState = z_tilt;
      SensorChanged = 1;
    }else if(nx_tilt != nx_lastState){
      nx_lastState = nx_tilt;
      SensorChanged = 1;
    }else if(ny_tilt > SampledVoltage*.75 && ny_lastState < SampledVoltage*.35 || ny_tilt < SampledVoltage*.35 && ny_lastState > SampledVoltage*.75){
      ny_lastState = ny_tilt;
      SensorChanged = 1;
    }else if(nz_tilt > SampledVoltage*.75 && nz_lastState < SampledVoltage*.35 || nz_tilt < SampledVoltage*.35 && nz_lastState > SampledVoltage*.75){
      nz_lastState = nz_tilt;
      SensorChanged = 1;
    }else {
      AllSensorsChecked = 1;
    }
  }while (AllSensorsChecked == 0);

  if(SensorChanged){
    WakeupChange();
  }
}

void WakeupChange() { // initlize all wakeup sensors
  BUTTON_PIN_BITMASK = 0x0;
  if (x_tilt == 0) {
    BUTTON_PIN_BITMASK += pow(2,x_sensor_pin);
  }
  if (y_tilt <= SampledVoltage*.35) {
    BUTTON_PIN_BITMASK += pow(2,y_sensor_pin);
  }
  if (z_tilt == 0) {
    BUTTON_PIN_BITMASK += pow(2,z_sensor_pin);
  }
  if (nx_tilt == 0) {
    BUTTON_PIN_BITMASK += pow(2,nx_sensor_pin);
  }
  if (ny_tilt <= SampledVoltage*.35) {
    BUTTON_PIN_BITMASK += pow(2,ny_sensor_pin);
  }
  if (nz_tilt <= SampledVoltage*.35) {
    BUTTON_PIN_BITMASK += pow(2,nz_sensor_pin);
  }
  if (BUTTON_PIN_BITMASK == 0x0){
    BUTTON_PIN_BITMASK += pow(2,nz_sensor_pin);
  }
  esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK,ESP_EXT1_WAKEUP_ANY_HIGH);
}
//---------------------------------------------------------------------------------------------------------------------------------
  
void setup() {
  Serial.begin(115200);
  
  // timer setup
  DeepSleepTimer = timerBegin(0,80,true);
  timerAttachInterrupt(DeepSleepTimer, &onDeepSleepTimer, true);
  timerAlarmWrite(DeepSleepTimer,1000000,true);
  timerAlarmEnable(DeepSleepTimer);
  
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

    pinMode(25,INPUT);
    pinMode(26,INPUT);
    pinMode(0,INPUT);
    pinMode(35,INPUT);
    pinMode(34,INPUT);
    pinMode(39,INPUT);
    pinMode(36,INPUT);
    pinMode(2, OUTPUT);
    pinMode(13, OUTPUT);
    pinMode(14, OUTPUT);
    
    x_tilt = digitalRead(x_sensor_pin); // 0x00200000
    y_tilt = analogRead(y_sensor_pin); // 0x00400000
    z_tilt = digitalRead(z_sensor_pin); // 0x0001
    nx_tilt = digitalRead(nx_sensor_pin); // 0x4000
    ny_tilt = analogRead(ny_sensor_pin); // 0x2000
    nz_tilt = analogRead(nz_sensor_pin); // 0x0004  


    
    x_lastState = x_tilt;
    y_lastState = y_tilt;
    nx_lastState = nx_tilt;
    z_lastState = z_tilt;
    ny_lastState = ny_tilt;
    nz_lastState = nz_tilt;

    WakeupChange();
}

//---------------------------------------------------------------------------------------------------------------------------------

void loop() {


    //Truetime = rtc.getTime();


    x_tilt = digitalRead(x_sensor_pin); // 0x00200000
    y_tilt = analogRead(y_sensor_pin); // 0x00400000
    z_tilt = digitalRead(z_sensor_pin); // 0x0001
    nx_tilt = digitalRead(nx_sensor_pin); // 0x4000
    ny_tilt = analogRead(ny_sensor_pin); // 0x2000
    nz_tilt = analogRead(nz_sensor_pin); // 0x0004   
    SensorCheck();
    voltageRead();

    Serial.print(BUTTON_PIN_BITMASK);
    Serial.print("\t");
    Serial.print(x_tilt);
    Serial.print("\t");
    Serial.print(nx_tilt);
    Serial.print("\t");
    Serial.print(y_tilt);
    Serial.print("\t");
    Serial.print(ny_tilt);
    Serial.print("\t");
    Serial.print(z_tilt);
    Serial.print("\t");
    Serial.println(nz_tilt);
// 


    /*
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
    } */

}
