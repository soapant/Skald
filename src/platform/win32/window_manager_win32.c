#include "core/window_manager.h"
#include "core/hotkey_manager.h"
#include <windows.h>

int pending_hotkey_vk = 0;
int pending_hotkey_mod = 0;
long pending_hotkey_timeMs = 0;

int WindowManager_RunWin32(WindowManager* wm) {
    if (!wm) return -1;
    wm->running = true;

    MSG msg;
    while (wm->running && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        hotkey_drain_pending();
    }

    return 0;
}

void WindowManager_ProcessEventsWin32(WindowManager* wm) {
    if (!wm) return;
    
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    hotkey_drain_pending();
}

void WindowManager_QuitWin32(WindowManager* wm) {
    if (wm) wm->running = false;
    PostQuitMessage(0);
}
