#include "core/flapi.h"

void open_youtube(void) {
    fl_launch_browser("https://youtube.com");
}

FLINT_FEATURE_INIT {
fl_register_hotkey("Alt+Ctrl+Y", open_youtube);
}
