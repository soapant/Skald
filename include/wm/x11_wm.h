#ifndef X11_WM_H
#define X11_WM_H
#include <stdbool.h>
#include "core/platform.h"
bool get_title_x11(char *buffer, int buffer_size);
char* get_clipboard_entry_x11();
char* paste_x11(char* content);
bool focus_window_title_x11(char* title);
#endif