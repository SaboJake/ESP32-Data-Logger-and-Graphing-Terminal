#ifndef TERMINAL_H
#define TERMINAL_H

#include <lvgl.h>

// Initializes the terminal GUI components
void init_terminal_ui(int screen_width, int screen_height);

// Animates the terminal open and close
void toggle_terminal();

// Check if terminal is open
bool is_terminal_active();

#endif // TERMINAL_H
