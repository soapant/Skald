#include "core/flapi.h"


void Testing(void) {
  fl_send_hotkey("Alt+F");
}



FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
