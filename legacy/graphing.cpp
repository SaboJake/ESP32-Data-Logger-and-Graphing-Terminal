#include "util.h"
#include "graphing.h"

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_BMP085.h> 

#define COLOR_GRID 0x39E7 // Dark Grey for dividers

unsigned long samplingRate = 50;

float potScale[2]   = {0, 4095};
float lightScale[2] = {0, 4095};
float tempScale[2]  = {20.0, 35.0};    // Expected temp range in Celsius
float pressScale[2] = {98000, 102000}; // Expected pressure range in Pascals

const int graphStartX = 45;  // Leave space on the left for text labels
int currentX = graphStartX;

// Previous Y values (needed to draw lines between points)
int prevPotY, prevLightY, prevTempY, prevPressY;
unsigned long lastUpdate = 0;

// Helper function to map float values to screen coordinates safely
int mapFloatToY(float value, float minVal, float maxVal, int yBottom, int yTop) {
  if (value < minVal) value = minVal;
  if (value > maxVal) value = maxVal;
  return (value - minVal) * (yTop - yBottom) / (maxVal - minVal) + yBottom;
}

// Function to draw the static labels and grid lines
void drawUI() {
  tft.fillRect(0, 0, screenWidth, screenHeight, ST77XX_BLACK); // Clear screen
  
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  
  // Track 1: Potentiometer (Y: 0 to 59)
  tft.setCursor(5, 25); tft.print("POT");
  tft.drawLine(0, 60, screenWidth, 60, COLOR_GRID);

  // Track 2: Light (Y: 60 to 119)
  tft.setCursor(5, 85); tft.print("LGT");
  tft.drawLine(0, 120, screenWidth, 120, COLOR_GRID);

  // Track 3: Temperature (Y: 120 to 179)
  tft.setCursor(5, 145); tft.print("TMP");
  tft.drawLine(0, 180, screenWidth, 180, COLOR_GRID);

  // Track 4: Pressure (Y: 180 to 239)
  tft.setCursor(5, 205); tft.print("PRS");
}

void setup_graphing() {
  Serial.begin(115200);

  Wire.begin(21, 22);

  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);

  tft.init(240, 320); 
  tft.setRotation(3);      
  tft.invertDisplay(false); 

  if (!bmp.begin()) {
    Serial.println("Could not find BMP180 sensor!");
  }

  // Draw initial layout
  drawUI();
}

void loop_graphing() {
  if (millis() - lastUpdate >= samplingRate) {
    lastUpdate = millis();

    long potSum = 0, lightSum = 0;
    for(int i = 0; i < 10; i++) {
      potSum += analogRead(34);
      lightSum += analogRead(35);
    }
    float potValue = potSum / 10.0;
    float lightValue = lightSum / 10.0;
    
    float temperature = bmp.readTemperature(); 
    float pressure = bmp.readPressure();     

    int curPotY   = mapFloatToY(potValue, potScale[0], potScale[1], 55, 5);
    int curLightY = mapFloatToY(lightValue, lightScale[0], lightScale[1], 115, 65);
    int curTempY  = mapFloatToY(temperature, tempScale[0], tempScale[1], 175, 125);
    int curPressY = mapFloatToY(pressure, pressScale[0], pressScale[1], 235, 185);

    if (currentX > graphStartX) { // Skip drawing a line on the very first pixel
      tft.drawLine(currentX - 1, prevPotY,   currentX, curPotY,   ST77XX_MAGENTA);
      tft.drawLine(currentX - 1, prevLightY, currentX, curLightY, ST77XX_YELLOW);
      tft.drawLine(currentX - 1, prevTempY,  currentX, curTempY,  ST77XX_GREEN);
      tft.drawLine(currentX - 1, prevPressY, currentX, curPressY, ST77XX_CYAN);
    }

    prevPotY   = curPotY;
    prevLightY = curLightY;
    prevTempY  = curTempY;
    prevPressY = curPressY;
    
    currentX++;

    // Wrap Around Logic
    if (currentX >= screenWidth) {
      currentX = graphStartX;
      
      // Clear ONLY the graph area (leaves text labels intact)
      tft.fillRect(graphStartX, 0, screenWidth - graphStartX, screenHeight, ST77XX_BLACK);
      
      // Redraw the grid dividers over the cleared area
      tft.drawLine(graphStartX, 60, screenWidth, 60, COLOR_GRID);
      tft.drawLine(graphStartX, 120, screenWidth, 120, COLOR_GRID);
      tft.drawLine(graphStartX, 180, screenWidth, 180, COLOR_GRID);
    }
  }
}
