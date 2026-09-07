#include "core/flapi.h"

void mute_messenger_call(void) {
    fl_adb_tap(323, 2148);
}

void toggle_messenger_speaker(void) {
    fl_adb_tap(757, 2148);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+M", mute_messenger_call);
    fl_register_hotkey("Alt+Shift+S", toggle_messenger_speaker);
}
