#include "core/flapi.h"
#include <stdio.h>
#include <stdlib.h>

typedef char *(*adb_tap_fn)(int x, int y);

static void ExampleTapHotkey(void) {
  FL_CAP_BIND(get, "adb.tap", adb_tap_fn);
  if (!get) {
    printf("adb feature not loaded - skipping\n");
    return;
  }
  char *out = get(540, 1200);
  if (out) {
    printf("tap -> %s\n", out);
    free(out);
  }
}

FLINT_FEATURE_INIT {
  fl_register_hotkey("Alt+Ctrl+G", ExampleTapHotkey);
}
