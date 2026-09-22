#include "core/flapi.h"


void Testing(void) {
    PROFILE_START(timer);
    char *reply = fl_send_ai_message("Hello, how are you?");
    printf("%s \n",reply);
    PROFILE_END_VAR(timer, "Testing - fl_send_ai_message");
}



FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
