#include <stdbool.h>
#include "core/window_manager.h"

int pending_hotkey_vk = 0;
int pending_hotkey_mod = 0;
long pending_hotkey_timeMs = 0;

int WindowManager_Run(WindowManager *wm) {
  (void)wm;
  return -1;
}

void WindowManager_ProcessEvents(WindowManager *wm) { (void)wm; }
