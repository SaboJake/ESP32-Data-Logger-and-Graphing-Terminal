#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include "ps2_keyboard.h"
#include <FS.h>
#include <SD.h>


#include "terminal.h"
#include "menu.h"
#include "graph.h"

// Screen dimensions
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

const int SD_CS_PIN = 16;

TFT_eSPI tft = TFT_eSPI(SCREEN_WIDTH, SCREEN_HEIGHT);

//LVGL Display Buffer
#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4]; // 32-bit aligned array

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)px_map, w * h, true);
    tft.endWrite();

    lv_display_flush_ready(disp);
}

uint32_t custom_tick_get(void) {
    return millis();
}

PS2Keyboard kb(26, 27); 
uint32_t last_key = 0; // Remembers the last key pressed for LVGL's release state


void keypad_read_cb(lv_indev_t * indev, lv_indev_data_t * data) {
    if (kb.available()) {
        char c = kb.read();
        uint32_t lv_key = 0;

        if (c == '`' || c == '~') {
            toggle_terminal();
            
            // Tell LVGL this keystroke was "eaten" by our system
            data->key = 0;
            data->state = LV_INDEV_STATE_RELEASED; 
            return; 
        }

        if (c == '\t') {
            toggle_menu();
            Serial.printf("Opening menu!\n");
            data->key = 0;
            data->state = LV_INDEV_STATE_RELEASED;
            return;
        }

        if ((c == 'l' || c == 'L') && !is_terminal_active() && !is_menu_active()) {
            cmd_toggle_labels();
            data->key = 0;
            data->state = LV_INDEV_STATE_RELEASED;
            return;
        }

        if (c == 129) {
            if (kb.isCtrl()) {
                if (!is_terminal_active() && !is_menu_active()) cmd_zoom_y_in();
                else lv_key = 133;
            } else {
                lv_key = LV_KEY_UP;
                if (!is_terminal_active() && !is_menu_active()) cmd_pan_y_relative(1);
            }
        }
        else if (c == 130) {
            if (kb.isCtrl()) {
                if (!is_terminal_active() && !is_menu_active()) cmd_zoom_y_out();
                else lv_key = 134;
            } else {
                lv_key = LV_KEY_DOWN;
                if (!is_terminal_active() && !is_menu_active()) cmd_pan_y_relative(-1);
            }
        }
        else if (c == 131) {
            if (kb.isCtrl()) {
                if (!is_terminal_active() && !is_menu_active()) cmd_zoom_out();
            } else {
                lv_key = LV_KEY_LEFT;
                if (!is_terminal_active() && !is_menu_active()) cmd_pan_relative(-1);
            }
        }
        else if (c == 132) {
            if (kb.isCtrl()) {
                if (!is_terminal_active() && !is_menu_active()) cmd_zoom_in();
            } else {
                lv_key = LV_KEY_RIGHT;
                if (!is_terminal_active() && !is_menu_active()) cmd_pan_relative(1);
            }
        }
        
        else if (c == '\n' || c == '\r') lv_key = LV_KEY_ENTER;
        else if (c == '\b') lv_key = LV_KEY_BACKSPACE;
        else if (c == 27) {
            Serial.printf("Pressed ESC!\n");
            lv_key = LV_KEY_ESC;
        }
        
        else lv_key = c;

        data->key = lv_key;
        data->state = LV_INDEV_STATE_PRESSED;
        last_key = lv_key; // Save it so we can tell LVGL what key was released later
    } else {
        // If nothing is available, tell LVGL the last known key is currently released
        data->key = last_key;
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

SPIClass hspi(HSPI);

void setup() {
    Serial.begin(115200);
    
    // Explicitly configure Chip Selects to prevent bus collision during initialization
    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH); // Keep TFT screen deselected from SPI

    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH); // Keep SD card deselected from SPI

    pinMode(32, OUTPUT);
    digitalWrite(32, HIGH);

    // Initialize shared SPI bus pins (SCLK=18, MISO=19, MOSI=23, SS=16)
    SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
    delay(10); // Short delay to let signals settle

    hspi.begin(14, 12, 13, SD_CS_PIN);
    delay(10);

    // Initialize SD Card
    if (!SD.begin(SD_CS_PIN, hspi, 4000000)) {
        Serial.println("SD Card Mount Failed!");
    } else {
        Serial.println("SD Card Mount Successful.");
    }

    tft.begin();
    tft.setRotation(1);
    tft.invertDisplay(false);
    tft.fillScreen(TFT_BLACK);
    
    lv_init();
    lv_tick_set_cb(custom_tick_get);

    lv_display_t * disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    kb.begin(); 

    lv_indev_t * indev_keypad = lv_indev_create();
    lv_indev_set_type(indev_keypad, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev_keypad, keypad_read_cb);

    lv_group_t * g = lv_group_create();
    lv_group_set_default(g); 
    lv_indev_set_group(indev_keypad, g);

    lv_obj_t * screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), LV_PART_MAIN); 
    
    init_graph(screen);
    init_terminal_ui(SCREEN_WIDTH, SCREEN_HEIGHT);
    init_menu(indev_keypad);
    
    Serial.println("System fully booted. Tap backtick (`) to open terminal.");
}

void loop() {
    lv_timer_handler(); 

    uint8_t raw = kb.getLastScancode();
    if (raw != 0) {
        Serial.printf("Raw Scancode: 0x%02X\n", raw);
    }

    delay(5); // Small delay prevents the ESP32's watchdog timer from crashing the board
}
