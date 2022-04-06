/*
 ESP32 startup counter example with Preferences library.
 This simple example demonstrates using the Preferences library to store how many times the ESP32 module has booted. 
 The Preferences library is a wrapper around the Non-volatile storage on ESP32 processor.
 created for arduino-esp32 09 Feb 2017 by Martin Sloup (Arcao)
 
 Complete project details at https://RandomNerdTutorials.com/esp32-save-data-permanently-preferences/
*/

// yyyymmddhhmmssF
#include <ESP32Time.h>
#include <TimeLib.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define Threshold 40
#define MAX_CHARS 15
#define x_sensor_pin 25  //D2
#define nx_sensor_pin 14 //D6
#define y_sensor_pin 26  //D3
#define ny_sensor_pin 13 //D7
#define z_sensor_pin 0   //D5
#define nz_sensor_pin 2  //D9
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b" // Currently using
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8" // still in use but need to switch
#define dateCharacteristicUUID  "c84b5feb-a12f-45bb-a3ff-a6adce24f69e"

Preferences preferences;
ESP32Time rtc;

BLECharacteristic dateCharacteristics("c84b5feb-a12f-45bb-a3ff-a6adce24f69e",BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor dateDescriptor(BLEUUID((uint16_t)0x2902));

String timeDifference = "timeDifference";
String baseNameDataSpace = "dtStorage";
String tempStartData = "";
String tempEndData = "";

volatile int Interupt_Counter = 0;
volatile int interruptCounter;
int totalInterruptCounter;
int count = 1;
int minutes;
int timeActive;
int x_lastState = 0;
int y_lastState = 0;
int z_lastState = 0;
int nx_lastState = 0;
int ny_lastState = 0;
int nz_lastState = 0;
uint32_t timePlayed = 0;
uint32_t BUTTON_PIN_BITMASK = 0x2000; //2^0 + 2^13 + 2^14 in Hex
uint8_t sensor = 0;
uint8_t x_tilt = 0;
uint8_t y_tilt = 0;
uint8_t z_tilt = 0;
uint8_t nx_tilt = 0;
uint8_t ny_tilt = 0;
uint8_t nz_tilt = 0;

String translate = "";
double dateValue;

RTC_DATA_ATTR bool first_use = true;

bool oldDeviceConnected = false;
bool AllSensorsChecked = 0;
bool SensorChanged = 0;
bool deviceConnected = false;
bool DetectMotionAndSound = 0;
bool isTouched = false;
bool isStorageFull = false; // most liekly set a limit of 5 start/end saves
bool isReadySendData = false;
bool isClearingData = false;

touch_pad_t touchPin;

hw_timer_t * timer = NULL;
hw_timer_t* SleepTimer = NULL;

portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};


void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  interruptCounter++;
  portEXIT_CRITICAL_ISR(&timerMux);
}

  

void IRAM_ATTR onDeepSleepTimer() {
  Interupt_Counter++;
  Serial.println(String(Interupt_Counter));
  if (Interupt_Counter >= 20) {
      Serial.println("start deep sleep");
      esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK,ESP_EXT1_WAKEUP_ANY_HIGH);
      esp_bt_controller_disable();
      esp_deep_sleep_start();
  }
}

void SensorCheck() {
  AllSensorsChecked = 0;
  SensorChanged = 0;
  do {
    if (x_tilt != x_lastState){
      x_lastState = x_tilt;
      SensorChanged = 1;
    }else if(y_tilt != y_lastState){
      y_lastState = y_tilt;
      SensorChanged = 1;
    }else if(z_tilt != z_lastState){
      z_lastState = z_tilt;
      SensorChanged = 1;
    }else if(nx_tilt != nx_lastState){
      nx_lastState = nx_tilt;
      SensorChanged = 1;
    }else if(ny_tilt != ny_lastState){
      ny_lastState = ny_tilt;
      SensorChanged = 1;
    }else if(nz_tilt != nz_lastState){
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
  if (BUTTON_PIN_BITMASK == 0x0){
    BUTTON_PIN_BITMASK += pow(2,nz_sensor_pin);
  }
  esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK,ESP_EXT1_WAKEUP_ANY_HIGH);
}

void callback(){
}

#define startTime // need to make this a continuous loop
#define bleServerName "Strings attached"

String GetCurrentTimeStamp(){
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
  return String(year()) + fMonth + fDay + fHr + fMin + fSec;
}

void CalculateTimeElapsed(String tStart, String tEnd){
  // Getting variables
  String sYearStr = String(tStart.charAt(0)) + String(tStart.charAt(1)) + String(tStart.charAt(2)) + String(tStart.charAt(3));
  String sMonthStr = String(tStart.charAt(4)) + String(tStart.charAt(5));
  String sDayStr = String(tStart.charAt(6)) + String(tStart.charAt(7));
  String sHrStr = String(tStart.charAt(8)) + String(tStart.charAt(9));
  String sMinStr = String(tStart.charAt(10)) + String(tStart.charAt(11));
  String sSecStr = String(tStart.charAt(12)) + String(tStart.charAt(13));
  
  String eYearStr = String(tEnd.charAt(0)) + String(tEnd.charAt(1)) + String(tEnd.charAt(2)) + String(tEnd.charAt(3));
  String eMonthStr = String(tEnd.charAt(4)) + String(tEnd.charAt(5));
  String eDayStr = String(tEnd.charAt(6)) + String(tEnd.charAt(7));
  String eHrStr = String(tEnd.charAt(8)) + String(tEnd.charAt(9));
  String eMinStr = String(tEnd.charAt(10)) + String(tEnd.charAt(11));
  String eSecStr = String(tEnd.charAt(12)) + String(tEnd.charAt(13));

  int sYear = sYearStr.toInt();
  int sMonth = sMonthStr.toInt();
  int sDay = sDayStr.toInt();
  int sHr = sHrStr.toInt();
  int sMin = sMinStr.toInt();
  int sSec = sSecStr.toInt();
  
  Serial.println(tStart);
  Serial.println("Start: " + sYearStr + " " + sMonthStr + " " + sDayStr + " " + sHrStr + " " + sMinStr + " " + sSecStr);

  int eYear = eYearStr.toInt();
  int eMonth = eMonthStr.toInt();
  int eDay = eDayStr.toInt();
  int eHr = eHrStr.toInt();
  int eMin = eMinStr.toInt();
  int eSec = eSecStr.toInt();

  Serial.println(tEnd);
  Serial.println("End: " + eYearStr + " " + eMonthStr + " " + eDayStr + " " + eHrStr + " " + eMinStr + " " + eSecStr);
  
  preferences.begin("timeDifference", false);
  int prevEH = preferences.getInt("elapsedHour", 0);
  int prevEM = preferences.getInt("elapsedMin", 0);
  int prevES = preferences.getInt("elapsedSec", 0);

  Serial.println("PrevEH: " + String(prevEH));
  Serial.println("PrevEM: " + String(prevEM));
  Serial.println("PrevES: " + String(prevES));
  
  int currentES = 0;
  int currentEM = 0;
  int currentEH = 0;

  // Determining current elapsed time
  if(eSec - sSec < 0){
    currentES = abs(eSec - sSec);
  } else{
    currentES = eSec - sSec;
  }
  //Serial.println("currentES: " + String(currentES));
  if(eMin - sMin < 0){
    currentEM = abs(eMin - sMin);
  } else{
    currentEM = eMin - sMin;
  }
  if(eHr - sHr < 0){
    currentEH = abs(eHr - sHr);
  } else{
    currentEH = eHr - sHr;
  }

  Serial.println("CurrEH: " + String(currentEH));
  Serial.println("CurrEM: " + String(currentEM));
  Serial.println("CurrES: " + String(currentES));
  
  // Adding to CumulativeSum
  int finES;
  int finEM;
  int finEH;
  int carry = 0;

  if(currentES + prevES > 59){
    finES = (currentES + prevES) % 60;
    carry = (currentES + prevES) / 60;
  } else{
    finES = currentES + prevES;
    carry = 0;
  }
  if(currentEM + prevEM + carry> 59){
    finEM = (currentEM + prevEM + carry) % 60;
    carry = (currentEM + prevEM + carry) / 60;
  } else{
    finEM = currentEM + prevEM + carry;
    carry = 0;
  }
  finEH = currentEH + prevEH + carry;
  carry = 0;

  preferences.putInt("elapsedHour", finEH);
  preferences.putInt("elapsedMin", finEM);
  preferences.putInt("elapsedSec", finES);
  
  Serial.println("ElapsedTime: " + String(finEH) + String(finEM) + String(finES));
  Serial.println("yay!");

  // FOR NOW
  if(finES > 30){
    Serial.println("It's only been 30 seconds since you practiced. Keep going.");
    preferences.putInt("elapsedHour", 0);
    preferences.putInt("elapsedMin", 0);
    preferences.putInt("elapsedSec", 0);
  }
  preferences.end();
  
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  SleepTimer = timerBegin(0,80,true);
  timerAttachInterrupt(SleepTimer, &onDeepSleepTimer, true);
  timerAlarmWrite(SleepTimer,1000000,true);
 
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000000, true);
  timerAlarmEnable(timer);

  touchAttachInterrupt(T3, callback, Threshold);
  esp_sleep_enable_touchpad_wakeup();

  // Remove all preferences under the opened namespace
  // preferences.clear();

  // Or remove the counter key only
  //preferences.remove("counter");

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
  
  stringService->addCharacteristic(&dateCharacteristics);
  dateDescriptor.setValue("Date: ");
  dateCharacteristics.addDescriptor(new BLE2902());

  stringService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x06);  // set value to 0x00 to not advertise this parameter
  pAdvertising->setMinPreferred(0x12);
  
  BLEDevice::startAdvertising();

// initialize sensors
// -------------------------------------------
    pinMode(25,INPUT);
    pinMode(14,INPUT_PULLDOWN);
    pinMode(26,INPUT);
    pinMode(13,INPUT);
    pinMode(0,INPUT_PULLDOWN);
    pinMode(2, INPUT);
    
    x_tilt = !digitalRead(x_sensor_pin); // 0x00200000
    y_tilt = !digitalRead(y_sensor_pin); // 0x00400000
    z_tilt = !digitalRead(z_sensor_pin); // 0x0001
    nx_tilt = digitalRead(nx_sensor_pin); // 0x4000
    ny_tilt = digitalRead(ny_sensor_pin); // 0x2000
    nz_tilt = digitalRead(nz_sensor_pin); // 0x0004  
  
    x_lastState = x_tilt;
    y_lastState = y_tilt;
    z_lastState = z_tilt;
    nx_lastState = nx_tilt;
    ny_lastState = ny_tilt;
    nz_lastState = nz_tilt;
 // -------------------------------------------
 
  for(int i = 1; i <= 10; i++){
        //
        String namingStorage = baseNameDataSpace + String(i); // mmddyyyyhhmmf
        int name_length = namingStorage.length() + 1;
        char nameData[name_length];
        namingStorage.toCharArray(nameData,name_length);
        Serial.println(nameData);

        preferences.begin(nameData, false);
        String stuffs = preferences.getString("date","nodate");
        preferences.end();
        //String stuffs = preferences.getString("date","nodate");
        Serial.println(stuffs);
        
// Clearing stuff for debug
        preferences.begin(nameData, false);
        preferences.clear();
        preferences.end();
        
      }

// Clearing stuff for debug

  int name_length = timeDifference.length() + 1;
  char timeDif[name_length];
  timeDifference.toCharArray(timeDif, name_length);
  Serial.println(timeDif);
  
  preferences.begin(timeDif, false);
  preferences.clear();
  preferences.end();
  

}

void loop() {
 
  x_tilt = !digitalRead(x_sensor_pin); // 0x00200000
  y_tilt = !digitalRead(y_sensor_pin); // 0x00400000
  z_tilt = !digitalRead(z_sensor_pin); // 0x0001
  nx_tilt = digitalRead(nx_sensor_pin); // 0x4000
  ny_tilt = digitalRead(ny_sensor_pin); // 0x2000
  nz_tilt = digitalRead(nz_sensor_pin); // 0x0004   
  SensorCheck();
 
 
  if (deviceConnected) {
    //Serial.println("Connected");
    if(touchRead(T0) < 60){
      if(!isTouched){
          isTouched = true;
          
          String namingStorage = baseNameDataSpace + String(count); // mmddyyyyhhmmf
          int name_length = namingStorage.length() + 1;
          char nameData[name_length];
          namingStorage.toCharArray(nameData,name_length);
          Serial.println(nameData);

          static char timestamp[20];
          preferences.begin(nameData, false);
          translate = GetCurrentTimeStamp();
          dateValue = translate.toDouble();
          
          dtostrf(dateValue, 14, 0, timestamp);
          tempStartData = timestamp;
          
          strcat(timestamp,"S");
          Serial.print(timestamp);
          Serial.println(". . . start");
         
          preferences.putString("date", timestamp);
          preferences.end();

          dateCharacteristics.setValue(timestamp);
          dateCharacteristics.notify();
        
          count++;
          if(count > 10){
            isStorageFull = true;
          }
      }
    }
    if(touchRead(T0) >= 66){
      if(isTouched){
        isTouched = false;
        
        String namingStorage = baseNameDataSpace + String(count); // mmddyyyyhhmmf
        int name_length = namingStorage.length() + 1;
        char nameData[name_length];
        namingStorage.toCharArray(nameData,name_length);
        Serial.println(nameData);
        
        preferences.begin(nameData, false);
        
        static char timestamp[20];
        translate = GetCurrentTimeStamp();
        dateValue = translate.toDouble();
        dtostrf(dateValue, 14, 0, timestamp);
        tempEndData = timestamp;

        strcat(timestamp,"E");
         
        Serial.print(timestamp);
        Serial.println(" . . . end");
        
        preferences.putString("date", timestamp);
        preferences.end();

        dateCharacteristics.setValue(timestamp);
        dateCharacteristics.notify();


        CalculateTimeElapsed(tempStartData, tempEndData);
    
        count++;
        if(count > 10){
          isStorageFull = true;
        }
       
      }   
    }
    if(isStorageFull){
      Serial.println("Sorry, data is full. Restarting in 5 seconds..");
      for(int i = 1; i <= 10; i++){
        //
//        String namingStorage = baseNameDataSpace + String(i); // mmddyyyyhhmmf
//        int name_length = namingStorage.length() + 1;
//        char nameData[name_length];
//        namingStorage.toCharArray(nameData,name_length);
//        Serial.println(nameData);

        //preferences.begin(nameData, false);
        //String stuffs = preferences.getString("date","nodate");
        //preferences.end();
        //String stuffs = preferences.getString("date","nodate");
        //Serial.println(stuffs);
        
      }
      //ESP.restart();
      delay(30000);
    } 
  }
  if(!deviceConnected){
    if(isReadySendData){
      char timeSent[20];
      
      for(int i = 1; i <= count; i++){
        String namingStorage = baseNameDataSpace + String(i);
        int name_length = namingStorage.length() + 1;
        char nameData[name_length];
        namingStorage.toCharArray(nameData,name_length);
      
        preferences.begin(nameData, false);
        String timeString = preferences.getString("date","nodate");
        int timeStringLength = timeString.length() + 1;
        
        timeString.toCharArray(timeSent,timeStringLength);
        
        dateCharacteristics.setValue(timeSent);
        dateCharacteristics.notify();
        preferences.end();
            
        count = 0;
        isStorageFull = false;
        isReadySendData = false;
      }
    }
    if(isClearingData){
      for(int i = 1; i <= 10; i++){
        String namingStorage = baseNameDataSpace + String(i);
        int name_length = namingStorage.length() + 1;
        char nameData[name_length];
        namingStorage.toCharArray(nameData,name_length);
        preferences.begin(nameData, false);
        preferences.clear();
        preferences.end();
      }
      isStorageFull = false;
      isReadySendData = false;
      isClearingData = false;
      count = 1;    
    }
  }
  
}
