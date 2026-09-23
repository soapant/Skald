#include "core/flapi.h"


void Testing(void) {
  printf("%s \n",getenv("TERMINAL"));
}



FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
