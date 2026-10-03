#include "core/flapi.h"
#include <stdio.h>

void RecordMacro() {
    printf("RECORD MACRO");
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("", RecordMacro);
}
