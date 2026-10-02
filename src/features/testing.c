#include "core/flapi.h"


void Testing(void) {
  fl_message(.message="nigga ", .slot=TOP_LEFT);
}



FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
