#include "core/flapi.h"

static void render(PromptState* state) {
    fl_place_editbox();
    if (fl_is_key_pressed(FL_KEY_L) && fl_ctrl_pressed()) {
        fl_subwindow(.x=100,.y=100,.w=500,.h=500);
    }
}

void Testing() {
    FlTimer p = fl_profile_start();
    fl_window(.render=render);
    fl_profile_end(p);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey(.hotkey="Alt+Shift+V",.cb=Testing);
}
