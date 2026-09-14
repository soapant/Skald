#include <windows.h>
#include "deps/log.h"
#include "core/context.h"
#include "core/modal.h"
#include "core/types.h"
#include "core/hotkey_manager.h"
#include "core/number_shortcut.h"
#include "core/feature_data.h"
#include "platform/subclasses.h"
#include "core/platform.h"
#include "bindings/bindings.h"
#include "helpers/flag.h"
#include "core/common_ui.h"
#include "core/window_manager.h"
#include "core/remap_manager.h"


char* Platform_SearchPopup(void) { log_error("operation failed, returning error"); return NULL; log_info("Platform_SearchPopup completed");
}

#include <gdiplus.h>

static PromptState *g_win32_prompt_state = NULL;
PromptState* Platform_GetWin32PromptState(void) { return g_win32_prompt_state; }

void Platform_WaitForKeypressAsync(KeyCallback callback) {
  FlintContext_Get()->g_waiting_for_key = true;
  FlintContext_Get()->g_key_callback = callback;
log_info("Platform_WaitForKeypressAsync callback=%p", callback);;
}

char* Platform_PromptUser(const PromptSpec* spec) {
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

  {
    const char* p = strchr(window_title, ' ');
    TimerEnd(FlintContext_Get()->g_timer, p ? p+1 : window_title);
  }
  HINSTANCE hInstance = GetModuleHandle(NULL);
  FlintContext_Get()->window.selection_complete = false;
  FlintContext_Get()->window.output_buffer[0] = '\0';
  PROFILE_START(timer);

  if (g_win32_prompt_state) { free(g_win32_prompt_state); g_win32_prompt_state = NULL; }
  PromptState *win_state = NULL;
  ListBoxInfo* lb_ptr = NULL;
  bool has_listbox = (window_list || spec->refresh);
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

  if (FlintContext_Get()->g_item_index_map) {
    HashMap_Destroy(FlintContext_Get()->g_item_index_map);
    FlintContext_Get()->g_item_index_map = NULL;
  }
  if (window_list && count > 0) {
    FlintContext_Get()->g_item_index_map = HashMap_Create(count);
    for (int i = 0; i < count; i++) {
      if (window_list[i]) {
        HashMap_Put(FlintContext_Get()->g_item_index_map, window_list[i], (void*)(size_t)(i + 1));
      }
    }
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
    FlintContext_Get()->window.current_feature = (char*)(p ? p+1 : window_title);
  }
  SetCurrentFeatureName(FlintContext_Get()->window.current_feature);
  BuildAliasDisplayItems(window_list, count);
  extern EditorMode g_last_mode;

  if (ModePersistsThroughFeatures) {
    SetModeWithListBox(g_last_mode, lb_ptr);
  } else {
    SetModeWithListBox(MODE_INSERT, lb_ptr);
  }

  WindowLayout layout;
  CalculateWindowMetrics(0, &layout, 640, 900, item_height);
  if (win_state) {
    win_state->width = layout.w;
    win_state->height = layout.h;
  }

  HFONT hCustomFont = CreateCustomNerdFont(FontSize, UsedFont);

  FlintContext_Get()->window.hPopupWnd = WH_FROM_HWND(CreateWindowExA(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_COMPOSITED,
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

  PROFILE_END_VAR(timer, "Window Init");

  PROFILE_START(timer1);

  CreateChildControls(FlintContext_Get()->window.hPopupWnd, hCustomFont, window_list, count, &layout, lb_ptr);
  if (render_callback) {
    if (WH_TO_HWND(FlintContext_Get()->window.hListBox)) ShowWindow(WH_TO_HWND(FlintContext_Get()->window.hListBox), SW_HIDE);
  }
  if (!window_list && refresh_callback && has_listbox && lb_ptr) {
      refresh_callback("", FlintContext_Get()->window.hListBox, lb_ptr);
      if (g_win32_prompt_state) g_win32_prompt_state->listbox = *lb_ptr;
  }

  PROFILE_END_VAR(timer1, "CreateChildControl");

  PROFILE_START(timer2);

  SetLayeredWindowAttributes(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), 0, WindowOpacity, LWA_ALPHA);

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
    DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd));
    FlintContext_Get()->window.hPopupWnd = WH_NULL;
  }
  ResetNumberShortcutState();

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
    data->items = window_list;
    data->item_count = count;
    data->refresh_callback = refresh_callback;
    data->enter_callback = enter_callback;
    data->ctrl_enter_callback = ctrl_enter_callback;
    data->tab_callback = tab_callback;
    data->copy_callback = copy_callback;
    data->render_callback = render_callback;
    data->edit_callback = edit_callback;
    data->filter_callback = filter_callback;
    data->help_callback = help_callback;
    data->filter_match = NULL;
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

  if (FlintContext_Get()->g_item_index_map) {
    HashMap_Destroy(FlintContext_Get()->g_item_index_map);
    FlintContext_Get()->g_item_index_map = NULL;
  }

  log_info("Returning from Platform_PromptUser. Buffer Length: %zu", strlen(FlintContext_Get()->window.output_buffer));
  return (strlen(FlintContext_Get()->window.output_buffer) > 0) ? FlintContext_Get()->window.output_buffer : NULL;
}

char* Platform_SimplePromptUser(const SimplePromptSpec* spec) {
  if (!spec) { log_error("Platform_SimplePromptUser null spec"); return NULL; }
  const char* prompt = spec->prompt;
  int item_height = 32;

  HINSTANCE hInstance = GetModuleHandle(NULL);
  WindowHandle saved_hPopupWnd = FlintContext_Get()->window.hPopupWnd;
  WindowHandle saved_hListBox = FlintContext_Get()->window.hListBox;
  WindowHandle saved_hEditControl = FlintContext_Get()->window.hEditControl;
  WindowHandle saved_gActive = FlintContext_Get()->window.g_ActiveEditControl;
  PromptState *saved_state = g_win32_prompt_state;
  char *saved_title = FlintContext_Get()->window.title;
  char *saved_feature = FlintContext_Get()->window.current_feature;
  FlintContext_Get()->window.selection_complete = false;
  FlintContext_Get()->window.output_buffer[0] = '\0';
  SetCurrentFeatureName(FlintContext_Get()->window.current_feature);

  SetMode(MODE_INSERT);

  int w = spec->w ? spec->w : 900;
  int h = spec->h ? spec->h : 640;
  int x = spec->x;
  int y = spec->y;
  if (!x || !y) {
    POINT ptPrimary = {0, 0};
    HMONITOR hMon = MonitorFromPoint(ptPrimary, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = {0};
    mi.cbSize = sizeof(MONITORINFO);
    GetMonitorInfoA(hMon, &mi);
    int monW = mi.rcMonitor.right - mi.rcMonitor.left;
    int monH = mi.rcMonitor.bottom - mi.rcMonitor.top;
    if (!x) x = mi.rcMonitor.left + (monW - w) / 2;
    if (!y) y = mi.rcMonitor.top + (monH - h) / 2;
  }
  WindowLayout layout;
  layout.w = w;
  layout.h = h;
  layout.x = x;
  layout.y = y;
  layout.item_height = item_height;

  HFONT hCustomFont = CreateCustomNerdFont(FontSize, UsedFont);

  FlintContext_Get()->window.hPopupWnd = WH_FROM_HWND(CreateWindowExA(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
      "QuickSwitcherModalClass", NULL, WS_POPUP | WS_CLIPCHILDREN,
      layout.x, layout.y, layout.w, layout.h, NULL, NULL, hInstance, NULL
      ));


  SetTimer(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), 999, 50, NULL);

  if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) == NULL) {
    DWORD err = GetLastError();
    if (err == 1407) {
    }
    DeleteObject(hCustomFont);
        log_error("operation failed, returning error spec=%p", spec);
return NULL;
  }
  Win32_ApplyRoundedCorners(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), layout.w, layout.h);

  SimpleCreateChildControls(FlintContext_Get()->window.hPopupWnd, hCustomFont, prompt, 1, &layout);

  SetLayeredWindowAttributes(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), 0, WindowOpacity, LWA_ALPHA);

  ForceWindowFocus(FlintContext_Get()->window.hPopupWnd, FlintContext_Get()->window.hEditControl);

  MSG msg;
  int loop_counter = 0;
  UI_PreparePopupPump();
  while (!FlintContext_Get()->window.selection_complete) {
    if (GetMessage(&msg, NULL, 0, 0)) {
      if (UI_ShouldSkipPopupKey(&msg)) continue;
      loop_counter++;
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      break;
    }
  }

  HWND hInnerWnd = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
  if (hInnerWnd != NULL && IsWindow(hInnerWnd)) {
    KillTimer(hInnerWnd, 999);
    DestroyWindow(hInnerWnd);
  }
  FlintContext_Get()->window.hPopupWnd = saved_hPopupWnd;
  FlintContext_Get()->window.hListBox = saved_hListBox;
  FlintContext_Get()->window.hEditControl = saved_hEditControl;
  FlintContext_Get()->window.g_ActiveEditControl = saved_gActive;
  g_win32_prompt_state = saved_state;
  FlintContext_Get()->window.title = saved_title;
  FlintContext_Get()->window.current_feature = saved_feature;
  if (WH_TO_HWND(saved_hPopupWnd) != NULL && IsWindow(WH_TO_HWND(saved_hPopupWnd))) {
    ForceWindowFocus(saved_hPopupWnd, saved_hEditControl);
    InvalidateRect(WH_TO_HWND(saved_hPopupWnd), NULL, TRUE);
    UpdateWindow(WH_TO_HWND(saved_hPopupWnd));
    if (WH_TO_HWND(saved_hListBox) != NULL) {
      InvalidateRect(WH_TO_HWND(saved_hListBox), NULL, TRUE);
      UpdateWindow(WH_TO_HWND(saved_hListBox));
    }
  }

  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  DeleteObject(hCustomFont); 
  FlintContext_Get()->window.selection_complete = false;
  log_info("Returning from Platform_SimplePromptUser. Buffer %s", FlintContext_Get()->window.output_buffer);
log_info("Platform_SimplePromptUser spec=%p", spec);;
  return (strlen(FlintContext_Get()->window.output_buffer) > 0) ? FlintContext_Get()->window.output_buffer : NULL;
}

void Platform_DisplayMessage(const char* message, int centerX, int centerY, int maxWidth, int maxHeight, int timeout, ColorRef color) {
  if (WH_TO_HWND(FlintContext_Get()->window.hGlobalTrayWnd) != NULL && GetCurrentThreadId() != FlintContext_Get()->g_main_thread_id) {
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
  FlintContext_Get()->g_timeout = timeout * 20;

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

  SetLayeredWindowAttributes(hPopupWnd, 0, pWndData->alpha, LWA_ALPHA);

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
  CalculateWindowMetrics(1, &layout, height, width, item_height);

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

  SetLayeredWindowAttributes(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), 0, WindowOpacity, LWA_ALPHA);

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
    KBDLLHOOKSTRUCT* _rk = (KBDLLHOOKSTRUCT*)lParam;
    if (_rk->flags & LLKHF_INJECTED) return CallNextHookEx(NULL, nCode, wParam, lParam);
    bool _isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    bool _isUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);
    if (_isDown || _isUp) {
        int _vk = (int)_rk->vkCode;
        int _val = _isDown ? 1 : 0;
        if (remap_should_swallow_vk(_vk, _val)) { log_warn("ui_win32: remap swalowed vk=%d val=%d", _vk, _val); return 1; }
        else { if (_vk==VK_LSHIFT||_vk==VK_RMENU||_vk=='W'||_vk=='A'||_vk=='S'||_vk=='D'||_vk=='I'||_vk=='U') log_warn("ui_win32: remap NOT swallowed vk=%d val=%d", _vk, _val); }
    }
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
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
    if (hotkey_process(pressed_vk, modifiers)) {
      return 1;
    }
    PROFILE_END_VAR(timer, "ProcessHotkey");

    if (shift_down && !IsInsertMode() && WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) != NULL) {
      if (WH_TO_HWND(FlintContext_Get()->window.g_hEditPopup) == NULL) {
        if (pressed_vk == COMMAND_MODE_KEY) {
          SetMode(MODE_INSERT);
          WindowManager* wm = WindowManager_Get();
          if (wm) {
            ListBoxInfo* lb = NULL;
            HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
            if (hPopup && IsWindow(hPopup)) {
              lb = (ListBoxInfo*)GetWindowLongPtrA(hPopup, GWLP_USERDATA);
            }
            Popup* popup = EditPopup_Create(wm, "", "Command Mode", 960, 540, win_command_confirm, lb);
            if (popup) WindowManager_PushPopup(wm, popup);
          }
          return 1;
        }
      }
    }
    if (WH_TO_HWND(FlintContext_Get()->window.g_hSearchPopup) == NULL && WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) != NULL && IsNormalMode()) {
      if (pressed_vk == SEARCH_PROGRAM_KEY) {
        Platform_SearchPopup();
      }
    }

    PROFILE_START(timer2);
    if (ctrl_down) {
      if (pressed_vk == HELP_SCREEN_KEY) {
        if (FlintContext_Get()->prompt_callbacks.help != NULL) {
          FlintContext_Get()->prompt_callbacks.help();
          PROFILE_END_VAR(timer2, "HelpScreen");
        }
      }
    }
    PROFILE_END_VAR(timer2, "HelpScreenCheck");

    int matchedDelay = 0;

    MotionAction vimAction = MOTION_NONE;
    const char* matchedSequence = NULL;
    int typedCount = 0;

    PROFILE_START(timer3);

    if (IsNormalMode()) {
      if (FlintContext_Get()->g_waiting_for_key) {
        FlintContext_Get()->g_waiting_for_key = false;
        if (FlintContext_Get()->g_key_callback) {
          FlintContext_Get()->g_key_callback(pKeyInfo->vkCode);
          PROFILE_END_VAR(timer3, "KeyCallback");
        }
        return 1;
      }       
    }
    PROFILE_END_VAR(timer3, "NormalModeCheck");

    PROFILE_START(timer4);
    if (CheckVimMotion(pressed_vk, modifiers, &vimAction, &matchedSequence, &typedCount)) {
      ExecuteMotion(vimAction, matchedSequence, typedCount);
      PROFILE_END_VAR(timer4, "ExecuteMotion");
      return 1;
    }
    PROFILE_END_VAR(timer4, "CheckVimMotion");


  }
  }
  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void Platform_RunHotKeyMessageLoop() {
  HINSTANCE hInstance = GetModuleHandle(NULL);

  FlintContext_Get()->hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);
  if (!FlintContext_Get()->hKeyboardHook) {
    log_error("Failed to install keyboard hook! Error: %lu", GetLastError());
    return;
  }

  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  if (FlintContext_Get()->hKeyboardHook) UnhookWindowsHookEx(FlintContext_Get()->hKeyboardHook);
log_info("Platform_RunHotKeyMessageLoop completed");
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

   FlintContext_Get()->hKeyboardHook = SetWindowsHookExA(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
log_info("Platform_InitUI completed");
}

void Platform_ShutdownUI() {
  extern void Win32_FreeBgCache(void);
  Win32_FreeBgCache();
  if (FlintContext_Get()->hKeyboardHook) {
    UnhookWindowsHookEx(FlintContext_Get()->hKeyboardHook);
    FlintContext_Get()->hKeyboardHook = NULL;
  }
  if (g_gdiplusToken != 0) {
    GdiplusShutdown(g_gdiplusToken);
  }
log_info("Platform_ShutdownUI completed");
}
