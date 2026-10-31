#include "platform/wm_platform.h"
#include "platform/platform.h"
#include "core/flapi.h"
#include "platform/input_state.h"
#include "core/window_manager.h"
#include "core/remap_manager.h"
#include "core/prompt_controller.h"
#include "deps/log.h"
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "core/prompt_geometry.h"
#include "core/context.h"
#include "core/modal.h"
#include "core/timer.h"
#include "core/listbox_hover.h"
#include "helpers/helpers_ui.h"
#include "platform/ui.h"
#include "platform/win32/common_ui_win32.h"
#include "platform/win32/popup_helpers_win32.h"

extern int WindowManager_RunWin32(WindowManager* wm);
extern void WindowManager_QuitWin32(WindowManager* wm);
extern void WindowManager_ProcessEventsWin32(WindowManager* wm);
#include "platform/win32/win32_state.h"

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
                x = mi.rcMonitor.left;
                y = mi.rcMonitor.top;
                w = mi.rcMonitor.right - mi.rcMonitor.left;
                h = mi.rcMonitor.bottom - mi.rcMonitor.top;
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


void Platform_GetTextLayoutMetrics(int* line_h, int* row_h, int* list_top) {
        

    int lh = FontSize;
    int rh = PromptRowHeight();
    if (line_h) *line_h = lh;
    if (row_h) *row_h = rh;
    if (list_top) *list_top = PromptListTop();
    
}

char* Platform_PromptUser(WindowManager* wm, const PromptSpec* spec) {
    log_info("Platform_PromptUser started wm=%p spec=%p",wm,spec);
    if (!wm || !spec) {  return NULL; }
    Popup* popup = PromptPopup_Create(wm, spec);
    if (!popup) {  return NULL; }
    if (spec->subwindow) {
        static bool s_subwindow_creating = false;
        if (s_subwindow_creating) {
            log_info("Platform_PromptUser subwindow REENTRANT blocked title=%s", spec->title ? spec->title : "(null)");
            
            return NULL;
        }
        if (wm && wm->popup_count > 1) {
            log_info("Platform_PromptUser subwindow BLOCKED (already open) title=%s popup_count=%d", spec->title ? spec->title : "(null)", wm->popup_count);
            
            return NULL;
        }
        s_subwindow_creating = true;
        log_info("Platform_PromptUser subwindow ENTER title=%s popup_count=%d", spec->title ? spec->title : "(null)", wm ? wm->popup_count : -1);
        PromptSpec heap_spec = *spec;
        heap_spec.subwindow = false;
        Popup* _created = PromptPopup_Create(wm, &heap_spec);
        log_info("Platform_PromptUser subwindow EXIT created=%p popup_count=%d", (void*)_created, wm ? wm->popup_count : -1);
        s_subwindow_creating = false;
        
        return NULL;
    }
    long long last_tick = 0;
    int last_mx = 0, last_my = 0;
    int last_btn = -1;
    while (wm->running && popup->manager) {
        Platform_WM_ProcessEvents(wm);
        bool mouse_changed = false;
        int mx = 0, my = 0;
        if (Platform_GetMousePos(&mx, &my)) {
            ListBox_PublishMousePos(mx, my);
            int btn = Platform_MouseStateDown(MOUSE_BTN_LEFT) ? 1 : 0;
            if (mx != last_mx || my != last_my || btn != last_btn) {
                last_mx = mx;
                last_my = my;
                last_btn = btn;
                mouse_changed = true;
            }
        }
        Popup* mouse_top = WindowManager_GetTopPopup(wm);
        if (mouse_changed && mouse_top && mouse_top->vtable && mouse_top->vtable->on_render) {
            mouse_top->vtable->on_render(mouse_top);
        }
        long long now = Platform_GetWallClockMs();
        if (now - last_tick >= 1000 && WindowManager_TopPopupWantsTick(wm)) {
            last_tick = now;
            Popup* top = WindowManager_GetTopPopup(wm);
            if (top && top->vtable && top->vtable->on_render) { top->vtable->on_render(top);}
        }
        if (PromptController_PollSequenceTimeout()) {
            Popup* top = WindowManager_GetTopPopup(wm);
            if (top && top->vtable && top->vtable->on_render) { top->vtable->on_render(top);}
        }
    }
    if (popup->manager == NULL) {
        if (popup->vtable && popup->vtable->destroy) popup->vtable->destroy(popup);
    } else {
        WindowManager_PopPopup(wm);
        if (popup->vtable && popup->vtable->destroy) popup->vtable->destroy(popup);
    }
    FlintContext* _c = FlintContext_Get();
    return _c->window.output_buffer[0] ? _c->window.output_buffer : NULL;
}
