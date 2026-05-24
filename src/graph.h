#ifndef GRAPH_H
#define GRAPH_H

#include "lvgl.h"

enum AppState {
    STATE_INITIAL,
    STATE_RUNNING,
    STATE_INSPECT
};

extern AppState current_state;

void init_graph(lv_obj_t * parent);
void set_active_sensor(int sensor_idx); // 0=pot, 1=light, 2=temp, 3=pres
void set_scale_x(int time_window);
void set_scale_y(int min_y, int max_y);
void set_rate(int rate);
void set_time(int time_sec);
void cmd_start_recording();
void cmd_stop_recording();
void cmd_zoom(int zoom_amount); // 256 = 100%
void cmd_zoom_in();
void cmd_zoom_out();
void cmd_zoom_reset();
void cmd_zoom_y_in();
void cmd_zoom_y_out();
void cmd_pan(int pan_amount);
void cmd_pan_y(int pan_amount);
void cmd_pan_relative(int direction);
void cmd_pan_y_relative(int direction);
void cmd_toggle_labels();
void cmd_save(const char* slot);
void cmd_load(const char* slot);

#endif // GRAPH_H
