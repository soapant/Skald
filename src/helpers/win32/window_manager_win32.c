#include "core/types/window.h"
#include "core/types/color.h"
#include "core/window_manager.h"
#include "core/hotkey_manager.h"
#include "core/context.h"
#include "core/listbox_hover.h"
#include "platform/widget.h"
#include "helpers/helpers_input_state.h"
#include "platform/platform.h"
#include <windows.h>
#include "core/feature_loader.h"

int pending_hotkey_vk = 0;
int pending_hotkey_mod = 0;
long pending_hotkey_timeMs = 0;

int WindowManager_Run(WindowManager *wm) {
  if (!wm)
    return -1;
  wm->running = true;

  MSG msg;
  while (wm->running) {
    feature_loader_drain_retired_unloads();
    bool got = false;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
      got = true;
      if (msg.message == WM_QUIT) {
        wm->running = false;
        break;
      }
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    Platform_Widget_DrainClicks();
    hotkey_drain_pending();
    if (!got)
      Sleep(10);
  }
  return 0;
}

void WindowManager_ProcessEvents(WindowManager *wm) {
  if (!wm) {
    return;
  }

  feature_loader_drain_retired_unloads();
  MSG msg;
  bool got = false;
  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    got = true;
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  if (!got)
    MsgWaitForMultipleObjects(0, NULL, FALSE, 16, QS_ALLINPUT);
  hotkey_drain_pending();
  {
    int mx = 0, my = 0;
    if (Platform_GetMousePos(&mx, &my))
      listbox_publish_mouse_pos(mx, my);
  }
  WindowManager_DrainSelectionComplete(wm);
}

void WindowManager_Quit(WindowManager *wm) {
  if (wm)
    wm->running = false;
  PostQuitMessage(0);
}
