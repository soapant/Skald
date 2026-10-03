#include "core/flapi.h"
#include <stdio.h>


void Testing(void) {
  FlPos pos = fl_get_cursor_pos();
  printf("pos=(%d,%d)\n", pos.x, pos.y);
}



FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
