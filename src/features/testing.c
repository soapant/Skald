#include "core/flapi.h"

void Testing(void) {
  Fl_Pos pos = fl_get_cursor_pos();
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
