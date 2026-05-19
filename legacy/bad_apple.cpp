#include "bad_apple.h"
#include "util.h"
#include "big_data.h"

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_BMP085.h> 

void setup_bad_apple() {
    pinMode(TFT_LED, OUTPUT);
    digitalWrite(TFT_LED, HIGH);
  
    tft.init(240, 320); 
    delay(100); 
    tft.setRotation(3);      
    tft.invertDisplay(false); 
    tft.fillScreen(ST77XX_BLACK); 
  }
  
  void loop_bad_apple() {
    uint32_t data_index = 0;
  
    for (int frame = 0; frame < NR_FRAMES; frame++) {
      uint32_t pixel_index = 0;
      
      tft.startWrite();
      tft.setAddrWindow(0, 0, tft.width(), tft.height());
  
      while (pixel_index < BUFF_SIZE) {
        uint8_t byte_in = big_array[data_index++];
        uint8_t color_bit = (byte_in >> 7) & 0x01; 
        uint8_t run_length = (byte_in & 0x7F) + 1; 
  
        uint16_t color = color_bit ? ST77XX_WHITE : ST77XX_BLACK;
        
        if (pixel_index + run_length > BUFF_SIZE) {
          run_length = BUFF_SIZE - pixel_index;
        }
  
        tft.writeColor(color, run_length);
        
        pixel_index += run_length;
      }
      
      tft.endWrite();
  
      delay(33);
    }
    
    // Animation restarts automatically
  }
