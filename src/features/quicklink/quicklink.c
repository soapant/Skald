#include "core/skapi.h"

void open_youtube(void) { sk_launch_browser("https://youtube.com"); }

SKALD_FEATURE_INIT {
  sk_register_hotkey(.hotkey = "Alt+Ctrl+Y", .cb = open_youtube);
  sk_add_to_cc(.display_name = "Open YouTube",
               .on_enter = (bool (*)(void))open_youtube, .type = "QuickLink",
               .icon = "/video.png");
}
