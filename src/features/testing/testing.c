#include "core/flapi.h"
#include "core/window_manager.h"
#include <stdio.h>


bool Testing() {
    return false;
}
FLINT_FEATURE_INIT {
    fl_register_hotkey(.hotkey="Space", .should_swallow=Testing);
}
