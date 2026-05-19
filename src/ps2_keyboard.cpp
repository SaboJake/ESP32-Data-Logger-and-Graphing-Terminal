#include "ps2_keyboard.h"

// --- Global ISR Wrapper ---
static void IRAM_ATTR ps2_isr_wrapper(void* arg) {
    static_cast<PS2Keyboard*>(arg)->handleInterrupt();
}

static volatile uint8_t _lastScancode = 0;

// --- PS/2 Scancode to ASCII Maps (Standard Set 2) ---
// (Keep your exact same unshiftedMap and shiftedMap arrays here)
static const char unshiftedMap[128] = {
  0,0,0,0,0,0,0,0,0,'\t',0,0,0,'\t','`',0,
  0,0,0,0,0,'q','1',0,0,0,'z','s','a','w','2',0,
  0,'c','x','d','e','4','3',0,0,' ','v','f','t','r','5',0,
  0,'n','b','h','g','y','6',0,0,0,'m','j','u','7','8',0,
  0,',','k','i','o','0','9',0,0,'.','/','l',';','p','-',0,
  0,0,'\'',0,'[','=',0,0,0,0,'\n',']',0,'\\',0,0,
  0,0,0,0,0,0,'\b',0,0,'1',0,'4','7',0,0,0,
  '0','.','2','5','6','8',0,0,0,'+','3','-','*','9',0,0
};

static const char shiftedMap[128] = {
  0,0,0,0,0,0,0,0,0,'\t',0,0,0,'\t','~',0,
  0,0,0,0,0,'Q','!',0,0,0,'Z','S','A','W','@',0,
  0,'C','X','D','E','$','#',0,0,' ','V','F','T','R','%',0,
  0,'N','B','H','G','Y','^',0,0,0,'M','J','U','&','*',0,
  0,'<','K','I','O',')','(',0,0,'>','?','L',':','P','_',0,
  0,0,'\"',0,'{','+',0,0,0,0,'\n','}',0,'|',0,0,
  0,0,0,0,0,0,'\b',0,0,'1',0,'4','7',0,0,0,
  '0','.','2','5','6','8',0,0,0,'+','3','-','*','9',0,0
};


PS2Keyboard::PS2Keyboard(uint8_t clockPin, uint8_t dataPin) {
    _clockPin = clockPin;
    _dataPin = dataPin;
    _bitCount = 0;
    _dataByte = 0;
    _head = 0;
    _tail = 0;
    
    _isUp = false;
    _isShifted = false;
    _isExtended = false;
    _isCtrl = false;
    _capsLock = false;
    _nextChar = 0;
}

void PS2Keyboard::begin() {
    pinMode(_clockPin, INPUT_PULLUP);
    pinMode(_dataPin, INPUT_PULLUP);
    attachInterruptArg(digitalPinToInterrupt(_clockPin), ps2_isr_wrapper, this, FALLING);
}

uint8_t PS2Keyboard::getLastScancode() {
    uint8_t sc = _lastScancode;
    _lastScancode = 0;
    return sc;
}

void IRAM_ATTR PS2Keyboard::handleInterrupt() {
    uint32_t now = micros();
    
    // TIMEOUT FIX: PS/2 clock runs at 10-16kHz (~60-100us per pulse).
    // If more than 2.5ms (2500us) have passed, reset the sequence.
    // This instantly recovers from missed bits caused by fast typing or screen updates.
    if (now - _lastInterruptTime > 2500) {
        _bitCount = 0;
        _dataByte = 0;
    }
    _lastInterruptTime = now;

    int val = digitalRead(_dataPin);
    
    // Bit 0 is the Start bit, which must always be 0 (LOW)
    if (_bitCount == 0 && val != 0) {
        return; 
    }
    
    // Bits 1-8 are the data payload
    if (_bitCount > 0 && _bitCount < 9) {
        _dataByte = _dataByte | (val << (_bitCount - 1));
    }
    
    _bitCount++;
    
    // Bit 10 is the Stop bit, which must always be 1 (HIGH)
    // (Bit 9 is parity, which we are skipping for simplicity)
    if (_bitCount == 11) {
        if (val == 1) { 
            uint8_t nextHead = (_head + 1) % BUFFER_SIZE;
            if (nextHead != _tail) {
                _rawBuffer[_head] = _dataByte;
                _head = nextHead;
            }
        }
        _dataByte = 0;
        _bitCount = 0;
    }
}

// --- HOST TO KEYBOARD TRANSMISSION ---
bool PS2Keyboard::write(uint8_t data) {
    uint8_t parity = 1;

    // 1. Disable interrupts while we take over the lines
    detachInterrupt(digitalPinToInterrupt(_clockPin));

    // 2. Inhibit communication (Pull clock low for >100us)
    pinMode(_clockPin, OUTPUT);
    digitalWrite(_clockPin, LOW);
    delayMicroseconds(120);

    // 3. Request to Send (Pull data low, release clock)
    pinMode(_dataPin, OUTPUT);
    digitalWrite(_dataPin, LOW);
    pinMode(_clockPin, INPUT_PULLUP);

    // Wait for keyboard to start generating clock pulses
    int timeout = 10000;
    while (digitalRead(_clockPin) == HIGH && timeout--) { delayMicroseconds(1); }
    if (timeout <= 0) goto reset_bus;

    // 4. Send 8 Data Bits
    for (int i = 0; i < 8; i++) {
        if (data & 1) digitalWrite(_dataPin, HIGH);
        else { digitalWrite(_dataPin, LOW); parity++; } // Calculate Odd Parity
        
        // Wait for clock high then low
        while (digitalRead(_clockPin) == LOW);
        while (digitalRead(_clockPin) == HIGH);
        data >>= 1;
    }

    // 5. Send Parity Bit
    if (parity & 1) digitalWrite(_dataPin, HIGH);
    else digitalWrite(_dataPin, LOW);
    while (digitalRead(_clockPin) == LOW);
    while (digitalRead(_clockPin) == HIGH);

    // 6. Send Stop Bit (High) and Release Data line
    pinMode(_dataPin, INPUT_PULLUP);
    while (digitalRead(_clockPin) == LOW);
    while (digitalRead(_clockPin) == HIGH);

    // 7. Wait for ACK from keyboard (Keyboard pulls Data low)
    while (digitalRead(_dataPin) == HIGH);
    while (digitalRead(_clockPin) == LOW);

reset_bus:
    // Restore bus to normal listening mode
    pinMode(_clockPin, INPUT_PULLUP);
    pinMode(_dataPin, INPUT_PULLUP);
    _bitCount = 0; // Reset ISR state
    attachInterruptArg(digitalPinToInterrupt(_clockPin), ps2_isr_wrapper, this, FALLING);
    
    return (timeout > 0);
}

void PS2Keyboard::updateLEDs() {
    // LED state byte: Bit 0=Scroll, Bit 1=Num, Bit 2=Caps
    uint8_t ledState = 0;
    if (_capsLock) ledState |= (1 << 2); 

    // Command 0xED tells the keyboard to update LEDs
    if (write(0xED)) {
        delay(10); // Give keyboard a tiny moment to process
        write(ledState); // Send the actual LED states
    }
}

void PS2Keyboard::processRawBytes() {
    while (_head != _tail && _nextChar == 0) {
        uint8_t code = _rawBuffer[_tail];
        _tail = (_tail + 1) % BUFFER_SIZE;

        if (code == 0xF0) {
            _isUp = true;
        } else if (code == 0xE0) {
            _isExtended = true;
        } else {
            if (_isUp) {
                // Key Released
                if (code == 0x12 || code == 0x59) { 
                    _isShifted = false;
                } else if (code == 0x14) {
                    _isCtrl = false;
                }
                _isUp = false;
                _isExtended = false;
            } else {
                // Key Pressed
                _lastScancode = code; // Store for debugging
                if (code == 0x12 || code == 0x59) {
                    _isShifted = true;
                } else if (code == 0x14) {
                    _isCtrl = true;
                } else if (code == 0x58) { 
                    // CAPS LOCK TOGGLE
                    _capsLock = !_capsLock;
                    updateLEDs();
                } else if (code < 128) {
                    char c = 0;
                    
                    // --- THE FIX IS HERE ---
                    if (code == 0x76) {
                        c = 27; // ESC key forced
                    } else if (code == 0x75) {
                        c = 129; // Up
                    } else if (code == 0x72) {
                        c = 130; // Down
                    } else if (code == 0x6B) {
                        c = 131; // Left
                    } else if (code == 0x74) {
                        c = 132; // Right
                    } else {
                        // Standard characters
                        c = _isShifted ? shiftedMap[code] : unshiftedMap[code];
                        
                        if (_capsLock) {
                            if (c >= 'a' && c <= 'z') c -= 32;
                            else if (c >= 'A' && c <= 'Z') c += 32;
                        }
                    }
                    
                    if (c != 0) {
                        _nextChar = c;
                    }
                }
                _isExtended = false; // Reset extended flag after processing
            }
        }
    }
}

bool PS2Keyboard::available() {
    processRawBytes();
    return _nextChar != 0;
}

char PS2Keyboard::read() {
    char c = _nextChar;
    _nextChar = 0;
    return c;
}
