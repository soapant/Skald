#include "platform/wm_platform.h"
#include "platform/input_state.h"
#include "core/window_manager.h"
#include "core/remap_manager.h"
#include "deps/log.h"
#include <windows.h>

extern int WindowManager_RunWin32(WindowManager* wm);
extern void WindowManager_QuitWin32(WindowManager* wm);
extern void WindowManager_ProcessEventsWin32(WindowManager* wm);

bool Platform_WM_InitDisplay(WindowManager* wm) {
    (void)wm;
    return true;
}

int Platform_WM_Run(WindowManager* wm) {
    return WindowManager_RunWin32(wm);
}

void Platform_WM_Quit(WindowManager* wm) {
    WindowManager_QuitWin32(wm);
}

void Platform_WM_ProcessEvents(WindowManager* wm) {
    WindowManager_ProcessEventsWin32(wm);
}

void Platform_WM_OnPopEmpty(WindowManager* wm) {
    (void)wm;
    remap_reset();
    Platform_InputState_Clear();
}

void Platform_WM_GetMonitorGeometry(int *ox, int *oy, int *ow, int *oh) {
    int x = 0, y = 0, w = GetSystemMetrics(SM_CXSCREEN), h = GetSystemMetrics(SM_CYSCREEN);
    if (!w) w = 1920;
    if (!h) h = 1080;
    POINT pt = { 0, 0 };
    if (GetCursorPos(&pt)) {
        HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        if (mon) {
            MONITORINFO mi;
            mi.cbSize = sizeof(mi);
            if (GetMonitorInfoA(mon, &mi)) {
                x = mi.rcWork.left;
                y = mi.rcWork.top;
                w = mi.rcWork.right - mi.rcWork.left;
                h = mi.rcWork.bottom - mi.rcWork.top;
            }
        }
    }
    if (!w) w = 1920;
    if (!h) h = 1080;
    log_info("Platform_WM_GetMonitorGeometry cursor=(%ld,%ld) mon=(%d,%d %dx%d)", pt.x, pt.y, x, y, w, h);
    if (ox) *ox = x;
    if (oy) *oy = y;
    if (ow) *ow = w;
    if (oh) *oh = h;
}

