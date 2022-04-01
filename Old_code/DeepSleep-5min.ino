/*
Deep Sleep with Touch Wake Up
=====================================
This code displays how to use deep sleep with
a touch as a wake up source and how to store data in
RTC memory to use it over reboots

This code is under Public Domain License.

Author:
Pranav Cherukupalli <cherukupallip@gmail.com>
*/



/*
 * NOTE: replace touch sensor with external input. This is in lieu of 
 *       the tilt sensor
 * 
 * 
 * 
 * 
 */


/* Timer */
volatile int interruptCounter;
int totalInterruptCounter;
int hours;
int minutes;

int timeActive;

hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
 
void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  interruptCounter++;
  portEXIT_CRITICAL_ISR(&timerMux);

 
}

/* Getting Deep sleeps */

#define Threshold 40 /* Greater the value, more the sensitivity */

RTC_DATA_ATTR int bootCount = 0;
touch_pad_t touchPin;
/*
Method to print the reason by which ESP32
has been awaken from sleep
*/
void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

/*
Method to print the touchpad by which ESP32
has been awaken from sleep
*/
void print_wakeup_touchpad(){
  touchPin = esp_sleep_get_touchpad_wakeup_status();

  switch(touchPin)
  {
    case 0  : Serial.println("Touch detected on GPIO 4"); break;
    case 1  : Serial.println("Touch detected on GPIO 0"); break;
    case 2  : Serial.println("Touch detected on GPIO 2"); break;
    case 3  : Serial.println("Touch detected on GPIO 15"); break;
    case 4  : Serial.println("Touch detected on GPIO 13"); break;
    case 5  : Serial.println("Touch detected on GPIO 12"); break;
    case 6  : Serial.println("Touch detected on GPIO 14"); break;
    case 7  : Serial.println("Touch detected on GPIO 27"); break;
    case 8  : Serial.println("Touch detected on GPIO 33"); break;
    case 9  : Serial.println("Touch detected on GPIO 32"); break;
    default : Serial.println("Wakeup not by touchpad"); break;
  }
}


/*When TP is touched, we run this function*/
void onActive(){
  Serial.println("Resetting timer for deep-sleep...");
  totalInterruptCounter = 0;
  minutes = 0;
  delay(1000);
  timeActive++;
  Serial.print("Time Active: ");
  Serial.println(timeActive);
}

void setup(){
  Serial.begin(115200);
  delay(1000); //Take some time to open up the Serial Monitor

  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000000, true);
  timerAlarmEnable(timer);

  //Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  //Print the wakeup reason for ESP32 and touchpad too
  print_wakeup_reason();
  print_wakeup_touchpad();

  //Setup interrupt on Touch Pad 3 (GPIO15)
  touchAttachInterrupt(T3, onActive, Threshold); // utilize A4 pin - put a tweezer on it!

  //Configure Touchpad as wakeup source
  esp_sleep_enable_touchpad_wakeup();

}

void loop(){  
  // Timer
  if (interruptCounter > 0) {
 
    portENTER_CRITICAL(&timerMux);
    interruptCounter--;
    portEXIT_CRITICAL(&timerMux);

    totalInterruptCounter++;
    
   
   // Case where there is 5 mins of inactivity
    if(totalInterruptCounter > 59){
      minutes++;
      totalInterruptCounter = 0;
      }
    if(minutes > 4){
        Serial.println("Going to sleep now - past the 5 min mark");
        totalInterruptCounter = 0;
        minutes = 0;
        esp_deep_sleep_start();
        }

 //For testing purposes of making sure it goes at a relatively accurate timing.
    Serial.print("Time Inactive...");
    if(minutes < 10){
        Serial.print("0");
        }
      Serial.print(minutes);
      Serial.print(":");
      if(totalInterruptCounter < 10){
        Serial.print("0");
        }
      Serial.println(totalInterruptCounter);

  }

}
