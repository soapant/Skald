#include "core/flapi.h"

void copy_seifisbad(void) {
    fl_copy_to_clipboard("seifisbad@gmail.com");
}
void copy_password(void) {
    fl_copy_to_clipboard("i_am_retarded1");
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+O", copy_seifisbad);
    fl_register_hotkey("Alt+Shift+P", copy_password);

    fl_register_feature("Text Snippets", copy_seifisbad);
}
