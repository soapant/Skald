#include "core/flapi.h"
#include <stdio.h>


void Testing(void) {
  fl_get_cursor_pos();
}



FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
