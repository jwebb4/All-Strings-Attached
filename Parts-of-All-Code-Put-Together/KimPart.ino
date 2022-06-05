/*
All Strings Attached Program
  Team Members: Kimberly Tse, Joshua Webb, Carlos Rosales
  Team Advisor: Dr. Ben Abbott

  Purpose: Send time to the mobile application associated with this project whenever 
              it is connected && detects activity
           Save time to local memory whenver it is not connected && detects activity
  
*/


/* ------------------------------------------------------------------------------------------
 * Libraries used:
 *  sys/time.h - Get RTC time
 *  Preferences - Save into Flash memory
 *  BLEDevice - Provides functionality to build BLE server with characteristics.
 *  BLEServer - ...
 *  BLEUtils - ...
 *  BLE2902 - ...
 *  
 */
 
#include <sys/time.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
//#include <BLE2902.h>

// FILLER FOR TILT SENSORS
#define Threshold 40

/* ------------------------------------------------------------------------------------------
 * DEFINE
 *  Storing timestamps in flash memory
 *  
 */
#define MAX_CHARS 15
#define MAX_LOCAL_STORAGE 50

/* ------------------------------------------------------------------------------------------
 * DEFINE
 *  Tilt Sensors usage based on pins
 *  
 */
#define x_sensor_pin 25  //D2
#define nx_sensor_pin 14 //D6
#define y_sensor_pin 26  //D3
#define ny_sensor_pin 13 //D7
#define z_sensor_pin 0   //D5
#define nz_sensor_pin 2  //D9

/* ------------------------------------------------------------------------------------------
 * DEFINE
 *  Creating BLE server using Service / Char UUID
 *  
 */
#define bleServerName "Strings attached"
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b" // Currently using
#define dateCharacteristicUUID  "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define recentTimeCharacteristicUUID "688091db-1736-4179-b7ce-e42a724a6a68"

/* ------------------------------------------------------------------------------------------
 * Objects
 *  Preferences - API for saving data
 *  BLECharacteristic - API for creating a BLECharacteristic
 *  
 *  
 */
Preferences preferences;
BLEServer* pServer = NULL;
BLECharacteristic* dateCharacteristics = NULL;
BLECharacteristic* recentTimeCharacteristics = NULL;
BLECharacteristic* batteryCharacteristics = NULL;

/* ------------------------------------------------------------------------------------------
 * Preference Variables
 *  baseNameDataSpace - Namespace for saving each of the timestamps
 *  temp... - Temporarily stores a timestamp for computation of elapsed time
 *  pref_count - Getting the number of counts currently ready to store in what namespace
 *  cumulativeHr - Gets number of hours practice cumulatively to tell users whenever 
 *    they need to change strings
 *  
 */
String baseNameDataSpace = "dtStorage";
String tempStartData = "";
String tempEndData = "";
int pref_count = 1;
int cumulativeHr;

String translate = "";
static char timestamp[21];

String currentETString = "";
static char currentETCharArray[7]; // 6 + \n

/* ------------------------------------------------------------------------------------------
 * Behavioral States
 *  isTouched - locking mechanism to not spam touch 
 *  isStorageFull - Leads to special cases and interrupt
 *  isTransfer - checks to see if the transfer is complete
 */
bool isTouched = false;
bool isStorageFull = false; // most liekly set a limit of 5 start/end saves
RTC_DATA_ATTR bool isTransfer = false;


/* ------------------------------------------------------------------------------------------
 * Timing Mechanism
 *  currentLoopCount - when uploading data, checks to see if what value we are uploading 
 */

int currentLoopCount = 1;

/* ------------------------------------------------------------------------------------------
 * Tilt Sensor States
 *  Each variable depicts the state of the tilt sensors 
 *  
 */
 
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

/* ------------------------------------------------------------------------------------------
 * Bluetooth States
 *  deviceConnected - state of whether microcontroller is connected to the mobile application
 *  oldDeviceConnected - whether we have connected to the same device as before
 *  firstConnection - whether it is the first transmission of data since connectivity 
 *    (repeats on each connection)
 *  first_use - relates to oldDeviceConnected
 *  isDeviceSettedUp - sets time of reference for microcontroller
 *  
 *  AllSensorsChecked - 
 *  SensorsChanged - 
 *  
 */

bool deviceConnected = false;
bool oldDeviceConnected = false;

bool firstConnection = false;

RTC_DATA_ATTR bool first_use = true;
RTC_DATA_ATTR bool isDeviceSettedUp = false;

bool AllSensorsChecked = 0;
bool SensorChanged = 0;

touch_pad_t touchPin;

/* ------------------------------------------------------------------------------------------
 * RTC Counters and Interrupt Variables
 *  LED
 *  Sleep
 * 
 */

volatile int LEDInterruptCounter;
int totalLEDInterruptCounter;
hw_timer_t* LEDTimer = NULL;
portMUX_TYPE LEDTimerMux = portMUX_INITIALIZER_UNLOCKED;

volatile int TransferInterruptCounter;
int totalTransferInterruptCounter;
hw_timer_t* TransferTimer = NULL;
portMUX_TYPE TransferTimerMux = portMUX_INITIALIZER_UNLOCKED;

volatile int Interupt_Counter = 0;
hw_timer_t* SleepTimer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

volatile int BLEInterruptCounter;
int totalBLEInterruptCounter;
hw_timer_t* BLETimer = NULL;
portMUX_TYPE BLETimerMux = portMUX_INITIALIZER_UNLOCKED;

/* ------------------------------------------------------------------------------------------
 * TimeStruct
 *  This is to get the RTC timer
 * 
 */

struct tm getTimeStruct()
{
  struct tm timeinfo;
  getLocalTime(&timeinfo, 0);
  return timeinfo;
}

/* ------------------------------------------------------------------------------------------
 * CLASS: BLEServerCallbacks
 *  This is to determining whether the device is connected or not
 * 
 */
 
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      firstConnection = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      firstConnection = false;
      pServer->startAdvertising();
    }
};


/* ------------------------------------------------------------------------------------------
 * Interrupts - LED
 *  Behaviors when we hit an interrupt
 * 
 */
 
void IRAM_ATTR onLEDTimer() {
  portENTER_CRITICAL_ISR(&LEDTimerMux);
  LEDInterruptCounter++;
  if(isStorageFull){
    //Serial.println("heyo stop");
    /*
     * Yellow LED code to blink
     */
    if(deviceConnected){
      //Serial.println("Transferred data");
      /* 
       *  Yellow LED turns off
       */
    }
  }
  if(cumulativeHr >= 100){
    //Serial.println("oops");
    if(deviceConnected){
      /*
      preferences.begin("timeDifference", false);
      preferences.clear();
      preferences.end();
      */
      /*
       * Red LED turns off
       */
    }
    /*  
     * Red LED blinks
     */
  }  
  portEXIT_CRITICAL_ISR(&LEDTimerMux);
}

/* ------------------------------
 * 
 */
void IRAM_ATTR onTransferTimer() {
  portENTER_CRITICAL_ISR(&TransferTimerMux);
  TransferInterruptCounter++;
  if(deviceConnected && (isStorageFull || pref_count > 1)){
    isTransfer = true;
  }
  portEXIT_CRITICAL_ISR(&TransferTimerMux);
}

/* ------------------------------------------------------------------------------------------
 * Interrupts - DeepSleep
 *  Behaviors when we hit an interrupt
 * 
 */
void IRAM_ATTR onDeepSleepTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  Interupt_Counter++;
  // Serial.println(String(Interupt_Counter));
  if (Interupt_Counter >= 60) {
      // Serial.println("start deep sleep");
      esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK,ESP_EXT1_WAKEUP_ANY_HIGH);
      esp_bt_controller_disable();
      esp_deep_sleep_start();
  }
  portEXIT_CRITICAL_ISR(&timerMux);
}

/* ------------------------------------------------------------------------------------------
 * Interrupts - BLEConnect
 *  Behaviors when we hit an interrupt
 * 
 */
void IRAM_ATTR onBLETimer() {
  portENTER_CRITICAL_ISR(&BLETimerMux);
  BLEInterruptCounter++;
  // Serial.println(String(BLEInterruptCounter));
  if (deviceConnected && BLEInterruptCounter >= 50) {
     // Serial.println("Auto-Disconnect");
     pServer->disconnectClient();
  }
  portEXIT_CRITICAL_ISR(&BLETimerMux);
}

/* ------------------------------------------------------------------------------------------
 * Procedure - SensorCheck
 *  Checks for sensors, change sensors states as needed
 * 
 */
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

/* ------------------------------------------------------------------------------------------
 * Procedure - WakeupChange
 *  ...
 * 
 */
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


/* ------------------------------------------------------------------------------------------
 * To be dated
 * 
 */
void IRAM_ATTR callback(){
}

/* ------------------------------------------------------------------------------------------
 * Function - GetCurrentTMStamp
 *  Returns timestamp upon called
 * 
 */
String GetCurrentTMStamp(int yearTM, int monthTM, int dayTM, 
 int hourTM, int minTM, int secTM){
  String fYear = "";
  String fMonth = "";
  String fDay = "";
  String fHr = "";
  String fMin = "";
  String fSec = "";

  yearTM += 1900;
  monthTM += 1;
  //minTM -= 18;

  fYear = String(yearTM);
  if(monthTM < 10 ){
    fMonth = "0" + String(monthTM);
  } else {
    fMonth = String(monthTM);
  }
  if(dayTM < 10){
    fDay = "0" + String(dayTM);
  } else{
    fDay = String(dayTM);  
  }
  if(hourTM < 10){
    fHr = "0" + String(hourTM);
  } else{
    fHr = String(hourTM);  
  }
  if(minTM < 10){
    fMin = "0" + String(minTM);
  } else{
    fMin = String(minTM);  
  }
  if(secTM < 10){
    fSec = "0" + String(secTM);
  } else{
    fSec = String(secTM);  
  }
  return fYear + fMonth + fDay + fHr + fMin + fSec;                           
}

/* ------------------------------------------------------------------------------------------
 * Function - GetCurrentTMStamp
 *  Returns timestamp upon called
 * 
 */
int CalculateTimeElapsed(String tStart, String tEnd){
  // Getting variables
  String sHrStr = String(tStart.charAt(8)) + String(tStart.charAt(9));
  String sMinStr = String(tStart.charAt(10)) + String(tStart.charAt(11));
  String sSecStr = String(tStart.charAt(12)) + String(tStart.charAt(13));
  
  String eHrStr = String(tEnd.charAt(8)) + String(tEnd.charAt(9));
  String eMinStr = String(tEnd.charAt(10)) + String(tEnd.charAt(11));
  String eSecStr = String(tEnd.charAt(12)) + String(tEnd.charAt(13));

  int sHr = sHrStr.toInt();
  int sMin = sMinStr.toInt();
  int sSec = sSecStr.toInt();

  int eHr = eHrStr.toInt();
  int eMin = eMinStr.toInt();
  int eSec = eSecStr.toInt();

  preferences.begin("timeDifference", false);
  int prevEH = preferences.getInt("elapsedHour", 0);
  int prevEM = preferences.getInt("elapsedMin", 0);
  int prevES = preferences.getInt("elapsedSec", 0);
  preferences.end();

  int currentES = 0;
  int currentEM = 0;
  int currentEH = 0;
  int carryMin = 0;
  int carryHr = 0;

  // Determining current elapsed time
  if(eSec - sSec < 0){
    currentES = 60 - (sSec - eSec);
    carryMin = 1;
  } else{
    currentES = eSec - sSec;
  }
  if((eMin - sMin - carryMin )< 0){
    currentEM = 60 - carryMin - (sMin - eMin);
    carryHr = 1;
  } else{
    currentEM = eMin - sMin - carryMin;
  }
  if((eHr - sHr - carryHr) < 0){
    currentEH = 24 - carryHr - (sHr - eHr);
  } else{
    currentEH = eHr - sHr - carryHr;
  }
  
  String cEHs;
  String cEMs;
  String cESs;
  
  if(currentEH < 10){
    cEHs = "0" + String(currentEH);
  } else{
    cEHs = String(currentEH);  
  }
  if(currentEM < 10){
    cEMs = "0" + String(currentEM);
  } else{
    cEMs = String(currentEM);  
  }
  if(currentES < 10){
    cESs = "0" + String(currentES);
  } else{
    cESs = String(currentES);  
  }

  currentETString = cEHs + cEMs + cESs;

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

  preferences.begin("timeDifference", false);
  preferences.putInt("elapsedHour", finEH);
  preferences.end();
  
  preferences.begin("timeDifference", false);
  preferences.putInt("elapsedMin", finEM);
  preferences.end();

  preferences.begin("timeDifference", false);
  preferences.putInt("elapsedSec", finES);
  preferences.end();
  
  Serial.println("ElapsedTime: " + String(finEH) + String(finEM) + String(finES));
  return finEH;
  
}

/*  Load/Increment/ClearPrefCount
 *   Allows us to manipuate the current count of data -1 saved in
 *   storage
 * 
 */
void LoadPrefCount(){
  preferences.begin("prefCount", false);
  pref_count = preferences.getInt("pref_count", 1);
  preferences.end();
}

void IncrementPrefCount(){
  preferences.begin("prefCount", false);
  pref_count = preferences.getInt("pref_count", 1);
  pref_count++;
  preferences.putInt("pref_count", pref_count);
  preferences.end();
}

void ClearPrefCount(){
  preferences.begin("prefCount", false);
  pref_count = preferences.getInt("pref_count", 1);
  pref_count = 1;
  preferences.putInt("pref_count", pref_count);
  preferences.end();
}


void setup() {
  // Begin baud rate
  Serial.begin(115200);
  Serial.println("All Strings Attached Program");
  

  // Set up timing for microcontroller if powered off before
  struct timeval tv;
  tv.tv_sec =   21600;  // enter UTC UNIX time (get it from https://www.unixtimestamp.com )
  settimeofday(&tv, NULL);

  // Set timezone to Epoch Central time
  setenv("TZ", "CST+6CDT,M3.2.0/2,M11.1.0/2", 1); // https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
  tzset();

  // Create BLE server name & callbacks
  BLEDevice::init(bleServerName);
  
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  // Create service and characteristics
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  dateCharacteristics = pService->createCharacteristic(
                      dateCharacteristicUUID,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  dateCharacteristics->setValue("dateChar");
  recentTimeCharacteristics = pService->createCharacteristic(
                      recentTimeCharacteristicUUID,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  recentTimeCharacteristics->setValue("recentTimeChar");

  // Start server and advertise characteristics
  pService->start();
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->start();

  // Start up multiple timer interrupts
  LEDTimer = timerBegin(0, 80, true);
  timerAttachInterrupt(LEDTimer, &onLEDTimer, true);
  timerAlarmWrite(LEDTimer, 1000000, true);
  timerAlarmEnable(LEDTimer);

  TransferTimer = timerBegin(1, 80, true);
  timerAttachInterrupt(TransferTimer, &onTransferTimer, true);
  timerAlarmWrite(TransferTimer, 50000, true);
  timerAlarmEnable(TransferTimer);

  SleepTimer = timerBegin(2,80,true);
  timerAttachInterrupt(SleepTimer, &onDeepSleepTimer, true);
  timerAlarmWrite(SleepTimer,1000000,true);
  //timerAlarmEnable(SleepTimer);

  BLETimer = timerBegin(3,80,true);
  timerAttachInterrupt(BLETimer, &onBLETimer, true);
  timerAlarmWrite(BLETimer,1000000,true);
  //timerAlarmEnable(BLETimer);

  // Set up deep sleep interrupt
  touchAttachInterrupt(T3, callback, Threshold);
  esp_sleep_enable_touchpad_wakeup();

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

 /*Check Storage*/
/*
  for(int i = 1; i <= MAX_LOCAL_STORAGE; i++){
    String namingStorage = baseNameDataSpace + String(i); // dtstorage#
    int name_length = namingStorage.length() + 1;
    char nameData[name_length];
    namingStorage.toCharArray(nameData,name_length);
    Serial.println(nameData);

    preferences.begin(nameData, false);
    String stuffs = preferences.getString("date","");
    Serial.println(stuffs);
    preferences.end();
         
  }
*/
/*  
  preferences.begin("timeDifference", false);
  int someNumberYes = preferences.getInt("elapsedSec", 0);
  Serial.println(someNumberYes);
  preferences.clear();
  preferences.end();
*/
  LoadPrefCount();

  if(pref_count > MAX_LOCAL_STORAGE){
    isStorageFull = true;
  }
}

void loop() {
  // Get current time
  //time_t timeNow;
  struct tm timeinfo = getTimeStruct();

  //time(&timeNow);
  //localtime_r(&timeNow, &timeinfo);
 
  x_tilt = !digitalRead(x_sensor_pin); // 0x00200000
  y_tilt = !digitalRead(y_sensor_pin); // 0x00400000
  z_tilt = !digitalRead(z_sensor_pin); // 0x0001
  nx_tilt = digitalRead(nx_sensor_pin); // 0x4000
  ny_tilt = digitalRead(ny_sensor_pin); // 0x2000
  nz_tilt = digitalRead(nz_sensor_pin); // 0x0004   

  // Check sensors
  SensorCheck();

  /* Case 1 - Device is Connected and Storage is not Full*/
  if (deviceConnected) {
    timerAlarmEnable(BLETimer);
    
    /* Get Start Time Stamp */
    if(touchRead(T3) < 60){
      timerAlarmDisable(BLETimer);
      BLEInterruptCounter = 0;
      
      if(!isTouched){
        Serial.println("DeviceConnected :: Start Touch");
        isTouched = true;

        // Get startTime
        translate = GetCurrentTMStamp(timeinfo.tm_year, 
                                        timeinfo.tm_mon, 
                                        timeinfo.tm_mday, 
                                        timeinfo.tm_hour,
                                        timeinfo.tm_min,
                                        timeinfo.tm_sec);
                                        
        // Convert to char array, used for later
        translate.toCharArray(timestamp, translate.length() + 1);
        Serial.println("Start touch");
        Serial.println(timestamp);

        // Save String tempStart so that we can calculate elapsedTime
        tempStartData = timestamp;
      }
    }

    /* Get End Time, Compute Elapsed Time, Send startTime & elapsedTime */
    if(touchRead(T3) >= 66){
      timerAlarmEnable(BLETimer);
      
      if(isTouched){
        isTouched = false;
        Serial.println("DeviceConnected :: End Touch");
        
        // Get endTime
        translate = GetCurrentTMStamp(timeinfo.tm_year, 
                                          timeinfo.tm_mon, 
                                          timeinfo.tm_mday, 
                                          timeinfo.tm_hour,
                                          timeinfo.tm_min,
                                          timeinfo.tm_sec);

        // convert to char array, save for later
        static char tempEndArr[20];
        translate.toCharArray(tempEndArr, translate.length() + 1);
        tempEndData = tempEndArr;

        // Get cumulative hour, also saves elapsed time
        cumulativeHr = CalculateTimeElapsed(tempStartData, tempEndData);
        currentETString.toCharArray(currentETCharArray, currentETString.length() + 1);

        // Concatenate startTime and elapsedTime
        strcat(timestamp,currentETCharArray);

        Serial.println(tempStartData);
        Serial.println(currentETCharArray);
        Serial.println(timestamp);

        // Send data to mobile application
        dateCharacteristics->setValue(timestamp);
        dateCharacteristics->notify();

      }
    }
    
    /* Sending data, clearing data*/
    if((isStorageFull || pref_count > 1) && isTransfer){
      if(currentLoopCount == 1){
        delay(50);
      }
      char timeSent[20];

      // Getting name storage
      String namingStorage = baseNameDataSpace + String(currentLoopCount);
      int name_length = namingStorage.length() + 1;
      char nameData[name_length];
      namingStorage.toCharArray(nameData,name_length);

      // Getting data to send and clear
      preferences.begin(nameData, false);
      String timeString = preferences.getString("date","nodate");
      preferences.end();
      
      int timeStringLength = timeString.length() + 1;
      timeString.toCharArray(timeSent,timeStringLength);

      // Send data
      dateCharacteristics->setValue(timeSent);
      dateCharacteristics->notify();

      // Clear specified storage
      preferences.begin(nameData, false);
      preferences.clear();
      preferences.end();

      Serial.println(nameData);
      
      currentLoopCount++;

      if(currentLoopCount >= pref_count){
        ClearPrefCount();
        isStorageFull = false;
        currentLoopCount = 1;
      }
      isTransfer = false;
      
    }
    if(firstConnection){
      delay(100);
      BLEInterruptCounter = 0;
      String recentTimeStamp = GetCurrentTMStamp(timeinfo.tm_year, 
                                          timeinfo.tm_mon, 
                                          timeinfo.tm_mday, 
                                          timeinfo.tm_hour,
                                          timeinfo.tm_min,
                                          timeinfo.tm_sec);
      char rtsChar[15];
      
      recentTimeStamp.toCharArray(rtsChar, recentTimeStamp.length() + 1);
      recentTimeCharacteristics->setValue(rtsChar);
      recentTimeCharacteristics->notify(); 
      firstConnection = false;
    }
  }

  /* Case 2 - Device is not connected && Storage is not full*/
  if(!deviceConnected && !isStorageFull){
    timerAlarmDisable(BLETimer);
    /* Get startTime */
    if(touchRead(T3) < 60){
      if(!isTouched){
        isTouched = true;
        Serial.println("!DeviceConnected :: Start Touch");

        // Get start timestamp
        translate = GetCurrentTMStamp(timeinfo.tm_year, 
                                        timeinfo.tm_mon, 
                                        timeinfo.tm_mday, 
                                        timeinfo.tm_hour,
                                        timeinfo.tm_min,
                                        timeinfo.tm_sec);
                                        
        // Convert to char array
        translate.toCharArray(timestamp, translate.length() + 1);

        // Save tempStart so that we can calculate elapsedTime
        tempStartData = timestamp;
      }
    }

    /* Get endTime*/
    if(touchRead(T3) >= 66){
      if(isTouched){
        isTouched = false;
        Serial.println("!DeviceConnected :: End Touch");

        // Creating datastorage for startTime, elapsedTime
        String namingStorage = baseNameDataSpace + String(pref_count);
        int name_length = namingStorage.length() + 1;
        char nameData[name_length];
        namingStorage.toCharArray(nameData,name_length);

        // Getting end time to calculate elapsedTime
        translate = GetCurrentTMStamp(timeinfo.tm_year, 
                                          timeinfo.tm_mon, 
                                          timeinfo.tm_mday, 
                                          timeinfo.tm_hour,
                                          timeinfo.tm_min,
                                          timeinfo.tm_sec);

        // Conversion to char array
        static char tempEndArr[20];
        translate.toCharArray(tempEndArr, translate.length() + 1);
        tempEndData = tempEndArr;

        // Get cumulative hour, also saves elapsed time
        cumulativeHr = CalculateTimeElapsed(tempStartData, tempEndData);
        currentETString.toCharArray(currentETCharArray, currentETString.length() + 1);

        // Concatenates startTime and elapsedTime
        strcat(timestamp,currentETCharArray);

        // Saving timestamp into flash
        preferences.begin(nameData, false);
        preferences.putString("date", timestamp);
        preferences.end();

        IncrementPrefCount();

        Serial.println(nameData);
        Serial.println(timestamp);
        
        //Serial.println("Count: " + String(pref_count));
        if(pref_count > MAX_LOCAL_STORAGE){
          isStorageFull = true;
        }
      }
    }
    // Stop sending mostRecentTimeCharacteristic value
    if(!firstConnection){
      firstConnection = true; 
    }   
  }
  
}
