#include "core/flapi.h"
#include <stdio.h>

FLINT_FEATURE_INIT {
    fl_register_remapping(.src="LeftShift+RightAlt", .dst="Enter");

    fl_register_remapping(.src="Ctrl+Alt+W", .dst="Up");
    fl_register_remapping(.src="Ctrl+Alt+A", .dst="Left");
    fl_register_remapping(.src="Ctrl+Alt+S", .dst="Down");
    fl_register_remapping(.src="Ctrl+Alt+D", .dst="Right");


    fl_register_remapping(.src="Ctrl+I", .dst="~");
    fl_register_remapping(.src="Ctrl+U", .dst="|");

    fl_register_remapping(.src="A", .dst="Left", .whitelist={"feh"});
    fl_register_remapping(.src="D", .dst="Right", .whitelist={"feh"});
}

