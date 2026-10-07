// #include "core/flapi.h"
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
//
// static bool g_recording = false;
// static FlThread g_thread = NULL;
//
// static void* recorder_loop(void* arg) {
//   while (1) {
//     FlKeyEvent e;
//     while (fl_poll_key(&e)) {
//       printf("mods=%d vk=%d sc=%d\n", e.mods, e.vk, e.scancode);
//     }
//     fl_sleep(0.1);
//   }
//   return NULL;
// }
//
// void RecordToggle(void) {
//   if (!g_recording) {
//     fl_clear_key_queue();
//     g_recording = true;
//     if (!g_thread) g_thread = fl_thread_create(recorder_loop, NULL);
//     return;
//   }
//   g_recording = false;
// }
//
// FLINT_FEATURE_INIT {
//   fl_register_hotkey("Alt+Shift+R", RecordToggle);
// }
