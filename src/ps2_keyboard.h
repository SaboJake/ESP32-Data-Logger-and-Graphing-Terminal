#ifndef PS2_KEYBOARD_H
#define PS2_KEYBOARD_H

#include <Arduino.h>

class PS2Keyboard {
public:
    PS2Keyboard(uint8_t clockPin, uint8_t dataPin);
    
    void begin();
    bool available();
    char read();
    void IRAM_ATTR handleInterrupt();
    bool isCtrl() const { return _isCtrl; }
    uint8_t getLastScancode(); // DEBUG

private:
    uint8_t _clockPin;
    uint8_t _dataPin;

    volatile uint8_t _bitCount;
    volatile uint8_t _dataByte;
    
    static const int BUFFER_SIZE = 64;
    volatile uint8_t _rawBuffer[BUFFER_SIZE];
    volatile uint8_t _head;
    volatile uint8_t _tail;
    volatile uint32_t _lastInterruptTime;

    // State machine variables
    bool _isUp;
    bool _isShifted;
    bool _isExtended;
    bool _isCtrl;
    bool _capsLock; // Track Caps Lock state
    char _nextChar;

    // Internal methods
    void processRawBytes();
    
    // Bi-directional PS/2 methods
    bool write(uint8_t data);
    void updateLEDs();
};

#endif // PS2_KEYBOARD_H
