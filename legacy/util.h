#ifndef UTIL_H
#define UTIL_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_BMP085.h> 

// --- TFT Pins ---
#define TFT_SCK    18 
#define TFT_MISO   19 
#define TFT_MOSI   23 
#define TFT_CS     14
#define TFT_DC     25
#define TFT_RST    4
#define TFT_LED    32

// --- Custom Colors ---
#define COLOR_GRID 0x39E7 // Dark Grey for dividers

extern Adafruit_ST7789 tft;
extern Adafruit_BMP085 bmp;

extern const int screenWidth;
extern const int screenHeight;

#endif // UTIL_H
