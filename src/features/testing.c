#include "core/flapi.h"

void Testing(void) {
  fl_focus_window_by_title("zen");
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}