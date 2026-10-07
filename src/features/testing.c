#include "core/flapi.h"
#include "core/window_manager.h"
#include <stdio.h>


void testing_render(FlWidgetState *state) {
}

void Testing(void) {
    FlTimer p = fl_profile_start();
    fl_message(.render=testing_render,.no_bg=false,.w=120,.h=180,.x=960,.y=540);
    fl_profile_end(p);
}



FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
