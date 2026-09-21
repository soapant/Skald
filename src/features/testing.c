#include "core/flapi.h"


void Testing(void) {
}



FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
