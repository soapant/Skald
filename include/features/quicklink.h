#ifndef QUICKLINK_H
#define QUICKLINK_H

#include "core/flapi.h"

void open_youtube(void) {
    fl_launch_browser("https://youtube.com");
}

static inline void quicklink_init(void) {
    fl_register_hotkey("Alt+Ctrl+Y", open_youtube);
}

#endif
