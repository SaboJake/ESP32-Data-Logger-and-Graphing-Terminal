#include "terminal.h"

ESPTerminal::ESPTerminal(Adafruit_ST7789* display, PS2Keyboard* keyboard, int columns, int lines, int textSize) {
    tft = display;
    kb = keyboard;
    cols = columns;
    rows = lines;
    
    // Standard Adafruit font is 6x8 pixels at size 1
    charWidth = 6 * textSize;
    charHeight = 8 * textSize;
    
    screenBuffer = new String[rows];
    for(int i = 0; i < rows; i++) {
        screenBuffer[i] = "";
    }
    
    tft->setTextSize(textSize);
    tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK); // White text, black background
}

ESPTerminal::~ESPTerminal() {
    delete[] screenBuffer;
}

void ESPTerminal::begin() {
    clearScreen();
    printPrompt();
}

void ESPTerminal::update() {
    if (kb->available()) {
        char c = kb->read();
        handleKey(c);
    }
    
    if (millis() - lastCursorBlink > 500) {
        cursorVisible = !cursorVisible;
        drawCursor(cursorVisible);
        lastCursorBlink = millis();
    }
}

void ESPTerminal::handleKey(char c) {
    drawCursor(false); // Hide cursor before changing text

    if (c == '\n' || c == '\r') {
        executeCommand();
    } 
    else if (c == KEY_BACKSPACE || c == 127) {
        if (cursorPos > 0) {
            currentCommand = currentCommand.substring(0, cursorPos - 1) + currentCommand.substring(cursorPos);
            cursorPos--;
            redrawInputLine();
        }
    }
    else if (c == KEY_UP) {
        browseHistory(-1);
    }
    else if (c == KEY_DOWN) {
        browseHistory(1);
    }
    else if (c == KEY_LEFT) {
        if (cursorPos > 0) {
            cursorPos--;
        }
    }
    else if (c == KEY_RIGHT) {
        if (cursorPos < currentCommand.length()) {
            cursorPos++;
        }
    }
    else if (c >= 32 && c <= 126) { // Printable characters
        if (currentCommand.length() < cols - 2) { // Reserve space for prompt
            currentCommand = currentCommand.substring(0, cursorPos) + c + currentCommand.substring(cursorPos);
            cursorPos++;
            redrawInputLine();
        }
    }

    // Force cursor visible after typing or moving
    cursorVisible = true;
    drawCursor(true);
    lastCursorBlink = millis();
}

void ESPTerminal::executeCommand() {
    if (currentCommand.length() > 0) {
        // Save to screen buffer
        screenBuffer[rows - 1] = "> " + currentCommand;
        
        // Save to history
        history[historyIndex] = currentCommand;
        historyIndex = (historyIndex + 1) % MAX_HISTORY;
        historyViewIndex = historyIndex; // Reset history browser
        
        // --- PROCESS YOUR COMMAND HERE ---
        // Example:
        if (currentCommand == "help") {
            println("Available cmds: help, clear");
        } else if (currentCommand == "clear") {
            clearScreen();
            currentCommand = "";
            cursorPos = 0;
            printPrompt();
            return;
        } else {
            println("Unknown command.");
        }
    }
    
    currentCommand = "";
    cursorPos = 0;
    scrollUp();
    printPrompt();
}

void ESPTerminal::browseHistory(int direction) {
    if (direction == -1) { // UP
        historyViewIndex--;
        if (historyViewIndex < 0) historyViewIndex = MAX_HISTORY - 1;
    } else { // DOWN
        historyViewIndex++;
        if (historyViewIndex >= MAX_HISTORY) historyViewIndex = 0;
    }
    
    currentCommand = history[historyViewIndex];
    cursorPos = currentCommand.length();
    redrawInputLine();
}

void ESPTerminal::println(String text) {
    // If text is longer than columns, it should ideally wrap, 
    // but for a simple terminal, we just truncate or let it run off.
    screenBuffer[rows - 1] = text;
    scrollUp();
}

void ESPTerminal::scrollUp() {
    // Shift all lines up by 1
    for (int i = 0; i < rows - 1; i++) {
        screenBuffer[i] = screenBuffer[i + 1];
    }
    screenBuffer[rows - 1] = ""; // Clear the bottom line
    
    redrawScreen();
}

void ESPTerminal::clearScreen() {
    for(int i = 0; i < rows; i++) screenBuffer[i] = "";
    tft->fillScreen(ST77XX_BLACK);
    cursorPos = 0;
    cursorY = 0;
}

void ESPTerminal::printPrompt() {
    cursorY = rows - 1;
    redrawInputLine();
}

void ESPTerminal::redrawScreen() {
    tft->fillScreen(ST77XX_BLACK);
    for (int i = 0; i < rows; i++) {
        tft->setCursor(0, i * charHeight);
        tft->print(screenBuffer[i]);
    }
    cursorY = rows - 1;
    redrawInputLine();
}

void ESPTerminal::redrawInputLine() {
    // Clear the line
    tft->fillRect(0, cursorY * charHeight, cols * charWidth, charHeight, ST77XX_BLACK);
    // Print prompt
    tft->setCursor(0, cursorY * charHeight);
    tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft->print("> ");
    // Print current command
    tft->print(currentCommand);
}

void ESPTerminal::drawCursor(bool show) {
    // Cursor X position on screen
    int cx = (2 + cursorPos) * charWidth;
    int cy = cursorY * charHeight;
    
    if (show) {
        // Draw a block cursor
        tft->fillRect(cx, cy, charWidth, charHeight, ST77XX_WHITE);
        // Draw the character under the cursor inverted if there is one
        if (cursorPos < currentCommand.length()) {
            tft->setCursor(cx, cy);
            tft->setTextColor(ST77XX_BLACK, ST77XX_WHITE);
            tft->print(currentCommand[cursorPos]);
            tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK); // Reset to normal
        }
    } else {
        // Clear the cursor block
        tft->fillRect(cx, cy, charWidth, charHeight, ST77XX_BLACK);
        // Redraw the character normally if there is one
        if (cursorPos < currentCommand.length()) {
            tft->setCursor(cx, cy);
            tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);
            tft->print(currentCommand[cursorPos]);
        }
    }
}
