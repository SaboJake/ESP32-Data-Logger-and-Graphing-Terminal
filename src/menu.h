#ifndef MENU_H
#define MENU_H

#include <lvgl.h>

void init_menu(lv_indev_t * indev);
void toggle_menu();
bool is_menu_active();

#endif
