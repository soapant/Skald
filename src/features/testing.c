#include "core/flapi.h"


void Testing(void) {
  fl_run_terminal_command("feh /home/diggy/Pictures/wallpaper.jpg");
}



FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
