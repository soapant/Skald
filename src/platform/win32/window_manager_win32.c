#include "core/window_manager.h"
#include "core/common_ui.h"
#include "core/modal.h"
#include "core/vim.h"
#include "core/platform.h"
#include "platform/win32/subclasses_win32.h"
#include "features/focus_switcher.h"
#include "features/music_player.h"
#include "features/video_viewer.h"
#include "features/process_killer.h"
#include "core/feature_data.h"
#include <windows.h>
#include <stdio.h>

int WindowManager_RunWin32(WindowManager* wm) {
    if (!wm) return -1;
    wm->running = true;

    MSG msg;
    while (wm->running && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
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
}

void WindowManager_QuitWin32(WindowManager* wm) {
    if (wm) wm->running = false;
    PostQuitMessage(0);
}
