#include "core/flapi.h"
#include <stdio.h>

FLINT_FEATURE_INIT {
    fl_register_remapping("LeftShift+RightAlt", "Enter");

    fl_register_remapping("Ctrl+Alt+W", "Up");
    fl_register_remapping("Ctrl+Alt+A", "Left");
    fl_register_remapping("Ctrl+Alt+S", "Down");
    fl_register_remapping("Ctrl+Alt+D", "Right");


    fl_register_remapping("Ctrl+I", "~");
    fl_register_remapping("Ctrl+U", "|");

    fl_register_remapping("A", "Left",WHITELIST,"feh");
    fl_register_remapping("D", "Right",WHITELIST,"feh");
}

