#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "core/context.h"
#include "platform/platform.h"
#include "core/flapi.h"
#include "platform/flapi_platform.h"
#include "core/remap_manager.h"

// TEST(terminal_open) {
//     PlWindowList before = Platform_GetWindowTitles();
//     Config_Load();
//     Config_ResolveDefaultApps();
//     Platform_RunTerminalCommand("echo Hello, World!", true);
//     PlWindowList after = {0};
//     for (int i = 0; i < 50; i++) {
//         fl_free_window_list(&after);
//         after = Platform_GetWindowTitles();
//         if (after.count == before.count + 1) break;
//         fl_sleep_platform(0.1);
//     }
//     ASSERT_EQ_INT(after.count, before.count + 1);
//     if (after.count == before.count + 1) { const char* new_title = after.windows[after.count - 1]; Platform_FocusWindow(new_title);
//         Platform_CloseActiveWindow();
//     }
//     fl_free_window_list(&before);
//     fl_free_window_list(&after);
// }

// TEST(browser_open) {
//     PlWindowList before = Platform_GetWindowTitles();
//     Config_Load();
//     Config_ResolveDefaultApps();
//     Platform_LaunchBrowserAtUrl("things",false);
//     PlWindowList after = {0};
//     for (int i = 0; i < 50; i++) {
//         fl_free_window_list(&after);
//         after = Platform_GetWindowTitles();
//         if (after.count == before.count + 1) break;
//         fl_sleep_platform(0.1);
//     }
//     ASSERT_EQ_INT(after.count, before.count + 1);
//     if (after.count == before.count + 1) {
//         const char* new_title = after.windows[after.count - 1]; 
//         Platform_FocusWindow(new_title);
//         remap_send_combo("Alt+W");
//         Platform_CloseActiveWindow();
//     }
//     fl_free_window_list(&before);
//     fl_free_window_list(&after);
// }

int main(void) {
    // RUN(terminal_open);
    // RUN(browser_open);
    // return TEST_SUMMARY() != 0 ? 1 : 0;
}
