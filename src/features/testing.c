#include "core/flapi.h"
#include "core/window_manager.h"
#include <stdio.h>


void Testing() {
    FlTimer p = fl_profile_start();
    printf("%i %i , %i %i %i \n",fl_get_cursor_pos().x,fl_get_cursor_pos().y,fl_get_pixel_color(fl_get_cursor_pos()).r,fl_get_pixel_color(fl_get_cursor_pos()).g,fl_get_pixel_color(fl_get_cursor_pos()).b);
    fl_profile_end(p);
}



FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
