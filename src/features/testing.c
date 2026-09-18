#include "core/flapi.h"

void nigga(void) {
  printf("AKJDAWKDJWAKD \n");
}

void Testing(void) {
  fl_run_after_time(3,nigga);
}


FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
