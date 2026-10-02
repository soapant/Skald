#include "core/window_manager.h"
#include "core/hotkey_manager.h"
#include "platform/widget.h"
#include <windows.h>

int pending_hotkey_vk = 0;
int pending_hotkey_mod = 0;
long pending_hotkey_timeMs = 0;

int WindowManager_RunWin32(WindowManager* wm) {
    if (!wm) return -1;
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

    return 0;
}

void WindowManager_ProcessEventsWin32(WindowManager* wm) {
    if (!wm) return;

    MSG msg;
    bool got = false;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        got = true;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (!got) MsgWaitForMultipleObjects(0, NULL, FALSE, 16, QS_ALLINPUT);
    hotkey_drain_pending();
}

void WindowManager_QuitWin32(WindowManager* wm) {
    if (wm) wm->running = false;
    PostQuitMessage(0);
}
