#include "core/flapi.h"
#include "core/window_manager.h"
#include <stdio.h>


void testing_render() {
    fl_draw_rect(0,0,100,100,RGB(255,0,0));
}

void Testing() {
    FlTimer p = fl_profile_start();
    fl_message(.render=testing_render,.no_bg=false,.w=120,.h=180,.x=960,.y=540);
    fl_profile_end(p);
}



FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
