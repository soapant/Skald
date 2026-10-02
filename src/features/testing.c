#include "core/flapi.h"


void Testing(void) {
  fl_message(.message="nigga",.position="top-left");
  fl_message(.message="test",.position="top-right");
}



FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
