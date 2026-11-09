#include "core/skapi.h"
#include <stdio.h>

void Testing() {
  SkTimer p = sk_profile_start();
  sk_profile_end(p);
}

SKALD_FEATURE_INIT {
  sk_register_hotkey(.hotkey = "Alt+Shift+V", .cb = Testing);
}
