#include "core/flapi.h"


void Testing(void) {
  fl_copy_to_clipboard("TEXT");
}



FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
