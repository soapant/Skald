#include <windows.h>
#include <gdiplus.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "deps/log.h"
#include "config.h"
#include "core/context.h"
#include "core/modal.h"
#include "core/types.h"
#include "core/vim.h"
#include "core/hotkey_manager.h"
#include "core/feature_data.h"
#include "core/platform.h"
#include "core/window_manager.h"
#include "core/snippet_manager.h"
#include "platform/win32/win32_state.h"
#include "core/remap_manager.h"
#include "core/timer.h"
#include "bindings/bindings.h"
#include "helpers/helpers_ui.h"
#include "helpers/helpers_hotkey.h"
#include "platform/ui.h"
#include "platform/win32/subclasses_win32.h"
#include "platform/win32/common_ui_win32.h"
#include "platform/win32/popup_helpers_win32.h"

bool IsPopupActive(WindowHandle hPopupWnd) {
    (void)hPopupWnd;
    return WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) != NULL && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd));
}

void UI_SetSelection(ListBoxInfo* lb, WindowHandle hList, int index) {
    if (!lb) return;
    int old_sel = lb->selected_index;
    if (old_sel == index) return;
    lb->selected_index = index;
    RECT invalidRect = {0};
    bool hasInvalid = false;
    RECT itemRect = {0, 0, 0, 0};
    if (old_sel >= 0 && old_sel < lb->populated_count) {
        if ((int)SendMessageA(WH_TO_HWND(hList), LB_GETITEMRECT, old_sel, (LPARAM)&itemRect) != LB_ERR) {
            invalidRect = itemRect;
            hasInvalid = true;
        }
    }
    if (index >= 0 && index < lb->populated_count) {
        if ((int)SendMessageA(WH_TO_HWND(hList), LB_GETITEMRECT, index, (LPARAM)&itemRect) != LB_ERR) {
            if (!hasInvalid) { invalidRect = itemRect; hasInvalid = true; }
            else UnionRect(&invalidRect, &invalidRect, &itemRect);
        }
    }
    if (FlintContext_Get()->prompt_callbacks.render != NULL || !IsWindowVisible(WH_TO_HWND(hList))) {
        HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
        if (hPopup && IsWindow(hPopup)) InvalidateRect(hPopup, NULL, FALSE);
        return;
    }
    if (hasInvalid) RedrawWindow(WH_TO_HWND(hList), &invalidRect, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}

void UI_SetHorizontalExtent(WindowHandle hList, int width) {
    SendMessageA(WH_TO_HWND(hList), LB_SETHORIZONTALEXTENT, width, 0);
}

void UI_LB_SetCount(ListBoxInfo* lb, WindowHandle hList, int count) {
    if (lb) lb->populated_count = count;
    SendMessageA(WH_TO_HWND(hList), LB_SETCOUNT, count, 0);
}

void UI_LB_SetItemHeight(WindowHandle hList, int height) {
    SendMessageA(WH_TO_HWND(hList), LB_SETITEMHEIGHT, 0, height);
}

void UI_LB_SetCaretIndex(WindowHandle hList, int index, bool scroll) {
    SendMessageA(WH_TO_HWND(hList), LB_SETCARETINDEX, index, scroll);
}

void UI_LB_SetTopIndex(WindowHandle hList, int index) {
    SendMessageA(WH_TO_HWND(hList), LB_SETTOPINDEX, index, 0);
}

int UI_LB_GetTopIndex(WindowHandle hList) {
    return (int)SendMessageA(WH_TO_HWND(hList), LB_GETTOPINDEX, 0, 0);
}

void UI_LB_SetFont(WindowHandle hList, void* hFont) {
    SendMessageA(WH_TO_HWND(hList), WM_SETFONT, (WPARAM)hFont, FALSE);
}

int UI_LB_GetItemHeight(WindowHandle hList) {
    return (int)SendMessageA(WH_TO_HWND(hList), LB_GETITEMHEIGHT, 0, 0);
}

void UI_NavigateTo(ListBoxInfo* lb, WindowHandle hListBox, int index) {
    if (!lb) return;
    int count = lb->populated_count;
    if (count <= 0) {
        lb->selected_index = -1;
        UI_LB_SetCaretIndex(hListBox, -1, false);
        if (FlintContext_Get()->prompt_callbacks.render != NULL || !IsWindowVisible(WH_TO_HWND(hListBox))) {
            HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
            if (hPopup && IsWindow(hPopup)) InvalidateRect(hPopup, NULL, FALSE);
        }
        return;
    }
    if (index >= count) index = count - 1;
    if (index == -1) {
        lb->selected_index = -1;
        UI_LB_SetCaretIndex(hListBox, -1, false);
        if (FlintContext_Get()->prompt_callbacks.render != NULL || !IsWindowVisible(WH_TO_HWND(hListBox))) {
            HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
            if (hPopup && IsWindow(hPopup)) InvalidateRect(hPopup, NULL, FALSE);
        }
        return;
    }
    int old_sel = lb->selected_index;
    if (old_sel == index) return;
    int oldTop = UI_LB_GetTopIndex(hListBox);
    lb->selected_index = index;
    UI_LB_SetCaretIndex(hListBox, index, false);
    int itemHeight = UI_LB_GetItemHeight(hListBox);
    RECT rect;
    GetClientRect(WH_TO_HWND(hListBox), &rect);
    int visibleCount = (itemHeight > 0) ? (rect.bottom / itemHeight) : 16;
    if (visibleCount < 1) visibleCount = 1;
    int newTop = oldTop;
    if (index <= oldTop + PaddingForScrolling) newTop = index - PaddingForScrolling;
    else if (index >= oldTop + visibleCount - PaddingForScrolling) newTop = index - visibleCount + PaddingForScrolling + 1;
    if (newTop < 0) newTop = 0;
    if (newTop + visibleCount > count) newTop = count - visibleCount;
    if (newTop < 0) newTop = 0;
    if (newTop != oldTop) {
        SendMessageA(WH_TO_HWND(hListBox), WM_SETREDRAW, FALSE, 0);
        UI_LB_SetTopIndex(hListBox, newTop);
        SendMessageA(WH_TO_HWND(hListBox), WM_SETREDRAW, TRUE, 0);
    }
    if (FlintContext_Get()->prompt_callbacks.render != NULL || !IsWindowVisible(WH_TO_HWND(hListBox))) {
        HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
        if (hPopup && IsWindow(hPopup)) InvalidateRect(hPopup, NULL, FALSE);
        return;
    }
    if (newTop != oldTop) {
        RedrawWindow(WH_TO_HWND(hListBox), NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
        return;
    }
    RECT invalidRect = {0};
    bool hasInvalid = false;
    RECT itemRect;
    if (old_sel >= 0 && old_sel != index && old_sel < lb->populated_count) {
        if ((int)SendMessageA(WH_TO_HWND(hListBox), LB_GETITEMRECT, old_sel, (LPARAM)&itemRect) != LB_ERR) {
            invalidRect = itemRect;
            hasInvalid = true;
        }
    }
    if (index >= 0 && index < lb->populated_count) {
        if ((int)SendMessageA(WH_TO_HWND(hListBox), LB_GETITEMRECT, index, (LPARAM)&itemRect) != LB_ERR) {
            if (!hasInvalid) { invalidRect = itemRect; hasInvalid = true; } else UnionRect(&invalidRect, &invalidRect, &itemRect);
        }
    }
    if (hasInvalid) RedrawWindow(WH_TO_HWND(hListBox), &invalidRect, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}

void UI_NavigateUp(ListBoxInfo* lb, WindowHandle hListBox) {
    if (!lb) return;
    int sel = lb->selected_index;
    if (sel >= 0) sel--;
    if (sel == lb->selected_index) return;
    UI_NavigateTo(lb, hListBox, sel);
}

void UI_NavigateDown(ListBoxInfo* lb, WindowHandle hListBox) {
    if (!lb) return;
    int count = lb->populated_count;
    if (count <= 0) return;
    int sel = lb->selected_index;
    if (sel < 0) sel = -1;
    if (sel >= count - 1) sel = count - 1;
    else sel++;
    if (sel == lb->selected_index) return;
    UI_NavigateTo(lb, hListBox, sel);
}

int UI_LB_GetTextLen(WindowHandle hList) {
    return (int)SendMessageA(WH_TO_HWND(hList), LB_GETTEXTLEN, 0, 0);
}

int UI_LB_GetText(WindowHandle hList, int index, char* buffer) {
    return (int)SendMessageA(WH_TO_HWND(hList), LB_GETTEXT, index, (LPARAM)buffer);
}

void UI_Edit_SetFont(WindowHandle hEdit, HFONT hFont) {
    SendMessageA(WH_TO_HWND(hEdit), WM_SETFONT, (WPARAM)hFont, FALSE);
}

HFONT UI_GetFont(WindowHandle hWnd) {
    return (HFONT)SendMessageA(WH_TO_HWND(hWnd), WM_GETFONT, 0, 0);
}

void UI_Edit_UpdateUIState(WindowHandle hEdit, unsigned int action, unsigned int uiFlags) {
    SendMessageA(WH_TO_HWND(hEdit), WM_UPDATEUISTATE, MAKEWPARAM(action, uiFlags), 0);
}

void UI_Edit_SetSel(WindowHandle hEdit, uint32_t start, uint32_t end) {
    SendMessageA(WH_TO_HWND(hEdit), EM_SETSEL, start, end);
}

void UI_Edit_GetSel(WindowHandle hEdit, uint32_t* start, uint32_t* end) {
    SendMessageA(WH_TO_HWND(hEdit), EM_GETSEL, (WPARAM)start, (LPARAM)end);
}

void UI_GetCurrentTextEdit(WindowHandle edit, char* buffer, size_t cap) {
    GetWindowTextA(WH_TO_HWND(edit), buffer, (int)cap);
}

void UI_SetText(WindowHandle hEdit, char* text) {
    SetWindowTextA(WH_TO_HWND(hEdit), text);
}

void UI_SetCursorPosition(WindowHandle ctrl, int position) {
    UI_Edit_SetSel(ctrl, position, position);
}

static PromptState *g_win32_prompt_state = NULL;
void UI_RedrawNow(ListBoxInfo* lb, WindowHandle hListBox) {
    FlintContext* _c = FlintContext_Get();
    if (_c->prompt_callbacks.refresh != NULL) {
        char buffer[MaxFeatures];
        UI_GetCurrentTextEdit(FlintContext_Get()->window.hEditControl, buffer, sizeof(buffer));
        if (g_win32_prompt_state) {
            strncpy(g_win32_prompt_state->filter, buffer, sizeof(g_win32_prompt_state->filter) - 1);
            g_win32_prompt_state->filter[sizeof(g_win32_prompt_state->filter) - 1] = '\0';
            HWND hEdit = WH_TO_HWND(FlintContext_Get()->window.hEditControl);
            if (hEdit && IsWindow(hEdit)) {
                DWORD cs = 0, ce = 0;
                SendMessageA(hEdit, EM_GETSEL, (WPARAM)&cs, (LPARAM)&ce);
                g_win32_prompt_state->cursor = (size_t)cs;
            }
        }
        _c->prompt_callbacks.refresh(buffer, FlintContext_Get()->window.hListBox, lb);
    }
}

static bool g_toggle_key_pending = false;

void UI_FlushPendingInput(void) {
    MSG msg;
    while (PeekMessageA(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE)) {
        if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN) g_toggle_key_pending = false;
    }
}

void UI_PreparePopupPump(void) {
    g_toggle_key_pending = true;
    UI_FlushPendingInput();
}

bool UI_ShouldSkipPopupKey(const void* msg) {
    const MSG* m = (const MSG*)msg;
    if (!g_toggle_key_pending) return false;
    if (m->message == WM_KEYDOWN || m->message == WM_SYSKEYDOWN || m->message == WM_KEYUP || m->message == WM_SYSKEYUP || m->message == WM_CHAR || m->message == WM_SYSCHAR) {
        g_toggle_key_pending = false;
        return true;
    }
    return false;
}

void UI_Redraw(ListBoxInfo* lb, WindowHandle hListBox) {
    if (IsPopupActive(FlintContext_Get()->window.hPopupWnd)) {
        SetTimer(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), UI_DEBOUNCE_TIMER_ID, UI_DEBOUNCE_MS, NULL);
        return;
    }
    UI_RedrawNow(lb, hListBox);
}

void UI_GetParent(WindowHandle hwnd) {
    SendMessageA(GetParent(WH_TO_HWND(hwnd)), WM_COMMAND, MAKEWPARAM(IDC_INPUT_BOX, EN_CHANGE), (LPARAM)WH_TO_HWND(FlintContext_Get()->window.hEditControl));
}

char* Platform_SearchPopup(void) { log_error("operation failed, returning error"); return NULL; log_info("Platform_SearchPopup completed");
}

#include <gdiplus.h>

PromptState* Platform_GetWin32PromptState(void) { return g_win32_prompt_state; }

void Platform_WaitForKeypressAsync(KeyCallback callback) {
  g_win32_waiting_for_key = true;
  g_win32_key_callback = callback;
log_info("Platform_WaitForKeypressAsync callback=%p", callback);;
}

char* Platform_PromptUser(WindowManager* wm, const PromptSpec* spec) {
  (void)wm;
  if (!spec) { log_error("Platform_PromptUser null spec"); return NULL; }
  const char** window_list = spec->items;
  const char* window_title = spec->title ? spec->title : "Flint";
  int count = 0;
  if (window_list) {
    for (count = 0; count < 8192 && window_list[count]; count++) {}
  }
  int item_height = 32;
  RefreshCallback refresh_callback = spec->refresh;
  EnterCallback enter_callback = spec->enter;
  CtrlEnterCallback ctrl_enter_callback = spec->ctrl_enter;
  TabCallback tab_callback = spec->tab;
  CopyCallback copy_callback = spec->copy;
  RenderCallback render_callback = spec->render;
  EditCallback edit_callback = spec->edit;
  FilterCallback filter_callback = spec->filter;
  HelpCallback help_callback = spec->help;

  HINSTANCE hInstance = GetModuleHandle(NULL);
  FlintContext_Get()->window.selection_complete = false;
  FlintContext_Get()->window.output_buffer[0] = '\0';
  PROFILE_START(timer);

  if (g_win32_prompt_state) { free(g_win32_prompt_state); g_win32_prompt_state = NULL; }
  PromptState *win_state = NULL;
  ListBoxInfo* lb_ptr = NULL;
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

  FlintContext_Get()->window.hPopupWnd = WH_FROM_HWND(CreateWindowExA(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_COMPOSITED,
      "QuickSwitcherModalClass", NULL, WS_POPUP | WS_CLIPCHILDREN,
      layout.x, layout.y, layout.w, layout.h, NULL, NULL, hInstance, NULL
      ));

  if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) == NULL) {
    DeleteObject(hCustomFont);
    if (win_state) free(win_state);
    else if (lb_ptr) free(lb_ptr);
    if (g_win32_prompt_state == win_state) g_win32_prompt_state = NULL;
    return NULL;
  }
  SetWindowLongPtrA(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), GWLP_USERDATA, (LONG_PTR)(lb_ptr));
  Win32_ApplyRoundedCorners(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), layout.w, layout.h);
  if (spec->tick) SetTimer(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), UI_TICK_TIMER_ID, UI_TICK_TIMER_MS, NULL);

  PROFILE_END_VAR(timer, "Window Init");

  PROFILE_START(timer1);

  CreateChildControls(FlintContext_Get()->window.hPopupWnd, hCustomFont, window_list, count, &layout, lb_ptr);
  if (render_callback) {
    if (WH_TO_HWND(FlintContext_Get()->window.hListBox)) ShowWindow(WH_TO_HWND(FlintContext_Get()->window.hListBox), SW_HIDE);
    if (WH_TO_HWND(FlintContext_Get()->window.hEditControl)) ShowWindow(WH_TO_HWND(FlintContext_Get()->window.hEditControl), SW_HIDE);
  }
  if (refresh_callback && has_listbox && lb_ptr) {
      refresh_callback("", FlintContext_Get()->window.hListBox, lb_ptr);
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

  ForceWindowFocus(FlintContext_Get()->window.hPopupWnd, FlintContext_Get()->window.hEditControl);


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

  if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) != NULL && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd))) {
    KillTimer(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), 999);
    KillTimer(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), UI_TICK_TIMER_ID);
    DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd));
    FlintContext_Get()->window.hPopupWnd = WH_NULL;
  }

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
  if (g_win32_prompt_state) { free(g_win32_prompt_state); g_win32_prompt_state = NULL; lb_ptr = NULL; }
  else if (win_state) { free(win_state); win_state = NULL; lb_ptr = NULL; }
  else if (lb_ptr) free(lb_ptr);
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
  return (strlen(FlintContext_Get()->window.output_buffer) > 0) ? FlintContext_Get()->window.output_buffer : NULL;
}

void Platform_DisplayMessage(const char* message, int centerX, int centerY, int maxWidth, int maxHeight, int timeout, ColorRef color) {
  if (WH_TO_HWND(FlintContext_Get()->window.hGlobalTrayWnd) != NULL && GetCurrentThreadId() != g_win32_main_thread_id) {
    DisplayMessageParams params = { message, centerX, centerY, maxWidth, maxHeight, timeout, color };
    SendMessageA(WH_TO_HWND(FlintContext_Get()->window.hGlobalTrayWnd), WM_DISPLAY_MESSAGE, 0, (LPARAM)&params);
    return;
  }
  Platform_CreateDisplayMessageWindow(message, centerX, centerY, maxWidth, maxHeight, timeout, color);
log_info("Platform_DisplayMessage message='%s' centerX=%d centerY=%d maxWidth=%d maxHeight=%d timeout=%d color=%p", message ? message : "", centerX, centerY, maxWidth, maxHeight, timeout, color);;
}

void Platform_CreateDisplayMessageWindow(const char* message, int centerX, int centerY, int maxWidth, int maxHeight, int timeout, ColorRef color) {
  HINSTANCE hInstance = GetModuleHandle(NULL);

  DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage));
  KillTimer(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage),999);

  FontHandle msgFont = Platform_CreateFont(FontSize, UsedFont);
  int textWidth = 0, textHeight = 0;
  Platform_GetTextExtents(msgFont, message, maxWidth - 30, &textWidth, &textHeight);

  int padding = 30;
  int winWidth = textWidth + padding;
  int winHeight = textHeight + padding;

  if (winWidth > maxWidth) winWidth = maxWidth;
  if (winHeight > maxHeight) winHeight = maxHeight;

  FadeWindowData* pWndData = (FadeWindowData*)malloc(sizeof(FadeWindowData));
  if (!pWndData) {
    Platform_DestroyFont(msgFont);
    return;
  }

  pWndData->message = _strdup(message);
  pWndData->hFont = msgFont;
  pWndData->alpha = WindowOpacity;
  pWndData->color = color;
  g_win32_timeout = timeout * 20;

  int topLeftX = centerX - (winWidth / 2);
  int topLeftY = centerY - (winHeight / 2);

  HWND hPopupWnd = CreateWindowExA(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE, 
      "DisplayMessageModalClass", NULL, WS_POPUP, 
      topLeftX, topLeftY, winWidth, winHeight, NULL, NULL, hInstance, pWndData
      );

  if (hPopupWnd == NULL) {
    Platform_DestroyFont(pWndData->hFont);
    free(pWndData);
    return;
  }
  Win32_ApplyRoundedCorners(hPopupWnd, winWidth, winHeight);

  SetLayeredWindowAttributes(hPopupWnd, 0, (BYTE)pWndData->alpha, LWA_ALPHA);

  ShowWindow(hPopupWnd, SW_SHOWNOACTIVATE);
  UpdateWindow(hPopupWnd);

  SetTimer(hPopupWnd, 999, 50, NULL);
  log_info("Platform_CreateDisplayMessageWindow message='%s' centerX=%d centerY=%d maxWidth=%d maxHeight=%d timeout=%d", message ? message : "", centerX, centerY, maxWidth, maxHeight, timeout);
  return; 
}

void Platform_SettingsPopUp() {
  HINSTANCE hInstance = GetModuleHandle(NULL);
  int height = 700;
  int width = 900;
  int item_height = 32;

  SetMode(MODE_INSERT);

  WindowLayout layout;
  CalculateWindowMetrics(&layout, height, width, item_height);

  HFONT hCustomFont = CreateCustomNerdFont(FontSize, UsedFont);

  FlintContext_Get()->window.hPopupWnd = WH_FROM_HWND(CreateWindowExA(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_COMPOSITED,
      "SettingsPopupClass", NULL, WS_POPUP | WS_CLIPCHILDREN, 
      510, 210, width, height, NULL, NULL, hInstance, NULL
      ));

  if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) == NULL) {
    DWORD err = GetLastError();
    log_error("CreateWindowExA failed: %lu", err);
    DeleteObject(hCustomFont);
    return;
  }
  Win32_ApplyRoundedCorners(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), width, height);

  FlintContext_Get()->window.hSettings = FlintContext_Get()->window.hPopupWnd;
  SetTimer(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), 999, 50, NULL);

  SettingsCreateChildControls(FlintContext_Get()->window.hPopupWnd, hCustomFont,&layout);

  SetLayeredWindowAttributes(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), 0, (BYTE)WindowOpacity, LWA_ALPHA);

  ForceWindowFocus(FlintContext_Get()->window.hPopupWnd, FlintContext_Get()->window.hEditControl);

  MSG msg;
  UI_PreparePopupPump();
  while (!FlintContext_Get()->window.selection_complete) {
    if (GetMessage(&msg, NULL, 0, 0)) {
      if (UI_ShouldSkipPopupKey(&msg)) continue;
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      break; 
    }
  }

  if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) != NULL && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd))) { 
    KillTimer(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), 999);
    DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd));
    FlintContext_Get()->window.hPopupWnd = WH_NULL;
  }
  FlintContext_Get()->window.hSettings = WH_NULL;

  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  DeleteObject(hCustomFont); 
log_info("Platform_SettingsPopUp completed");
}

int Platform_InitializeTrayIcon() {
  HINSTANCE hInstance = GetModuleHandle(NULL);
  if (Tray) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "TrayAppClass";

    if (!RegisterClass(&wc)) {
            log_error("operation failed, returning error");
return -1;
    }

    FlintContext_Get()->window.hGlobalTrayWnd = WH_FROM_HWND(CreateWindowExA(
        WS_EX_TOOLWINDOW,
        wc.lpszClassName, "TrayApp", 
        WS_POPUP, 
        0, 0, 0, 0, 
        NULL, NULL, hInstance, NULL
        ));

    if (WH_TO_HWND(FlintContext_Get()->window.hGlobalTrayWnd) == NULL) {
      return -1;
    }
  }
log_info("Platform_InitializeTrayIcon completed");
  return 0;
}

void HandleCommandExecution(ListBoxInfo* lb, char* string) {
    if (!lb) return;
    int num = atoi(string);
    UI_SetSelection(lb, FlintContext_Get()->window.hListBox, num - 1);
    UI_LB_SetCaretIndex(FlintContext_Get()->window.hListBox, num, TRUE);
}

static void win_command_confirm(const char* text, void* user_data) {
  ListBoxInfo* lb = (ListBoxInfo*)user_data;
  if (!text) return;
  char* dup = _strdup(text);
  if (!dup) return;
  HandleCommandExecution(lb, dup);
  free(dup);
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode < 0) {
    return CallNextHookEx(NULL, nCode, wParam, lParam);
  }

  if (nCode == HC_ACTION) {
    bool _popupActive = false;
    WindowManager* _wm = WindowManager_Get();
    if (_wm && WindowManager_GetTopPopup(_wm)) _popupActive = true;
    if (! _popupActive && WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd))) _popupActive = true;
    KBDLLHOOKSTRUCT* _rk = (KBDLLHOOKSTRUCT*)lParam;
    if (_rk->flags & LLKHF_INJECTED) return CallNextHookEx(NULL, nCode, wParam, lParam);
    {
        bool _isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        bool _isUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);
        if (_isDown || _isUp) {
            int _vk = (int)_rk->vkCode;
            int _val = _isDown ? 1 : 0;
            if (remap_should_swallow_vk(_vk, _val)) {
                char* _t = Platform_GetActiveWindowTitle();
                log_info("hook swallow=remap vk=0x%X val=%d title='%s'", _vk, _val, _t ? _t : "");
                return 1;
            }
        }
    }
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
    if (g_win32_timeout == 6969 && WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage) && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage))) {
      KillTimer(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage), 999);
      DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage));
      FlintContext_Get()->window.hDisplayMessage = WH_NULL;
    }
    KBDLLHOOKSTRUCT* pKeyInfo = (KBDLLHOOKSTRUCT*)lParam;

    bool alt_down   = (pKeyInfo->flags & LLKHF_ALTDOWN) || (GetAsyncKeyState(VK_MENU) & 0x8000);
    bool ctrl_down  = (GetAsyncKeyState(VK_CONTROL) & 0x8000);
    bool shift_down = (GetAsyncKeyState(VK_SHIFT) & 0x8000) || 
      (GetAsyncKeyState(VK_LSHIFT) & 0x8000) || 
      (GetAsyncKeyState(VK_RSHIFT) & 0x8000);

    double seconds = Platform_GetUniversalTimeSeconds();
    if (seconds >= 300) {
      Platform_ResetUniversalTimer();
      log_info("SUPPOSED TO CLEAR FILE");
    }


    int pressed_vk = (int)pKeyInfo->vkCode;
    int modifiers = HOTKEY_MOD_NONE;
    if (alt_down) modifiers |= HOTKEY_MOD_ALT;
    if (shift_down) modifiers |= HOTKEY_MOD_SHIFT;
    if (ctrl_down) modifiers |= HOTKEY_MOD_CTRL;

    PROFILE_START(timer);
    if (! _popupActive && hotkey_process(pressed_vk, modifiers)) {
      char* _t2 = Platform_GetActiveWindowTitle();
      log_info("hook swallow=hotkey vk=0x%X mods=%d title='%s'", pressed_vk, modifiers, _t2 ? _t2 : "");
      return 1;
    }
    PROFILE_END_VAR(timer, "ProcessHotkey");

    PROFILE_START(timer3);

    if (IsNormalMode()) {
      if (g_win32_waiting_for_key) {
        g_win32_waiting_for_key = false;
        if (g_win32_key_callback) {
          g_win32_key_callback(pKeyInfo->vkCode);
          PROFILE_END_VAR(timer3, "KeyCallback");
        }
        log_info("hook swallow=waiting_for_key vk=0x%X", pKeyInfo->vkCode);
        return 1;
      }
    }
    PROFILE_END_VAR(timer3, "NormalModeCheck");


  }

  }
  if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
    WindowManager* wmx = WindowManager_Get();
    if (!wmx || !WindowManager_GetTopPopup(wmx)) {
      KBDLLHOOKSTRUCT* rk = (KBDLLHOOKSTRUCT*)lParam;
      if (snippet_feed_vk((int)rk->vkCode, 1, true)) {
        log_info("hook swallow=snippet vk=0x%X", (int)rk->vkCode);
        return 1;
      }
    }
  }
  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

static ULONG_PTR g_gdiplusToken = 0;

void Platform_InitUI() {
  struct GdiplusStartupInput gdiplusStartupInput;
  gdiplusStartupInput.GdiplusVersion = 1;
  gdiplusStartupInput.DebugEventCallback = NULL;
  gdiplusStartupInput.SuppressBackgroundThread = FALSE;
  gdiplusStartupInput.SuppressExternalCodecs = FALSE;
  GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);


  WNDCLASSA wc = {0};
  wc.lpfnWndProc = PopupWndProc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = GetStockObject(NULL_BRUSH);
  wc.lpszClassName = "QuickSwitcherModalClass";

  if (RegisterClassA(&wc)) {
    log_info("QuickSwitcherModalClass registered successfully with OS.");
  } else {
    log_error("CRITICAL WARNING: Failed to register window class! Error: %lu", GetLastError());
  }

  WNDCLASSA wcFade = {0};
  wcFade.lpfnWndProc = DisplayMessageProc;
  wcFade.hInstance = GetModuleHandle(NULL);
  wcFade.hCursor = LoadCursor(NULL, IDC_ARROW);
  wcFade.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1); 
  wcFade.lpszClassName = "DisplayMessageModalClass";
  RegisterClassA(&wcFade);

  WNDCLASSA editPopupWc = {0};
  editPopupWc.lpfnWndProc = EditPopupProc;
  editPopupWc.hInstance = GetModuleHandle(NULL);
  editPopupWc.hCursor = LoadCursor(NULL, IDC_ARROW);
  editPopupWc.hbrBackground = CreateSolidBrush(COLOR_BG);
  editPopupWc.lpszClassName = "EditPopupClass";
  RegisterClassA(&editPopupWc);

  WNDCLASSA searchPopupWc = {0};
  searchPopupWc.lpfnWndProc = SearchPopupProc;
  searchPopupWc.hInstance = GetModuleHandle(NULL);
  searchPopupWc.hCursor = LoadCursor(NULL, IDC_ARROW);
  searchPopupWc.hbrBackground = CreateSolidBrush(COLOR_BG);
  searchPopupWc.lpszClassName = "SearchPopupClass";
  RegisterClassA(&searchPopupWc);

   WNDCLASSA settingsPopupWc = {0};
   settingsPopupWc.lpfnWndProc = SettingsPopupProc;
   settingsPopupWc.hInstance = GetModuleHandle(NULL);
   settingsPopupWc.hCursor = LoadCursor(NULL, IDC_ARROW);
   settingsPopupWc.hbrBackground = CreateSolidBrush(COLOR_BG);
   settingsPopupWc.lpszClassName = "SettingsPopupClass";
   RegisterClassA(&settingsPopupWc);

   g_win32_hKeyboardHook = SetWindowsHookExA(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);

   FontHandle warm = Platform_CreateFont(FontSize, UsedFont);
   Platform_DestroyFont(warm);
log_info("Platform_InitUI completed");
}

void Platform_ShutdownUI() {
  extern void Win32_FreeBgCache(void);
  Win32_FreeBgCache();
  if (g_win32_hKeyboardHook) {
    UnhookWindowsHookEx(g_win32_hKeyboardHook);
    g_win32_hKeyboardHook = NULL;
  }
  if (g_gdiplusToken != 0) {
    GdiplusShutdown(g_gdiplusToken);
  }
log_info("Platform_ShutdownUI completed");
}
