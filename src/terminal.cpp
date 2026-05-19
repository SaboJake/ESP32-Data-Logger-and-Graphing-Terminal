#include "terminal.h"
#include "graph.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

LV_FONT_DECLARE(lv_font_unscii_16);

#define MAX_HISTORY 8
static char command_history[MAX_HISTORY][64];
static int history_count = 0;         
static int history_browse_idx = -1;   
static char current_draft[64] = "";   

#define MAX_LOG_LINES 32
static char terminal_logs[MAX_LOG_LINES][64] = {0};
static int log_line_count = 0;
static int scroll_offset = 0;

static lv_obj_t * terminal_log_label; 
static lv_obj_t * terminal_container;
static bool is_terminal_open = false;
static lv_obj_t * terminal_ta; 

static int term_screen_width = 320;
static int term_height = 120;

static void print_to_terminal(const char* text) {
    if (log_line_count < MAX_LOG_LINES) {
        strncpy(terminal_logs[log_line_count], text, 63);
        log_line_count++;
    } else {
        for (int i = 0; i < MAX_LOG_LINES - 1; i++) {
            strcpy(terminal_logs[i], terminal_logs[i + 1]);
        }
        strncpy(terminal_logs[MAX_LOG_LINES - 1], text, 63);
    }

    static char combined_buffer[2500];
    combined_buffer[0] = '\0';
    for (int i = 0; i < log_line_count; i++) {
        strcat(combined_buffer, "> ");
        strcat(combined_buffer, terminal_logs[i]);
        if (i < log_line_count - 1) {
            strcat(combined_buffer, "\n");
        }
    }

    lv_label_set_text(terminal_log_label, combined_buffer);
    scroll_offset = 0;
    lv_obj_align(terminal_log_label, LV_ALIGN_BOTTOM_LEFT, 6, -2);
}

static void push_to_history(const char* cmd) {
    if (strlen(cmd) == 0) return;

    if (history_count > 0 && strcmp(command_history[0], cmd) == 0) return;

    for (int i = MAX_HISTORY - 1; i > 0; i--) {
        strcpy(command_history[i], command_history[i - 1]);
    }
    
    strncpy(command_history[0], cmd, 63);
    
    if (history_count < MAX_HISTORY) {
        history_count++;
    }
}

static void parse_command(const char* cmd) {
    char cmd_copy[128];
    strncpy(cmd_copy, cmd, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';

    char* token = strtok(cmd_copy, " \t");
    if (!token) return;

    if (strcmp(token, "graph") == 0) {
        char* sensor = strtok(NULL, " \t");
        if (sensor) {
            int idx = -1;
            if (strcmp(sensor, "pot") == 0) idx = 0;
            else if (strcmp(sensor, "light") == 0) idx = 1;
            else if (strcmp(sensor, "temp") == 0) idx = 2;
            else if (strcmp(sensor, "pres") == 0) idx = 3;
            
            if (idx >= 0) {
                set_active_sensor(idx);
                char msg[64];
                snprintf(msg, sizeof(msg), "Graph sensor set to: %s", sensor);
                print_to_terminal(msg);
            } else {
                print_to_terminal("Error: invalid sensor");
            }
        } else {
            print_to_terminal("Error: graph requires <sensor>");
        }
    } else if (strcmp(token, "scale") == 0) {
        char* axis = strtok(NULL, " \t");
        if (axis) {
            if (strcmp(axis, "x") == 0) {
                char* window = strtok(NULL, " \t");
                if (window) {
                    set_scale_x(atoi(window));
                    char msg[64];
                    snprintf(msg, sizeof(msg), "Scale X set to: %s s", window);
                    print_to_terminal(msg);
                } else {
                    print_to_terminal("Error: scale x requires <time_window>");
                }
            } else if (strcmp(axis, "y") == 0) {
                char* min_y = strtok(NULL, " \t");
                char* max_y = strtok(NULL, " \t");
                if (min_y && max_y) {
                    set_scale_y(atoi(min_y), atoi(max_y));
                    char msg[64];
                    snprintf(msg, sizeof(msg), "Scale Y set to Min:%s Max:%s", min_y, max_y);
                    print_to_terminal(msg);
                } else {
                    print_to_terminal("Error: scale y requires <min> <max>");
                }
            } else {
                print_to_terminal("Error: scale requires 'x' or 'y'");
            }
        } else {
            print_to_terminal("Error: scale requires 'x' or 'y'");
        }
    } else if (strcmp(token, "rate") == 0) {
        char* x = strtok(NULL, " \t");
        if (x) {
            set_rate(atoi(x));
            char msg[64];
            snprintf(msg, sizeof(msg), "Sampling rate set to: %s", x);
            print_to_terminal(msg);
        } else {
            print_to_terminal("Error: rate requires <x>");
        }
    } else if (strcmp(token, "time") == 0) {
        char* x = strtok(NULL, " \t");
        if (x) {
            set_time(atoi(x));
            char msg[64];
            snprintf(msg, sizeof(msg), "Recording time set to: %s", x);
            print_to_terminal(msg);
        } else {
            print_to_terminal("Error: time requires <x>");
        }
    } else if (strcmp(token, "start") == 0) {
        cmd_start_recording();
        print_to_terminal("Recording started.");
    } else if (strcmp(token, "stop") == 0) {
        cmd_stop_recording();
        print_to_terminal("Recording forcefully stopped.");
    } else if (strcmp(token, "save") == 0) {
        char* file = strtok(NULL, " \t");
        if (file) {
            cmd_save(file);
            char msg[64];
            snprintf(msg, sizeof(msg), "Data saved to EEPROM slot: %s", file);
            print_to_terminal(msg);
        } else {
            print_to_terminal("Error: save requires <file>");
        }
    } else if (strcmp(token, "load") == 0) {
        char* file = strtok(NULL, " \t");
        if (file) {
            cmd_load(file);
            char msg[64];
            snprintf(msg, sizeof(msg), "Data loaded from EEPROM slot: %s", file);
            print_to_terminal(msg);
        } else {
            print_to_terminal("Error: load requires <file>");
        }
    } else if (strcmp(token, "zoom") == 0) {
        char* x = strtok(NULL, " \t");
        if (x) {
            if (strcmp(x, "in") == 0) {
                cmd_zoom_in();
                print_to_terminal("Graph zoomed in.");
            } else if (strcmp(x, "out") == 0) {
                cmd_zoom_out();
                print_to_terminal("Graph zoomed out.");
            } else if (strcmp(x, "reset") == 0) {
                cmd_zoom_reset();
                print_to_terminal("Graph zoom and pan reset.");
            } else if (strcmp(x, "y") == 0) {
                char* y_dir = strtok(NULL, " \t");
                if (y_dir && strcmp(y_dir, "in") == 0) {
                    cmd_zoom_y_in();
                    print_to_terminal("Graph Y-axis zoomed in.");
                } else if (y_dir && strcmp(y_dir, "out") == 0) {
                    cmd_zoom_y_out();
                    print_to_terminal("Graph Y-axis zoomed out.");
                } else {
                    print_to_terminal("Error: zoom y requires in/out");
                }
            } else {
                cmd_zoom(atoi(x));
                char msg[64];
                snprintf(msg, sizeof(msg), "Graph zoom set to: %s", x);
                print_to_terminal(msg);
            }
        } else {
            print_to_terminal("Error: zoom requires <x> or in/out");
        }
    } else if (strcmp(token, "pan") == 0) {
        char* amount = strtok(NULL, " \t");
        if (amount) {
            if (strcmp(amount, "y") == 0) {
                char* y_val = strtok(NULL, " \t");
                if (y_val) {
                    cmd_pan_y(atoi(y_val));
                    char msg[64];
                    snprintf(msg, sizeof(msg), "Graph Y panned by: %s", y_val);
                    print_to_terminal(msg);
                } else {
                    print_to_terminal("Error: pan y requires <amount>");
                }
            } else {
                cmd_pan(atoi(amount));
                char msg[64];
                snprintf(msg, sizeof(msg), "Graph panned by: %s", amount);
                print_to_terminal(msg);
            }
        } else {
            print_to_terminal("Error: pan requires <amount>");
        }
    } else if (strcmp(token, "labels") == 0) {
        cmd_toggle_labels();
        print_to_terminal("Toggled labels.");
    } else if (strcmp(token, "exit") == 0) {
        print_to_terminal("Exiting terminal.");
        toggle_terminal();
    } else if (strcmp(token, "clear") == 0) {
        log_line_count = 0;
        lv_label_set_text(terminal_log_label, "");
        scroll_offset = 0;
        lv_obj_align(terminal_log_label, LV_ALIGN_BOTTOM_LEFT, 6, -2);
    } else if (strcmp(token, "hotkeys") == 0) {
        print_to_terminal("--- System Hotkeys ---");
        print_to_terminal("[ ` ] or [ ~ ] : Toggle terminal");
        print_to_terminal("[ TAB ]        : Toggle menu");
        print_to_terminal("[ L ]          : Toggle graph labels");
        print_to_terminal("[ Arrows ]     : Pan graph (X/Y)");
        print_to_terminal("[ Ctrl+Arrows ]: Zoom graph (X/Y)");
        print_to_terminal("[ Ctrl+Up/Down]: Scroll terminal");
    } else if (strcmp(token, "help") == 0) {
        print_to_terminal("Commands: graph, scale, rate, time, start");
        print_to_terminal("stop, save, load, zoom, pan, labels");
        print_to_terminal("hotkeys, exit, clear");
    } else {
        print_to_terminal("Unknown command. Type 'help' for list.");
    }
}

static void terminal_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = (lv_obj_t *)lv_event_get_target(e);

    if (code == LV_EVENT_READY) { 
        const char * text = lv_textarea_get_text(ta);
        
        if (strlen(text) > 0) {
            print_to_terminal(text);
            push_to_history(text);
            parse_command(text);
        }
        
        lv_textarea_set_text(ta, ""); 
        history_browse_idx = -1;
        current_draft[0] = '\0';
    }
    else if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
        if (key == LV_KEY_UP) {
            if (history_count == 0) return; 

            if (history_browse_idx == -1) {
                strncpy(current_draft, lv_textarea_get_text(ta), 63);
            }

            history_browse_idx++;
            if (history_browse_idx >= history_count) {
                history_browse_idx = history_count - 1; 
            }

            lv_textarea_set_text(ta, command_history[history_browse_idx]);
            lv_textarea_set_cursor_pos(ta, LV_TEXTAREA_CURSOR_LAST);
        }
        else if (key == LV_KEY_DOWN) {
            if (history_browse_idx == -1) return; 

            history_browse_idx--;
            if (history_browse_idx == -1) {
                lv_textarea_set_text(ta, current_draft);
            } else {
                lv_textarea_set_text(ta, command_history[history_browse_idx]);
            }
            lv_textarea_set_cursor_pos(ta, LV_TEXTAREA_CURSOR_LAST);
        }
        else if (key == 133) { 
            lv_obj_update_layout(terminal_log_label);
            int label_h = lv_obj_get_height(terminal_log_label);
            int visible_h = term_height - 34;
            int max_scroll = label_h - visible_h + 10;
            if (max_scroll < 0) max_scroll = 0;
            scroll_offset += 20;
            if (scroll_offset > max_scroll) scroll_offset = max_scroll;
            lv_obj_align(terminal_log_label, LV_ALIGN_BOTTOM_LEFT, 6, -2 + scroll_offset);
        }
        else if (key == 134) { 
            scroll_offset -= 20;
            if (scroll_offset < 0) scroll_offset = 0;
            lv_obj_align(terminal_log_label, LV_ALIGN_BOTTOM_LEFT, 6, -2 + scroll_offset);
        }
    }
}

void init_terminal_ui(int screen_width, int screen_height) {
    term_screen_width = screen_width;
    term_height = screen_height / 2;

    terminal_container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(terminal_container, term_screen_width, term_height);
    lv_obj_set_pos(terminal_container, 0, -term_height);
    
    lv_obj_set_style_bg_color(terminal_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(terminal_container, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_border_width(terminal_container, 2, 0);
    lv_obj_set_style_radius(terminal_container, 0, 0); 
    lv_obj_remove_flag(terminal_container, LV_OBJ_FLAG_SCROLLABLE); 
    lv_obj_set_style_pad_all(terminal_container, 0, 0); 

    lv_obj_t * log_container = lv_obj_create(terminal_container);
    lv_obj_set_size(log_container, term_screen_width, term_height - 32);
    lv_obj_align(log_container, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(log_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(log_container, 0, 0);
    lv_obj_set_style_pad_all(log_container, 0, 0);
    lv_obj_remove_flag(log_container, LV_OBJ_FLAG_SCROLLABLE);

    terminal_log_label = lv_label_create(log_container);
    lv_obj_set_width(terminal_log_label, term_screen_width - 12); 
    lv_obj_align(terminal_log_label, LV_ALIGN_BOTTOM_LEFT, 6, -2); 
    lv_obj_set_style_text_font(terminal_log_label, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(terminal_log_label, lv_color_hex(0x00AA00), 0); 
    lv_label_set_text(terminal_log_label, ""); 

    terminal_ta = lv_textarea_create(terminal_container);
    lv_textarea_set_one_line(terminal_ta, true); 
    lv_obj_set_size(terminal_ta, LV_PCT(100), 32); 
    lv_obj_align(terminal_ta, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    
    lv_obj_set_scrollbar_mode(terminal_ta, LV_SCROLLBAR_MODE_OFF);

    uint32_t target_states[] = {LV_STATE_DEFAULT, LV_STATE_FOCUSED, LV_STATE_EDITED, LV_STATE_PRESSED};
    for(int i = 0; i < 4; i++) {
        lv_obj_set_style_bg_opa(terminal_ta, LV_OPA_TRANSP, target_states[i]); 
        lv_obj_set_style_border_width(terminal_ta, 0, target_states[i]);               
        lv_obj_set_style_outline_width(terminal_ta, 0, target_states[i]); 
        lv_obj_set_style_shadow_width(terminal_ta, 0, target_states[i]);
        lv_obj_set_style_radius(terminal_ta, 0, target_states[i]);
    }
    
    lv_obj_set_style_pad_top(terminal_ta, 8, 0);
    lv_obj_set_style_pad_bottom(terminal_ta, 6, 0);
    lv_obj_set_style_pad_left(terminal_ta, 6, 0); 
    lv_obj_set_style_pad_right(terminal_ta, 6, 0);

    lv_obj_set_style_text_font(terminal_ta, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(terminal_ta, lv_color_hex(0x00FF00), 0); 
    
    lv_textarea_set_cursor_click_pos(terminal_ta, false); 
    lv_obj_set_style_bg_color(terminal_ta, lv_color_hex(0x00FF00), LV_PART_CURSOR);
    lv_obj_set_style_bg_opa(terminal_ta, LV_OPA_COVER, LV_PART_CURSOR); 
    lv_obj_set_style_text_color(terminal_ta, lv_color_hex(0x000000), LV_PART_CURSOR); 
    lv_obj_set_style_width(terminal_ta, 10, LV_PART_CURSOR); 
    lv_obj_set_style_anim_duration(terminal_ta, 500, LV_PART_CURSOR); 
    
    lv_textarea_set_placeholder_text(terminal_ta, "Enter command...");
    lv_obj_set_style_text_font(terminal_ta, &lv_font_unscii_16, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(terminal_ta, lv_color_hex(0x004400), LV_PART_INDICATOR); 
    
    lv_obj_add_event_cb(terminal_ta, terminal_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(terminal_ta, terminal_event_cb, LV_EVENT_KEY, NULL);
    
    // Starts closed, so disable input
    lv_obj_add_state(terminal_ta, LV_STATE_DISABLED);
}

void toggle_terminal() {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, terminal_container); 
    
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    
    lv_anim_set_time(&a, 250);
    
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);

    if (is_terminal_open) {
        lv_anim_set_values(&a, 0, -term_height);
        lv_obj_add_state(terminal_ta, LV_STATE_DISABLED);
    } else {
        lv_anim_set_values(&a, -term_height, 0);
        lv_obj_remove_state(terminal_ta, LV_STATE_DISABLED);
    }
    
    lv_anim_start(&a);
    
    is_terminal_open = !is_terminal_open;
}

bool is_terminal_active() {
    return is_terminal_open;
}
