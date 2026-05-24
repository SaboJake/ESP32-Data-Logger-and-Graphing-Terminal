#include "graph.h"
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <EEPROM.h>

AppState current_state = STATE_INITIAL;

static Adafruit_BMP085 bmp;
static bool bmp_found = false;

static lv_obj_t * chart;
static lv_chart_series_t * ser;
static lv_obj_t * scale_y;
static lv_obj_t * scale_x;
static bool show_labels = true;

LV_FONT_DECLARE(lv_font_unscii_16);

#define MAX_SAMPLES 250 // Fits in 4KB EEPROM (250 * 16 bytes = 4000)

struct SensorSnapshot {
  uint32_t timestamp;  // 4 bytes (Time since boot)
  uint16_t potValue;   // 2 bytes (0-4095)
  uint16_t lightValue; // 2 bytes (0-4095)
  float temperature;   // 4 bytes
  float pressure;      // 4 bytes
};

static SensorSnapshot data_buffer[MAX_SAMPLES];

static int active_sensor = 0; // 0=pot, 1=light, 2=temp, 3=pres
static int sample_rate = 1; // Hz
static int record_time = 10; // seconds
static int sample_count = 0;

static int scale_x_window = 30; // seconds
static int scale_y_min = 0;
static int scale_y_max = 4096;
static int pan_offset = 0;

static int orig_scale_x_window = 30;
static int orig_scale_y_min = 0;
static int orig_scale_y_max = 4096;

static uint32_t last_sample_time = 0;
static lv_timer_t * recording_timer;

static void update_chart_source() {
    int visible_points = scale_x_window * sample_rate;
    if (visible_points < 2) visible_points = 2;

    if (current_state == STATE_RUNNING) {
        pan_offset = sample_count - visible_points;
        if (pan_offset < 0) pan_offset = 0;
    }
    
    if (pan_offset > MAX_SAMPLES - visible_points) {
        pan_offset = MAX_SAMPLES - visible_points;
    }
    if (pan_offset < 0) pan_offset = 0;

    lv_chart_set_point_count(chart, visible_points);
    
    for(int i = 0; i < visible_points; i++) {
        int idx = pan_offset + i;
        int32_t val = 0;
        if (idx < sample_count) {
            if (active_sensor == 0) val = data_buffer[idx].potValue;
            else if (active_sensor == 1) val = data_buffer[idx].lightValue;
            else if (active_sensor == 2) val = (int32_t)data_buffer[idx].temperature;
            else if (active_sensor == 3) val = (int32_t)data_buffer[idx].pressure;
        }
        lv_chart_set_value_by_id(chart, ser, i, val);
    }
    
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, scale_y_min, scale_y_max);
    lv_scale_set_range(scale_y, scale_y_min, scale_y_max);

    int start_time = pan_offset / sample_rate;
    int end_time = (pan_offset + visible_points) / sample_rate;
    lv_scale_set_range(scale_x, start_time, end_time);

    lv_chart_refresh(chart);
}

static void recording_task(lv_timer_t * timer) {
    if (current_state != STATE_RUNNING) return;

    uint32_t now = millis();
    if (now - last_sample_time >= (1000 / sample_rate)) {
        last_sample_time = now;
        
        if (sample_count < (sample_rate * record_time) && sample_count < MAX_SAMPLES) {
            // Read sensors
            data_buffer[sample_count].timestamp = now;
            data_buffer[sample_count].potValue = analogRead(34);
            data_buffer[sample_count].lightValue = analogRead(35);
            if (bmp_found) {
                data_buffer[sample_count].temperature = bmp.readTemperature();
                data_buffer[sample_count].pressure = bmp.readPressure();
            } else {
                data_buffer[sample_count].temperature = 0.0f;
                data_buffer[sample_count].pressure = 0.0f;
            }
            
            sample_count++;
            
            // update chart
            update_chart_source();
            
            if (sample_count >= (sample_rate * record_time) || sample_count >= MAX_SAMPLES) {
                current_state = STATE_INSPECT;
                Serial.println("Recording Finished -> INSPECT STATE");
            }
        }
    }
}

void init_graph(lv_obj_t * parent) {
    Wire.begin(21, 22);
    if (bmp.begin()) {
        bmp_found = true;
    } else {
        Serial.println("BMP180 NOT FOUND!");
    }
    
    EEPROM.begin(4096);

    chart = lv_chart_create(parent);
    lv_obj_set_size(chart, 240, 205);
    lv_obj_align(chart, LV_ALIGN_TOP_RIGHT, -20, 5);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    
    // Retro style
    lv_obj_set_style_bg_color(chart, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(chart, 0, 0);
    lv_obj_set_style_line_color(chart, lv_color_hex(0x004400), LV_PART_MAIN); // grid
    
    // Series line style
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR); // Hide dots
    
    ser = lv_chart_add_series(chart, lv_color_hex(0x00FF00), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 4096); // Default 12-bit ADC range
    
    // Y-Axis Scale
    scale_y = lv_scale_create(parent);
    lv_scale_set_mode(scale_y, LV_SCALE_MODE_VERTICAL_LEFT);
    lv_scale_set_total_tick_count(scale_y, 11);
    lv_scale_set_major_tick_every(scale_y, 5);
    lv_obj_set_size(scale_y, 60, 205);
    lv_obj_align_to(scale_y, chart, LV_ALIGN_OUT_LEFT_MID, 0, 0);
    lv_obj_set_style_text_font(scale_y, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(scale_y, lv_color_hex(0x00AA00), 0);
    lv_obj_set_style_line_color(scale_y, lv_color_hex(0x00AA00), LV_PART_MAIN);
    lv_obj_set_style_line_color(scale_y, lv_color_hex(0x00AA00), LV_PART_INDICATOR);
    lv_obj_set_style_line_color(scale_y, lv_color_hex(0x00AA00), LV_PART_ITEMS);

    // X-Axis Scale
    scale_x = lv_scale_create(parent);
    lv_scale_set_mode(scale_x, LV_SCALE_MODE_HORIZONTAL_BOTTOM);
    lv_scale_set_total_tick_count(scale_x, 11);
    lv_scale_set_major_tick_every(scale_x, 5);
    lv_obj_set_size(scale_x, 240, 30);
    lv_obj_align_to(scale_x, chart, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(scale_x, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(scale_x, lv_color_hex(0x00AA00), 0);
    lv_obj_set_style_line_color(scale_x, lv_color_hex(0x00AA00), LV_PART_MAIN);
    lv_obj_set_style_line_color(scale_x, lv_color_hex(0x00AA00), LV_PART_INDICATOR);
    lv_obj_set_style_line_color(scale_x, lv_color_hex(0x00AA00), LV_PART_ITEMS);
    
    recording_timer = lv_timer_create(recording_task, 10, NULL);
}

void set_active_sensor(int sensor_idx) {
    active_sensor = sensor_idx % 4;
    update_chart_source();
}

void set_scale_x(int time_window) {
    scale_x_window = time_window;
    if (scale_x_window < 1) scale_x_window = 1;
    update_chart_source();
}

void set_scale_y(int min_y, int max_y) {
    scale_y_min = min_y;
    scale_y_max = max_y;
    update_chart_source();
}

void set_rate(int rate) {
    if (current_state == STATE_INITIAL || current_state == STATE_INSPECT) {
        sample_rate = rate;
        if (sample_rate < 1) sample_rate = 1;
    }
}

void set_time(int time_sec) {
    if (current_state == STATE_INITIAL || current_state == STATE_INSPECT) {
        record_time = time_sec;
    }
}

void cmd_start_recording() {
    if (current_state == STATE_INITIAL || current_state == STATE_INSPECT) {
        sample_count = 0;
        current_state = STATE_RUNNING;
        last_sample_time = millis();
        // Clear data arrays
        memset(data_buffer, 0, sizeof(data_buffer));
        
        orig_scale_x_window = scale_x_window;
        orig_scale_y_min = scale_y_min;
        orig_scale_y_max = scale_y_max;
        
        pan_offset = 0;
        update_chart_source();
    }
}

void cmd_stop_recording() {
    if (current_state == STATE_RUNNING) {
        current_state = STATE_INSPECT;
    }
}

void cmd_zoom(int zoom_amount) {
    if (current_state == STATE_INSPECT) {
        // Zoom amount is mapped to time window in seconds
        set_scale_x(zoom_amount);
    }
}

void cmd_zoom_in() {
    if (current_state == STATE_INSPECT) {
        int diff = scale_x_window / 4;
        if (diff < 1) diff = 1;
        set_scale_x(scale_x_window - diff);
    }
}

void cmd_zoom_out() {
    if (current_state == STATE_INSPECT) {
        int diff = scale_x_window / 4;
        if (diff < 1) diff = 1;
        set_scale_x(scale_x_window + diff);
    }
}

void cmd_zoom_reset() {
    if (current_state == STATE_INSPECT) {
        scale_x_window = orig_scale_x_window;
        scale_y_min = orig_scale_y_min;
        scale_y_max = orig_scale_y_max;
        pan_offset = 0;
        update_chart_source();
    }
}

void cmd_zoom_y_in() {
    if (current_state == STATE_INSPECT) {
        int range = scale_y_max - scale_y_min;
        int step = range / 8;
        if (step < 1) step = 1;
        set_scale_y(scale_y_min + step, scale_y_max - step);
    }
}

void cmd_zoom_y_out() {
    if (current_state == STATE_INSPECT) {
        int range = scale_y_max - scale_y_min;
        int step = range / 8;
        if (step < 1) step = 1;
        set_scale_y(scale_y_min - step, scale_y_max + step);
    }
}

void cmd_pan(int pan_amount) {
    if (current_state == STATE_INSPECT) {
        pan_offset += pan_amount;
        update_chart_source();
    }
}

void cmd_pan_y(int pan_amount) {
    if (current_state == STATE_INSPECT) {
        set_scale_y(scale_y_min + pan_amount, scale_y_max + pan_amount);
    }
}

void cmd_pan_relative(int direction) {
    if (current_state == STATE_INSPECT) {
        int visible_points = scale_x_window * sample_rate;
        if (visible_points < 2) visible_points = 2;
        int step = visible_points / 10;
        if (step < 1) step = 1;
        cmd_pan(direction * step);
    }
}

void cmd_pan_y_relative(int direction) {
    if (current_state == STATE_INSPECT) {
        int range = scale_y_max - scale_y_min;
        if (range < 0) range = -range;
        int step = range / 10;
        if (step < 1) step = 1;
        cmd_pan_y(direction * step);
    }
}

void cmd_toggle_labels() {
    show_labels = !show_labels;
    if (show_labels) {
        lv_obj_remove_flag(scale_x, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(scale_y, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(chart, 240, 205);
        lv_obj_align(chart, LV_ALIGN_TOP_RIGHT, -20, 5);
    } else {
        lv_obj_add_flag(scale_x, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(scale_y, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(chart, 320, 240);
        lv_obj_align(chart, LV_ALIGN_CENTER, 0, 0);
    }
}

void cmd_save(const char* slot) {
    if (current_state == STATE_INSPECT || current_state == STATE_INITIAL) {
        EEPROM.put(0, sample_count);
        EEPROM.put(sizeof(int), sample_rate);
        int offset = sizeof(int) * 2;
        
        EEPROM.put(offset, data_buffer);
        EEPROM.commit();
    }
}

void cmd_load(const char* slot) {
    if (current_state == STATE_INSPECT || current_state == STATE_INITIAL) {
        EEPROM.get(0, sample_count);
        EEPROM.get(sizeof(int), sample_rate);
        int offset = sizeof(int) * 2;
        
        if (sample_count > MAX_SAMPLES || sample_count < 0) sample_count = 0;
        
        EEPROM.get(offset, data_buffer);
        
        current_state = STATE_INSPECT;
        update_chart_source();
    }
}
