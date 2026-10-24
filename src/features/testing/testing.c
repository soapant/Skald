#include "core/flapi.h"

void Testing() {
    FlTimer p = fl_profile_start();
    fl_window();
    fl_window(.x=100);
    fl_profile_end(p);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey(.hotkey="Alt+Shift+V",.cb=Testing);
}
