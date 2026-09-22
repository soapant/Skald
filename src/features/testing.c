#include "core/flapi.h"


void Testing(void) {
    FlTimer timer = fl_profile_start();
    fl_sleep(0.5);
    fl_profile_end(timer);
}



FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
