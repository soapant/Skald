#include "core/flapi.h"

void Testing(void) {
    fl_type_text("Bitch Ass NIgga");
}


FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
