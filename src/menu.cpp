#include "menu.h"
#include "graph.h"
#include <stdio.h>
#include <stdlib.h>

LV_FONT_DECLARE(lv_font_unscii_16);

static lv_obj_t * menu_screen;
static lv_group_t * menu_group;
static lv_group_t * submenu_group;
static lv_group_t * load_group;
static lv_indev_t * keypad_indev;
static lv_group_t * prev_group;

static bool is_menu_open = false;

static lv_obj_t * btn_start_rec;
static lv_obj_t * btn_save;
static lv_obj_t * btn_load;
static lv_obj_t * btn_exit;

static lv_obj_t * submenu_container;
static lv_obj_t * load_cont;
static lv_obj_t * main_cont;
static lv_obj_t * label_graph;
static int sensor_idx = 0;
static const char* sensors[] = {"pot", "light", "temp", "pres"};

static lv_obj_t * ta_scale_x;
static lv_obj_t * ta_scale_y_min;
static lv_obj_t * ta_scale_y_max;
static lv_obj_t * ta_rate;
static lv_obj_t * ta_time;
static lv_obj_t * btn_sub_start;
static lv_obj_t * btn_sub_cancel;

static void open_submenu();
static void close_submenu();
static void open_load_submenu();
static void close_load_submenu();

static void apply_retro_style(lv_obj_t * obj) {
    lv_obj_set_style_text_font(obj, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(0x00AA00), 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_border_width(obj, 2, 0);
    lv_obj_set_style_radius(obj, 0, 0);
}

static void apply_retro_btn_style(lv_obj_t * btn) {
    apply_retro_style(btn);
    // Focused state
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x00AA00), LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(btn, lv_color_hex(0x000000), LV_STATE_FOCUSED);
    
    // Pressed state
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x00FF00), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(btn, lv_color_hex(0x000000), LV_STATE_PRESSED);
}

static void close_menu() {
    lv_obj_add_flag(menu_screen, LV_OBJ_FLAG_HIDDEN);
    is_menu_open = false;
    lv_indev_set_group(keypad_indev, prev_group);
}

static void menu_event_cb(lv_event_t * e) {
    lv_obj_t * obj = (lv_obj_t *)lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        if (obj == btn_start_rec) open_submenu();
        else if (obj == btn_save) cmd_save("slot1");
        else if (obj == btn_load) cmd_load("slot1");
        else if (obj == btn_exit) close_menu();
    } else if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        if (key == LV_KEY_ESC) close_menu();
        else if (key == LV_KEY_UP) lv_group_focus_prev(menu_group);
        else if (key == LV_KEY_DOWN) lv_group_focus_next(menu_group);
    }
}

static void handle_ta_arrows(lv_obj_t * ta, uint32_t key) {
    if (!lv_obj_has_state(ta, LV_STATE_EDITED)) {
        if (key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
            const char * text = lv_textarea_get_text(ta);
            int val = atoi(text);
            if (key == LV_KEY_RIGHT) val++;
            else if (key == LV_KEY_LEFT) val--;
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", val);
            lv_textarea_set_text(ta, buf);
        }
    }
}

static void submenu_event_cb(lv_event_t * e) {
    lv_obj_t * obj = (lv_obj_t *)lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        if (obj == btn_sub_start) {
            int sx = atoi(lv_textarea_get_text(ta_scale_x));
            int sy_min = atoi(lv_textarea_get_text(ta_scale_y_min));
            int sy_max = atoi(lv_textarea_get_text(ta_scale_y_max));
            int rate = atoi(lv_textarea_get_text(ta_rate));
            int t = atoi(lv_textarea_get_text(ta_time));
            
            set_scale_x(sx);
            set_scale_y(sy_min, sy_max);
            set_rate(rate);
            set_time(t);
            set_active_sensor(sensor_idx);
            
            cmd_start_recording();
            
            close_submenu();
            close_menu();
        }
        else if (obj == btn_sub_cancel) close_submenu();
    } else if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        if (key == LV_KEY_ESC) close_submenu();
        
        // Manual group navigation for UP/DOWN just in case
        if (!lv_obj_has_state(obj, LV_STATE_EDITED)) {
            if (key == LV_KEY_UP) lv_group_focus_prev(submenu_group);
            else if (key == LV_KEY_DOWN) lv_group_focus_next(submenu_group);
        }

        if (obj == lv_obj_get_parent(label_graph)) { 
            if (key == LV_KEY_RIGHT) {
                sensor_idx = (sensor_idx + 1) % 4;
                lv_label_set_text_fmt(label_graph, "Graph: %s", sensors[sensor_idx]);
            } else if (key == LV_KEY_LEFT) {
                sensor_idx = (sensor_idx + 3) % 4;
                lv_label_set_text_fmt(label_graph, "Graph: %s", sensors[sensor_idx]);
            }
        }
        else if (obj == ta_scale_x || obj == ta_scale_y_min || obj == ta_scale_y_max || obj == ta_rate || obj == ta_time) {
            handle_ta_arrows(obj, key);
        }
    }
}

static void open_submenu() {
    lv_obj_add_flag(main_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(submenu_container, LV_OBJ_FLAG_HIDDEN);
    lv_indev_set_group(keypad_indev, submenu_group);
    lv_group_focus_obj(lv_obj_get_parent(label_graph)); // Focus the graph button
}

static void close_submenu() {
    lv_obj_add_flag(submenu_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(main_cont, LV_OBJ_FLAG_HIDDEN);
    lv_indev_set_group(keypad_indev, menu_group);
    lv_group_focus_obj(btn_start_rec); // Restore focus!
}

static void load_submenu_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        if (key == LV_KEY_ESC) close_load_submenu();
    }
}

static void open_load_submenu() {
    lv_obj_add_flag(main_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(load_cont, LV_OBJ_FLAG_HIDDEN);
    lv_indev_set_group(keypad_indev, load_group);
    lv_group_focus_next(load_group); // Focus the dummy button
}

static void close_load_submenu() {
    lv_obj_add_flag(load_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(main_cont, LV_OBJ_FLAG_HIDDEN);
    lv_indev_set_group(keypad_indev, menu_group);
    lv_group_focus_obj(btn_load); // Restore focus to the load button!
}

static lv_obj_t * create_menu_btn(lv_obj_t * parent, const char * text, lv_group_t * group, lv_event_cb_t cb) {
    lv_obj_t * btn = lv_button_create(parent);
    lv_obj_set_width(btn, 260);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_ALL, NULL);
    apply_retro_btn_style(btn);

    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    
    lv_group_add_obj(group, btn);
    return btn;
}

static lv_obj_t * create_submenu_ta(lv_obj_t * parent, const char * title, lv_group_t * group) {
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 280, 40);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);

    lv_obj_t * label = lv_label_create(cont);
    lv_label_set_text(label, title);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_font(label, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x00AA00), 0);

    lv_obj_t * ta = lv_textarea_create(cont);
    lv_textarea_set_one_line(ta, true);
    lv_obj_set_size(ta, 120, 32);
    lv_obj_align(ta, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_textarea_set_text(ta, "0");
    
    apply_retro_style(ta);
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x004400), LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(ta, lv_color_hex(0x00FF00), LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x00AA00), LV_STATE_EDITED);
    lv_obj_set_style_text_color(ta, lv_color_hex(0x000000), LV_STATE_EDITED);
    
    lv_group_add_obj(group, ta);
    lv_obj_add_event_cb(ta, submenu_event_cb, LV_EVENT_KEY, NULL);
    return ta;
}

void init_menu(lv_indev_t * indev) {
    keypad_indev = indev;
    menu_group = lv_group_create();
    submenu_group = lv_group_create();
    load_group = lv_group_create();

    menu_screen = lv_obj_create(lv_screen_active());
    lv_obj_set_size(menu_screen, 320, 240);
    lv_obj_center(menu_screen);
    lv_obj_set_style_bg_color(menu_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(menu_screen, LV_OPA_80, 0);
    lv_obj_set_style_border_width(menu_screen, 0, 0);
    lv_obj_add_flag(menu_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(menu_screen);
    
    main_cont = lv_obj_create(menu_screen);
    lv_obj_set_size(main_cont, 300, 220);
    lv_obj_center(main_cont);
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    apply_retro_style(main_cont);
    lv_obj_set_style_bg_color(main_cont, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(main_cont, LV_OPA_COVER, 0);

    lv_obj_t * title = lv_label_create(main_cont);
    lv_label_set_text(title, "MAIN MENU");
    lv_obj_set_style_text_font(title, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x00FF00), 0);
    
    btn_start_rec = create_menu_btn(main_cont, "New Recording", menu_group, menu_event_cb);
    btn_save = create_menu_btn(main_cont, "Save", menu_group, menu_event_cb);
    btn_load = create_menu_btn(main_cont, "Load", menu_group, menu_event_cb);
    btn_exit = create_menu_btn(main_cont, "Exit", menu_group, menu_event_cb);

    submenu_container = lv_obj_create(menu_screen);
    lv_obj_set_size(submenu_container, 300, 220);
    lv_obj_center(submenu_container);
    lv_obj_set_flex_flow(submenu_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(submenu_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    apply_retro_style(submenu_container);
    lv_obj_add_flag(submenu_container, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t * sub_title = lv_label_create(submenu_container);
    lv_label_set_text(sub_title, "NEW RECORDING");
    lv_obj_set_style_text_font(sub_title, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(sub_title, lv_color_hex(0x00FF00), 0);
    // Add some bottom margin to the title so it pushes the scroll container down
    lv_obj_set_style_margin_bottom(sub_title, 5, 0);

    lv_obj_t * scroll_cont = lv_obj_create(submenu_container);
    // Let the scroll container fill the remaining height
    lv_obj_set_size(scroll_cont, 290, 170); 
    lv_obj_set_style_pad_all(scroll_cont, 0, 0);
    lv_obj_set_style_border_width(scroll_cont, 0, 0);
    lv_obj_set_style_bg_opa(scroll_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(scroll_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scroll_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * graph_btn = lv_button_create(scroll_cont);
    lv_obj_set_width(graph_btn, 260);
    apply_retro_btn_style(graph_btn);
    lv_group_add_obj(submenu_group, graph_btn);
    lv_obj_add_event_cb(graph_btn, submenu_event_cb, LV_EVENT_KEY, NULL);
    label_graph = lv_label_create(graph_btn);
    lv_label_set_text_fmt(label_graph, "Graph: %s", sensors[sensor_idx]);
    lv_obj_center(label_graph);

    ta_scale_x = create_submenu_ta(scroll_cont, "Sc X (s):", submenu_group);
    lv_textarea_set_text(ta_scale_x, "10");
    ta_scale_y_min = create_submenu_ta(scroll_cont, "Sc Y Min:", submenu_group);
    lv_textarea_set_text(ta_scale_y_min, "0");
    ta_scale_y_max = create_submenu_ta(scroll_cont, "Sc Y Max:", submenu_group);
    lv_textarea_set_text(ta_scale_y_max, "4096");
    ta_rate = create_submenu_ta(scroll_cont, "Rate (Hz):", submenu_group);
    lv_textarea_set_text(ta_rate, "1");
    ta_time = create_submenu_ta(scroll_cont, "Time (s):", submenu_group);
    lv_textarea_set_text(ta_time, "20");

    btn_sub_start = create_menu_btn(scroll_cont, "Start", submenu_group, submenu_event_cb);
    btn_sub_cancel = create_menu_btn(scroll_cont, "Cancel", submenu_group, submenu_event_cb);

    // Load Submenu
    load_cont = lv_obj_create(menu_screen);
    lv_obj_set_size(load_cont, 300, 220);
    lv_obj_center(load_cont);
    lv_obj_set_flex_flow(load_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(load_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    apply_retro_style(load_cont);
    lv_obj_add_flag(load_cont, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t * load_title = lv_label_create(load_cont);
    lv_label_set_text(load_title, "LOAD RECORDING (Blank)");
    lv_obj_set_style_text_font(load_title, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(load_title, lv_color_hex(0x00FF00), 0);
    
    lv_obj_t * dummy_btn = lv_button_create(load_cont);
    lv_obj_set_width(dummy_btn, 260);
    apply_retro_btn_style(dummy_btn);
    lv_obj_add_event_cb(dummy_btn, load_submenu_event_cb, LV_EVENT_KEY, NULL);
    lv_obj_t * dummy_label = lv_label_create(dummy_btn);
    lv_label_set_text(dummy_label, "Back");
    lv_obj_center(dummy_label);
    lv_group_add_obj(load_group, dummy_btn);
}

void toggle_menu() {
    if (is_menu_open) {
        close_menu();
    } else {
        prev_group = lv_indev_get_group(keypad_indev);
        lv_obj_remove_flag(menu_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(main_cont, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(submenu_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(load_cont, LV_OBJ_FLAG_HIDDEN);
        lv_indev_set_group(keypad_indev, menu_group);
        lv_group_focus_obj(btn_start_rec); // Focus first button
        is_menu_open = true;
    }
}

bool is_menu_active() {
    return is_menu_open;
}
