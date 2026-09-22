#include "core/flapi.h"


void Testing(void) {
    FlTimer timer = fl_profile_start();
    char *reply = fl_query_ai("implement a simple linked list in C");
    printf("%s \n",reply);
    fl_profile_end(timer);
}



FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
