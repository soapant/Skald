#include "core/flapi.h"

void Testing(void) {
  WindowList output = fl_get_list_of_active_windows();
  for (int i = 0 ; i < output.count;i++) {
    printf("%s \n",output.windows[i]);
  }
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
