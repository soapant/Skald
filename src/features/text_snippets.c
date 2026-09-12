#include "core/flapi.h"

void copy_seifisbad(void) {
    fl_copy_to_clipboard("seifisbad@gmail.com");
    SubjectiveMessageDisplay("seifisbad@gmail.com",140,1,COLOR_TEXT);
}
void copy_password(void) {
    fl_copy_to_clipboard("i_am_retarded1");
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Ctrl+O", copy_seifisbad);
    fl_register_hotkey("Alt+Ctrl+P", copy_password);

    fl_register_feature("Text Snippets", copy_seifisbad);
}
