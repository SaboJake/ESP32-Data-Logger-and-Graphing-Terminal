#include "util.h"

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
Adafruit_BMP085 bmp;
const int screenWidth = 320;
const int screenHeight = 240;
