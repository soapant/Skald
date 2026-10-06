#include "core/flapi.h"
#include <stdio.h>

void Testing(void) {
    FlTimer p = fl_profile_start();
    FlColor c = fl_get_pixel_color(fl_get_cursor_pos());
    printf("color=%d,%d,%d\n", c.r, c.g, c.b);
    fl_profile_end(p);
}



FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
