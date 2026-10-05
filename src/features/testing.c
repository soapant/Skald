#include "core/flapi.h"
#include <stdio.h>

void Testing(void) {
    FlTimer p = fl_profile_start();
    // fl_get_pixel_color(0, 0);
    fl_profile_end(p);
}



FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
