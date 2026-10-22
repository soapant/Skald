#include "core/flapi.h"
#include "core/window_manager.h"
#include <stdio.h>


void Testing() {
    FlTimer p = fl_profile_start();
    printf("%s \n",fl_resolve_data_path(__FILE__));
    fl_profile_end(p);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey(.hotkey="Alt+Shift+V",.cb=Testing);
}
