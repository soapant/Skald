#ifndef SWAY_WM_H
#define SWAY_WM_H
#include <stdbool.h>
#include "core/platform.h"
bool get_title_sway(char *buffer, int buffer_size);
char* get_clipboard_entry_sway();
char* paste_sway(char* content);
bool focus_window_title_sway(char* title);
WindowList get_window_titles_sway();
WindowList get_window_names_sway();

#endif

