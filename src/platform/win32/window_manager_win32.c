#include "core/window_manager.h"
#include "core/hotkey_manager.h"
#include "core/context.h"
#include "platform/widget.h"
#include <windows.h>

#include "core/timer.h"
int pending_hotkey_vk = 0;
int pending_hotkey_mod = 0;
long pending_hotkey_timeMs = 0;

int WindowManager_RunWin32(WindowManager* wm) {
        PROFILE_START(_pt);
if (!wm) { PROFILE_END_VAR(_pt, "WindowManager_RunWin32"); return -1; }
    wm->running = true;

    MSG msg;
    while (wm->running) {
        bool got = false;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            got = true;
            if (msg.message == WM_QUIT) { wm->running = false; break; }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        Platform_Widget_DrainClicks();
        hotkey_drain_pending();
        if (!got) Sleep(10);
    }

    { PROFILE_END_VAR(_pt, "WindowManager_RunWin32"); return 0; }
}

static void WindowManager_DrainSelectionComplete(WindowManager* wm) {
        PROFILE_START(_pt);
    if (!wm || wm->popup_count == 0) { PROFILE_END_VAR(_pt, "DrainSelectionComplete"); return; }
    if (!FlintContext_Get()->window.selection_complete) { PROFILE_END_VAR(_pt, "DrainSelectionComplete"); return; }
    FlintContext_Get()->window.selection_complete = false;
    WindowManager_PopPopup(wm);
    Popup* top_after = WindowManager_GetTopPopup(wm);
    if (top_after && top_after->vtable && top_after->vtable->on_render) top_after->vtable->on_render(top_after);
    { PROFILE_END_VAR(_pt, "DrainSelectionComplete"); }
}

void WindowManager_ProcessEventsWin32(WindowManager* wm) {
        PROFILE_START(_pt);
if (!wm) { PROFILE_END_VAR(_pt, "WindowManager_ProcessEventsWin32"); return ; }

    MSG msg;
    bool got = false;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        got = true;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (!got) MsgWaitForMultipleObjects(0, NULL, FALSE, 16, QS_ALLINPUT);
    hotkey_drain_pending();
    WindowManager_DrainSelectionComplete(wm);
}

void WindowManager_QuitWin32(WindowManager* wm) {
        PROFILE_START(_pt);
if (wm) wm->running = false;
    PostQuitMessage(0);
}
