#include "core/flapi.h"

void open_youtube(void) { fl_launch_browser("https://youtube.com"); }

FLINT_FEATURE_INIT {
  fl_register_hotkey(.hotkey = "Alt+Ctrl+Y", .cb = open_youtube);
  fl_add_to_cc(.display_name = "Open YouTube",
               .on_enter = (bool (*)(void))open_youtube, .type = "QuickLink",
               .icon = "/video.png");
}
