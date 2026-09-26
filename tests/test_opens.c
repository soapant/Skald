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
//     Fl_WindowList before = Platform_GetWindowTitles();
//     Config_Load();
//     Config_ResolveDefaultApps();
//     Platform_RunTerminalCommand("echo Hello, World!", true);
//     Fl_WindowList after = {0};
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
//     Fl_WindowList before = Platform_GetWindowTitles();
//     Config_Load();
//     Config_ResolveDefaultApps();
//     Platform_LaunchBrowserAtUrl("things",false);
//     Fl_WindowList after = {0};
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

// TEST(image_viewer_open) {
//     Fl_WindowList before = Platform_GetWindowTitles();
//     Config_Load();
//     Config_ResolveDefaultApps();
//     unsigned char px[12] = {255,0,0, 0,255,0, 0,0,255, 255,255,255};
//     char ppm[sizeof("P6\n2 2\n255\n") + sizeof(px)];
//     memcpy(ppm, "P6\n2 2\n255\n", sizeof("P6\n2 2\n255\n") - 1);
//     memcpy(ppm + sizeof("P6\n2 2\n255\n") - 1, px, sizeof(px));
//     char img[512] = {0};
//     ASSERT_TRUE(fl_write_temp_file("flinttest.ppm", ppm, sizeof(ppm), img, sizeof(img)));
//     char cmd[1024];
//     snprintf(cmd, sizeof(cmd), "%s %s", UsedImageViewer, img);
//     printf("%s \n",cmd);
//     Platform_LaunchDetached(cmd);
//     Fl_WindowList after = {0};
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
//         Platform_CloseActiveWindow();
//     }
//     remove(img);
//     fl_free_window_list(&before);
//     fl_free_window_list(&after);
// }

int main(void) {
    // RUN(terminal_open);
    // RUN(image_viewer_open);
    // RUN(browser_open);
    // return TEST_SUMMARY() != 0 ? 1 : 0;
}
