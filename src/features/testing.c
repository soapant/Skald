#include "core/flapi.h"
#include <stdio.h>


void Testing(void) {
  fl_click_at_pos((FlPos){960,540});
}



FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
