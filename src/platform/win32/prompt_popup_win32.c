#include "core/context.h"
#include "core/window_manager.h"
#include "core/timer.h"
#include "deps/log.h"
#include "platform/wm_platform.h"
#include "platform/ui.h"
#include <windows.h>
#include <stdlib.h>

static HWND g_prev_fg = NULL;
static BOOL g_old_track = FALSE;
static BOOL g_track_saved = FALSE;
static HWND g_pending_restore = NULL;

static void CALLBACK Flint_ReenableTrackProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time) {
        PROFILE_START(_pt);
(void)hwnd; (void)msg; (void)time;
    KillTimer(NULL, id);
    if (g_track_saved) {
        SystemParametersInfoA(SPI_SETACTIVEWINDOWTRACKING, 0, (PVOID)(uintptr_t)g_old_track, 0);
        log_info("re-enabled active window tracking=%d", (int)g_old_track);
        g_track_saved = FALSE;
    }
}

static void CALLBACK Flint_RestoreFocusProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time) {
        PROFILE_START(_pt);
(void)hwnd; (void)msg; (void)time;
    KillTimer(NULL, id);
    if (g_pending_restore && IsWindow(g_pending_restore)) {
        HWND cur = GetForegroundWindow();
        if (cur != g_pending_restore) {
            log_info("Flint_RestoreFocusProc re-asserting focus cur=%p target=%p", cur, g_pending_restore);
            HWND target = g_pending_restore;
            if (IsIconic(target)) ShowWindow(target, SW_RESTORE);
            DWORD fgTid = cur ? GetWindowThreadProcessId(cur, NULL) : 0;
            DWORD targetTid = GetWindowThreadProcessId(target, NULL);
            DWORD curTid = GetCurrentThreadId();
            if (fgTid && fgTid != curTid) AttachThreadInput(fgTid, curTid, TRUE);
            if (targetTid && targetTid != curTid) AttachThreadInput(targetTid, curTid, TRUE);
            AllowSetForegroundWindow(ASFW_ANY);
            SetForegroundWindow(target);
            SetActiveWindow(target);
            BringWindowToTop(target);
            if (fgTid && fgTid != curTid) AttachThreadInput(fgTid, curTid, FALSE);
            if (targetTid && targetTid != curTid) AttachThreadInput(targetTid, curTid, FALSE);
        }
        g_pending_restore = NULL;
    }
}

static bool is_flint_popup_class(HWND hwnd) {
        PROFILE_START(_pt);
if (!hwnd || !IsWindow(hwnd)) { PROFILE_END_VAR(_pt, "is_flint_popup_class"); return false; }
    char cls[64] = {0};
    GetClassNameA(hwnd, cls, sizeof(cls));
    { PROFILE_END_VAR(_pt, "is_flint_popup_class"); return strcmp(cls, "QuickSwitcherModalClass")==0 || strcmp(cls, "DisplayMessageModalClass")==0
        || strcmp(cls, "EditPopupClass")==0 || strcmp(cls, "SearchPopupClass")==0; }
}

HWND Platform_GetFlintPrevFg(void) {
        PROFILE_START(_pt);
if (g_prev_fg && IsWindow(g_prev_fg)) { PROFILE_END_VAR(_pt, "Platform_GetFlintPrevFg"); return g_prev_fg; }
    { PROFILE_END_VAR(_pt, "Platform_GetFlintPrevFg"); return NULL; }
}
void Platform_SetFlintPrevFg(HWND h) {
        PROFILE_START(_pt);
if (h && IsWindow(h)) g_prev_fg = h;
}

bool Platform_PromptPopupShowWindow(Popup* self, int x, int y, int w, int h) {
        PROFILE_START(_pt);
(void)self;
    PROFILE_START(timer);
    HWND fg = GetForegroundWindow();
    if (fg && IsWindow(fg) && !is_flint_popup_class(fg)) g_prev_fg = fg;
    else if (!fg || !IsWindow(fg)) g_prev_fg = NULL;
    char _save_title[128] = {0};
    if (g_prev_fg) GetWindowTextA(g_prev_fg, _save_title, sizeof(_save_title));
    log_info("Platform_PromptPopupShowWindow saving prev_fg=%p title='%s'", g_prev_fg, _save_title);
    {
        BOOL cur = FALSE;
        if (SystemParametersInfoA(SPI_GETACTIVEWINDOWTRACKING, 0, &cur, 0)) {
            g_old_track = cur;
            g_track_saved = TRUE;
            if (cur) {
                SystemParametersInfoA(SPI_SETACTIVEWINDOWTRACKING, 0, (PVOID)FALSE, 0);
                log_info("disabled active window tracking (was %d)", (int)cur);
            }
        }
    }
    HINSTANCE hInstance = GetModuleHandle(NULL);
    FlintContext_Get()->window.hPopupWnd = WH_FROM_HWND(CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_COMPOSITED,
        "QuickSwitcherModalClass", NULL, WS_POPUP | WS_CLIPCHILDREN,
        x, y, w, h, NULL, NULL, hInstance, NULL
        ));
    if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) == NULL) {
        log_error("CreateWindowExA failed: %lu", GetLastError());
        PROFILE_END_VAR(timer, "window_manager:Platform_PromptPopupShowWindow");
        return false;
    }
    win32_apply_rounded_corners(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), w, h);
    PROFILE_END_VAR(timer, "window_manager:Platform_PromptPopupShowWindow");
    { PROFILE_END_VAR(_pt, "Platform_PromptPopupShowWindow"); return true; }
}

void Platform_PromptPopupHideWindow(Popup* self) {
        PROFILE_START(_pt);
(void)self;
    PROFILE_START(timer);
    if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) != NULL && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd))) {
        KillTimer(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), 999);
        KillTimer(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), UI_TICK_TIMER_ID);
        DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd));
        FlintContext_Get()->window.hPopupWnd = WH_NULL;
    }
    if (g_prev_fg && IsWindow(g_prev_fg)) {
        HWND target = g_prev_fg;
        log_info("Platform_PromptPopupHideWindow restoring prev_fg=%p", target);
        if (IsIconic(target)) ShowWindow(target, SW_RESTORE);
        HWND fg = GetForegroundWindow();
        DWORD fgTid = fg ? GetWindowThreadProcessId(fg, NULL) : 0;
        DWORD targetTid = GetWindowThreadProcessId(target, NULL);
        DWORD curTid = GetCurrentThreadId();
        if (fgTid && fgTid != curTid) AttachThreadInput(fgTid, curTid, TRUE);
        if (targetTid && targetTid != curTid) AttachThreadInput(targetTid, curTid, TRUE);
        AllowSetForegroundWindow(ASFW_ANY);
        SetForegroundWindow(target);
        SetActiveWindow(target);
        BringWindowToTop(target);
        if (fgTid && fgTid != curTid) AttachThreadInput(fgTid, curTid, FALSE);
        if (targetTid && targetTid != curTid) AttachThreadInput(targetTid, curTid, FALSE);
        HWND after = GetForegroundWindow();
        log_info("after restore cur=%p target=%p %s", after, target, after==target?"ok":"MISMATCH");
        g_pending_restore = target;
        SetTimer(NULL, 0xC0DE, 80, Flint_RestoreFocusProc);
        if (g_track_saved && g_old_track) SetTimer(NULL, 0xC0DF, 800, Flint_ReenableTrackProc);
        else g_track_saved = FALSE;
        g_prev_fg = NULL;
    } else {
        if (g_track_saved && g_old_track) SetTimer(NULL, 0xC0DF, 800, Flint_ReenableTrackProc);
        else g_track_saved = FALSE;
    }
    PROFILE_END_VAR(timer, "window_manager:Platform_PromptPopupHideWindow");
}

void Platform_PromptPopupRender(Popup* self) {
        PROFILE_START(_pt);
(void)self;
}

void Platform_PromptPopupDestroyWindow(Popup* self) {
        PROFILE_START(_pt);
Platform_PromptPopupHideWindow(self);
}

bool Platform_PromptPopupOnKey(Popup* self, const PopupKeyEvent* key) {
        PROFILE_START(_pt);
(void)self;
    (void)key;
    { PROFILE_END_VAR(_pt, "Platform_PromptPopupOnKey"); return false; }
}

void HandleCommandExecution(ListBoxInfo* lb, char* string) {
        PROFILE_START(_pt);
if (!lb) { PROFILE_END_VAR(_pt, "HandleCommandExecution"); return ; }
    int num = atoi(string);
    UI_SetSelection(lb, FlintContext_Get()->window.hListBox, num - 1);
    UI_LB_SetCaretIndex(FlintContext_Get()->window.hListBox, num, TRUE);
}
