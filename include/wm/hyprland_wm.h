#ifndef HYPRLAND_WM_H
#define HYPRLAND_WM_H
#include <stdbool.h>
#include "core/platform.h"
bool get_title_hyprland(char *buffer, int buffer_size);
char* get_clipboard_entry_hyprland();
char* paste_hyprland(char* content);
bool focus_window_title_hyprland(char* title);
#endif