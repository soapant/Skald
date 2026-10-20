#include "core/flapi.h"

bool open_youtube(void) {
    fl_launch_browser("https://youtube.com");
    return true;
}

FLINT_FEATURE_INIT {
    fl_register_hotkey(.hotkey="Alt+Ctrl+Y", .cb=open_youtube);
    fl_add_to_cc(.display_name="Open YouTube",.on_enter=open_youtube,.type="QuickLink",.icon="/video.png");
}
