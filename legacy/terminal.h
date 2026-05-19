#ifndef ESP_TERMINAL_H
#define ESP_TERMINAL_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "ps2_keyboard.h"

// Define special keys based on your specific PS/2 library.
// You may need to adjust these values to match what your keyboard.read() outputs for arrows.
#define KEY_BACKSPACE 8
#define KEY_ENTER     13
#define KEY_UP        129 // Adjust based on your ps2_keyboard.h
#define KEY_DOWN      130 // Adjust based on your ps2_keyboard.h
#define KEY_LEFT      131
#define KEY_RIGHT     132

const int MAX_HISTORY = 10;

class ESPTerminal {
  private:
    Adafruit_ST7789* tft;
    PS2Keyboard* kb;
    
    int cols;
    int rows;
    int charWidth;
    int charHeight;
    
    String* screenBuffer;
    int cursorY = 0;
    int cursorPos = 0; // Position within the currentCommand
    
    String currentCommand = "";
    unsigned long lastCursorBlink = 0;
    bool cursorVisible = true;
    
    String history[MAX_HISTORY];
    int historyIndex = 0;     // Where the next saved command goes
    int historyViewIndex = 0; // Where we are currently browsing in history

  public:
    ESPTerminal(Adafruit_ST7789* display, PS2Keyboard* keyboard, int columns, int lines, int textSize = 2);

    ~ESPTerminal();

    void begin();

    void update();

  private:
    void handleKey(char c);

    void executeCommand();

    void browseHistory(int direction);

    void println(String text);

    void scrollUp();

    void clearScreen();

    void printPrompt();

    void redrawScreen();

    void redrawInputLine();

    void drawCursor(bool show);
};

#endif