#include "basic_logging.h"
#include "util.h"

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_BMP085.h> 

void setup_basic_logging() {
  Serial.begin(115200);

  // Initialize I2C Bus for the BMP180
  Wire.begin(21, 22);

  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);

  tft.init(240, 320); 
  delay(100); 
  tft.setRotation(3);      
  tft.invertDisplay(false); 
  tft.fillScreen(ST77XX_BLACK); 

  if (!bmp.begin()) {
    Serial.println("Could not find BMP180 sensor!");
  }

  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(3);
  tft.setCursor(20, 20);
  tft.println("SENSOR DASH");
  
  // Draw a dividing line
  tft.drawLine(20, 55, 300, 55, ST77XX_BLUE);
}

void loop_basic_logging() {
  long potSum = 0;
  long lightSum = 0;
  for(int i = 0; i < 50; i++) {
    potSum += analogRead(34);
    lightSum += analogRead(35);
    delay(1); 
  }
  int potValue = potSum / 50;
  int lightValue = lightSum / 50;

  float temperature = bmp.readTemperature(); // In Celsius
  int32_t pressure = bmp.readPressure();     // In Pascals

  // float temperature = 0; // In Celsius
  // int32_t pressure = 0;     // In Pascals

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);

  tft.setCursor(20, 80);
  tft.print("Pot   : "); 
  tft.print(potValue); 
  tft.print("    ");

  tft.setCursor(20, 115);
  tft.print("Light : "); 
  tft.print(lightValue); 
  tft.print("    ");

  tft.setCursor(20, 150);
  tft.print("Temp  : "); 
  tft.print(temperature); 
  tft.print(" C  ");

  tft.setCursor(20, 185);
  tft.print("Press : "); 
  tft.print(pressure); 
  tft.print(" Pa   ");

  delay(500);
}