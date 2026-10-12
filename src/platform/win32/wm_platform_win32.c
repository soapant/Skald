#include "platform/wm_platform.h"
#include "platform/input_state.h"
#include "core/window_manager.h"
#include "core/remap_manager.h"
#include "deps/log.h"
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "core/prompt_geometry.h"
#include "core/context.h"
#include "core/modal.h"
#include "core/feature_data.h"
#include "core/timer.h"
#include "helpers/helpers_ui.h"
#include "platform/ui.h"
#include "platform/win32/common_ui_win32.h"
#include "platform/win32/popup_helpers_win32.h"

extern int WindowManager_RunWin32(WindowManager* wm);
extern void WindowManager_QuitWin32(WindowManager* wm);
extern void WindowManager_ProcessEventsWin32(WindowManager* wm);
#include "platform/win32/win32_state.h"

bool Platform_WM_InitDisplay(WindowManager* wm) {
        PROFILE_START(_pt);
(void)wm;
    { PROFILE_END_VAR(_pt, "Platform_WM_InitDisplay"); return true; }
}

int Platform_WM_Run(WindowManager* wm) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "Platform_WM_Run"); return WindowManager_RunWin32(wm); }
}

void Platform_WM_Quit(WindowManager* wm) {
        PROFILE_START(_pt);
WindowManager_QuitWin32(wm);
}

void Platform_WM_ProcessEvents(WindowManager* wm) {
        PROFILE_START(_pt);
WindowManager_ProcessEventsWin32(wm);
}

void Platform_WM_OnPopEmpty(WindowManager* wm) {
        PROFILE_START(_pt);
(void)wm;
    remap_reset();
    Platform_InputState_Clear();
}

void Platform_WM_GetMonitorGeometry(int *ox, int *oy, int *ow, int *oh) {
        PROFILE_START(_pt);
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
        PROFILE_START(_pt);
PROFILE_START(timer);
    int lh = FontSize;
    int rh = PromptRowHeight();
    if (line_h) *line_h = lh;
    if (row_h) *row_h = rh;
    if (list_top) *list_top = PromptListTop();
    PROFILE_END_VAR(timer, "wm_platform_win32:Platform_GetTextLayoutMetrics");
}

char* Platform_PromptUser(WindowManager* wm, const PromptSpec* spec) {
    PROFILE_START(_pt);
(void)wm;
  if (!spec) { log_error("Platform_PromptUser null spec"); return NULL; }
  const char** window_list = spec->items;
  const char* window_title = spec->title ? spec->title : "Flint";
  int count = 0;
  if (window_list) {
    for (count = 0; count < 8192 && window_list[count]; count++) {}
  }
  int item_height = PromptRowHeight();
  RefreshCallback refresh_callback = spec->refresh;
  EnterCallback enter_callback = spec->enter;
  CtrlEnterCallback ctrl_enter_callback = spec->ctrl_enter;
  TabCallback tab_callback = spec->tab;
  CopyCallback copy_callback = spec->copy;
  RenderCallback render_callback = spec->render;
  EditCallback edit_callback = spec->edit;
  FilterCallback filter_callback = spec->filter;
  HelpCallback help_callback = spec->help;

  FlintContext_Get()->window.selection_complete = false;
  FlintContext_Get()->window.output_buffer[0] = '\0';
  PROFILE_START(timer);

  if (g_win32_prompt_state) { ListBoxInfo_Free(&g_win32_prompt_state->listbox); free(g_win32_prompt_state); g_win32_prompt_state = NULL; }
  PromptState *win_state = NULL;
  ListBoxInfo* lb_ptr = NULL;
  FlintContext_Get()->window.editbox_placed = true;
  bool has_listbox = (window_list || spec->refresh || spec->render);
  if (has_listbox) {
    win_state = (PromptState*)calloc(1, sizeof(PromptState));
    if (!win_state) { log_error("operation failed, returning error spec=%p", spec); return NULL; }
    win_state->width = 900;
    win_state->height = 640;
    win_state->has_listbox = true;
    win_state->window_title = (char*)window_title;
    lb_ptr = &win_state->listbox;
    ListBoxInfo_Init(lb_ptr);
    g_win32_prompt_state = win_state;
  }

  FlintContext_Get()->prompt_callbacks.refresh = refresh_callback;
  FlintContext_Get()->prompt_callbacks.enter = enter_callback;
  FlintContext_Get()->prompt_callbacks.ctrl_enter = ctrl_enter_callback;
  FlintContext_Get()->prompt_callbacks.tab = tab_callback;
  FlintContext_Get()->prompt_callbacks.copy = copy_callback;
  FlintContext_Get()->prompt_callbacks.render = render_callback;
  FlintContext_Get()->prompt_callbacks.edit = edit_callback;
  FlintContext_Get()->prompt_callbacks.filter = filter_callback;
  FlintContext_Get()->prompt_callbacks.help = help_callback;
  FlintContext_Get()->window.title = window_title;
  {
    const char* p = strchr(window_title, ' ');
    const char* name = window_title;
    if (p) {
        int has_high = 0;
        for (const char* q = window_title; q < p; q++) {
            if ((unsigned char)*q >= 0x80) { has_high = 1; break; }
        }
        if (has_high) name = p + 1;
    }
    FlintContext_Get()->window.current_feature = (char*)name;
  }
  SetCurrentFeatureName(FlintContext_Get()->window.current_feature);
  BuildAliasDisplayItems(lb_ptr, window_list, count);
  extern EditorMode g_last_mode;

  if (ModePersistsThroughFeatures) {
    SetModeWithListBox(g_last_mode, lb_ptr);
  } else {
    SetModeWithListBox(MODE_INSERT, lb_ptr);
  }

  WindowLayout layout;
  CalculateWindowMetrics(&layout, 640, 900, item_height);
  if (win_state) {
    win_state->width = layout.w;
    win_state->height = layout.h;
  }

  HFONT hCustomFont = CreateCustomNerdFont(FontSize, UsedFont);

  if (!Platform_PromptPopupShowWindow(NULL, layout.x, layout.y, layout.w, layout.h)) {
    DeleteObject(hCustomFont);
    if (win_state) free(win_state);
    else if (lb_ptr) free(lb_ptr);
    if (g_win32_prompt_state == win_state) g_win32_prompt_state = NULL;
    return NULL;
  }
  SetWindowLongPtrA(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), GWLP_USERDATA, (LONG_PTR)(lb_ptr));
  if (spec->tick) SetTimer(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), UI_TICK_TIMER_ID, UI_TICK_TIMER_MS, NULL);

  PROFILE_END_VAR(timer, "Window Init");

  PROFILE_START(timer1);

  CreateChildControls(FlintContext_Get()->window.hPopupWnd, hCustomFont, window_list, count, &layout, lb_ptr);
  if (render_callback) {
    if (WH_TO_HWND(FlintContext_Get()->window.hListBox)) ShowWindow(WH_TO_HWND(FlintContext_Get()->window.hListBox), SW_HIDE);
  }
  if (refresh_callback && has_listbox && lb_ptr) {
      refresh_callback("", lb_ptr);
      if (g_win32_prompt_state) g_win32_prompt_state->listbox = *lb_ptr;
  }

  PROFILE_END_VAR(timer1, "CreateChildControl");

  PROFILE_START(timer2);

  SetLayeredWindowAttributes(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), 0, (BYTE)WindowOpacity, LWA_ALPHA);

  PROFILE_END_VAR(timer2, "WindowBs");

  PROFILE_START(timer3);

  if (IsNormalMode() && RememberLastIndex && last_index >= 0 && has_listbox) {
    RequestInitialNavigation(last_index);
  }

  ForceWindowFocus(FlintContext_Get()->window.hPopupWnd, WH_NULL);


    log_info("Platform_PromptUser spec=%p", spec);;
PROFILE_END_VAR(timer3, "Focus");

  log_info("Entering local message modal loop...");
  log_info("Entering modal loop", GetTickCount());
  MSG msg;
  int loop_counter = 0;
  UI_PreparePopupPump();
  while (!FlintContext_Get()->window.selection_complete) {
    if (GetMessage(&msg, NULL, 0, 0)) {
      if (UI_ShouldSkipPopupKey(&msg)) continue;
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      loop_counter++;
    } else {
      break; 
    }
  }
  log_info("Left local loop. Processed %d messages.", loop_counter);

  Platform_PromptPopupHideWindow(NULL);

  log_info("Flushing trailing queue entries...");
  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  struct feature_data* data = GetFeatureData(FlintContext_Get()->window.current_feature);
  if (data) {
    char* new_name = _strdup(FlintContext_Get()->window.current_feature);
    if (new_name) {
        free(data->name);
        data->name = new_name;
    }
  }


  DeleteObject(hCustomFont);
  if (g_win32_prompt_state) { ListBoxInfo_Free(&g_win32_prompt_state->listbox); free(g_win32_prompt_state); g_win32_prompt_state = NULL; lb_ptr = NULL; }
  else if (win_state) { ListBoxInfo_Free(&win_state->listbox); free(win_state); win_state = NULL; lb_ptr = NULL; }
  else if (lb_ptr) { ListBoxInfo_Free(lb_ptr); free(lb_ptr); }
  FlintContext_Get()->window.selection_complete = false;
  FlintContext_Get()->prompt_callbacks.refresh = NULL;
  FlintContext_Get()->prompt_callbacks.enter = NULL;
  FlintContext_Get()->prompt_callbacks.ctrl_enter = NULL;
  FlintContext_Get()->prompt_callbacks.tab = NULL;
  FlintContext_Get()->prompt_callbacks.copy = NULL;
  FlintContext_Get()->prompt_callbacks.render = NULL;
  FlintContext_Get()->prompt_callbacks.edit = NULL;
  FlintContext_Get()->prompt_callbacks.filter = NULL;
  FlintContext_Get()->prompt_callbacks.help = NULL;

  if (FlintContext_Get()->item_index_map) {
    shfree(FlintContext_Get()->item_index_map);
    FlintContext_Get()->item_index_map = NULL;
  }

  log_info("Returning from Platform_PromptUser. Buffer Length: %zu", strlen(FlintContext_Get()->window.output_buffer));
  { PROFILE_END_VAR(_pt, "Platform_PromptUser"); return (strlen(FlintContext_Get()->window.output_buffer) > 0) ? FlintContext_Get()->window.output_buffer : NULL; }
}
