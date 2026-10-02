#include "core/flapi.h"


void Testing(void) {
  fl_launch_with_args("nob.c",NULL);
}



FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
