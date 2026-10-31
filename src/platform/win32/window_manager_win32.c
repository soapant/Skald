#include "core/window_manager.h"
#include "core/hotkey_manager.h"
#include "core/context.h"
#include "core/listbox_hover.h"
#include "platform/widget.h"
#include "platform/input_state.h"
#include <windows.h>

#include "core/timer.h"
int pending_hotkey_vk = 0;
int pending_hotkey_mod = 0;
long pending_hotkey_timeMs = 0;

int WindowManager_RunWin32(WindowManager *wm) {
  if (!wm)
    return -1;
  wm->running = true;

  MSG msg;
  while (wm->running) {
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

static void WindowManager_DrainSelectionComplete(WindowManager *wm) {

  if (!wm || wm->popup_count == 0) {
    return;
  }
  if (!FlintContext_Get()->window.selection_complete) {
    return;
  }
  FlintContext_Get()->window.selection_complete = false;
  WindowManager_PopPopup(wm);
  Popup *top_after = WindowManager_GetTopPopup(wm);
  if (top_after && top_after->vtable && top_after->vtable->on_render)
    top_after->vtable->on_render(top_after);
}

void WindowManager_ProcessEventsWin32(WindowManager *wm) {
  if (!wm) {
    return;
  }

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
      ListBox_PublishMousePos(mx, my);
  }
  WindowManager_DrainSelectionComplete(wm);
}

void WindowManager_QuitWin32(WindowManager *wm) {
  if (wm)
    wm->running = false;
  PostQuitMessage(0);
}
