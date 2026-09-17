#include "core/flapi.h"

void Testing(void) {
    fl_type_string("Bitch Ass NIgga");
}


FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
