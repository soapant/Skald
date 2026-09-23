#include "core/flapi.h"


void Testing(void) {
  fl_screenshot_open_area("/tmp/screenshot.png");
}



FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
