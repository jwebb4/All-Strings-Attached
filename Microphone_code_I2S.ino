#include <arduinoFFT.h>
#include "driver/i2s.h"

#define SAMPLES 512              //Must be a power of 2
#define SamplingRate 40000

const i2s_port_t I2S_PORT = I2S_NUM_0;
double vReal[SAMPLES];
double vImag[SAMPLES];
double StoredData[SAMPLES];
unsigned long newTime, oldTime;
unsigned int sampling_period_us;
int32_t SampleData;
uint8_t frequencyCount = 0;
int bytes_read;
bool StringPlaying = 0;

arduinoFFT FFT = arduinoFFT();

void setup() {

  
  Serial.begin(115200);
  esp_err_t err;
  
  pinMode(12,OUTPUT);
  pinMode(4,INPUT);
  pinMode(22,OUTPUT);

  // The I2S config as per the example
  const i2s_config_t i2s_config = {
      .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX), // Receive, not transfer
      .sample_rate = SamplingRate,                         // 16KHz
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // could only get it to work with 32bits
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // use right channel
      .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,     // Interrupt level 1
      .dma_buf_count = 4,                           // number of buffers
      .dma_buf_len = 16                             // 8 samples per buffer (minimum)
  };

  sampling_period_us = round(1000000 * (1.0 / SamplingRate));

  // The pin config as per the setup
  const i2s_pin_config_t pin_config = {
      .bck_io_num = 22,   // Serial Clock (SCK) SCL
      .ws_io_num = 12,    // Word Select (WS)   D2
      .data_out_num = I2S_PIN_NO_CHANGE, // not used (only for speakers)
      .data_in_num = 4   // Serial Data (SD)   D3
  };

  // Configuring the I2S driver and pins.
  // This function must be called before any I2S driver read/write operations.
  err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed installing driver: %d\n", err);
    while (true);
  }
  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Failed setting pin: %d\n", err);
    while (true);
  }
  Serial.println("I2S driver installed.");
}

void loop() {
  // Read a single sample and log it for the Serial Plotter.
  for (int i = 0; i < SAMPLES; i++)
  {
    
    newTime = micros() - oldTime;
    oldTime = newTime;
    SampleData = 0;
    bytes_read = i2s_pop_sample(I2S_PORT, (char *)&SampleData, portMAX_DELAY); // no timeout
    vReal[i] = SampleData/10;
    StoredData[i] = SampleData;
    vImag[i] = 0;
    while (micros() < (newTime + sampling_period_us))
    {
      delay(0);
    }
  }
  FFT.Windowing(vReal, SAMPLES, FFT_WIN_TYP_HANN, FFT_FORWARD);
  FFT.Compute(vReal, vImag, SAMPLES, FFT_FORWARD);

  for(uint16_t i = 0; i < SAMPLES; i++){
    vReal[i] = sqrt(sq(vReal[i]) + sq(vImag[i]));
  }

  StringPlaying = 0;
  frequencyCount = 0;
  for(int i = 0; i < SAMPLES; i++){             // Band pass filter
      if(i < 2 || i >= 130){
        vReal[i] = 0;
      }
      if(vReal[i] < 50000000){
        vReal[i] = 0;
      }
      if (vReal[i] != 0 && vReal[i+2] < vReal[i] * .26) {
        frequencyCount++;
      }
      if(frequencyCount > 5) {
        StringPlaying = 1;
      }
    }
    Serial.println(StringPlaying);
  }
