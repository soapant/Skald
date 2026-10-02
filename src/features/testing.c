#include "core/flapi.h"


void Testing(void) {
  fl_message(.message="nigga nigg aniga dawkj dakwjd kawjdk jawk djwkj",.position="top-left");
}



FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
