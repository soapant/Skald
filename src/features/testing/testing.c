#include "core/flapi.h"
#include "core/window_manager.h"
#include <stdio.h>


bool Testing() {
    return true;
}
FLINT_FEATURE_INIT {
    fl_register_hotkey("Space", Testing);
}
