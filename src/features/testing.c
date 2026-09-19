#include "core/flapi.h"


void Testing(void) {
  printf("%s \n",fl_get_last_clipboard_entry());
}



FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
