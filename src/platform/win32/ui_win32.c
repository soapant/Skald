#include <windows.h>
#include "common.h"
#include "core/modal.h"
#include "core/types.h"
#include "helpers.h"
#include "core/common_ui.h"
#include "core/number_shortcut.h"
#include "core/feature_data.h"
#include "window/subclasses.h"
#include "core/platform.h"
#include "bindings/bindings.h"
#include "bindings/hotkey_definitions.h"
#include "bindings/macro_bindings.h"
#include "core/vim.h"
#include "features/focus_switcher.h"
#include "features/music_player.h"
#include "features/video_viewer.h"
#include "features/process_killer.h"
#include <gdiplus.h>

void Platform_WaitForKeypressAsync(KeyCallback callback) {
  g_waiting_for_key = true;
  g_key_callback = callback;
}

char* Platform_PromptUser(const PromptSpec* spec) {
  const char** window_list = spec->items;
  const char* window_title = spec->title;
  int count = spec->count;
  int height = spec->height;
  int width = spec->width;
  int item_height = spec->item_height;
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
    TimerEnd(g_timer, p ? p+1 : window_title);
  }
  HINSTANCE hInstance = GetModuleHandle(NULL);
  selection_complete = false;
  output_buffer[0] = '\0';
  WinTimer timer = TimerStart();

  ListBoxInfo* lb_ptr = NULL;
  bool has_listbox = (count >= 0);
  if (has_listbox) {
    lb_ptr = (ListBoxInfo*)calloc(1, sizeof(ListBoxInfo));
    if (!lb_ptr) return NULL;
    ListBoxInfo_Init(lb_ptr);
  }

  if (g_item_index_map) {
    HashMap_Destroy(g_item_index_map);
    g_item_index_map = NULL;
  }
  if (window_list && count > 0) {
    g_item_index_map = HashMap_Create(count);
    for (int i = 0; i < count; i++) {
      if (window_list[i]) {
        HashMap_Put(g_item_index_map, window_list[i], (void*)(size_t)(i + 1));
      }
    }
  }
  g_refresh_callback = refresh_callback;
  g_enter_callback = enter_callback;
  g_ctrl_enter_callback = ctrl_enter_callback;
  g_tab_callback = tab_callback;
  g_copy_callback = copy_callback;
  g_render_callback = render_callback;
  g_edit_callback = edit_callback;
  g_filter_callback = filter_callback;
  g_help_callback = help_callback;
  g_title = window_title;
  {
    const char* p = strchr(window_title, ' ');
    g_current_feature = (char*)(p ? p+1 : window_title);
  }
  SetCurrentFeatureName(g_current_feature);
  BuildAliasDisplayItems(window_list, count);

  if (ModePersistsThroughFeatures) {
    SetModeWithListBox(g_last_mode, lb_ptr);
  } else {
    SetModeWithListBox(MODE_INSERT, lb_ptr);
  }

  WindowLayout layout;
  CalculateWindowMetrics(count, &layout, height, width, item_height);

  HFONT hCustomFont = CreateCustomNerdFont(FontSize, UsedFont);

  hPopupWnd = WH_FROM_HWND(CreateWindowExA(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_COMPOSITED,
      "QuickSwitcherModalClass", NULL, WS_POPUP | WS_CLIPCHILDREN, 
      layout.x, layout.y, layout.w, layout.h, NULL, NULL, hInstance, NULL
      ));

  if (WH_TO_HWND(hPopupWnd) == NULL) {
    DeleteObject(hCustomFont);
    if (lb_ptr) free(lb_ptr);
    return NULL;
  }
  SetWindowLongPtrA(WH_TO_HWND(hPopupWnd), GWLP_USERDATA, (LONG_PTR)(lb_ptr));

  TimerEnd(timer,"Window Init");

  WinTimer timer1 = TimerStart();


  CreateChildControls(hPopupWnd, hCustomFont, window_list, count, &layout, lb_ptr);

  TimerEnd(timer1,"CreateChildControl");

  WinTimer timer2 = TimerStart();

  SetLayeredWindowAttributes(WH_TO_HWND(hPopupWnd), 0, WindowOpacity, LWA_ALPHA);

  TimerEnd(timer2,"WindowBs");

  WinTimer timer3 = TimerStart();

  if (IsNormalMode() && RememberLastIndex && last_index >= 0 && has_listbox) {
    RequestInitialNavigation(last_index);
  }

  ForceWindowFocus(hPopupWnd, hEditControl);


  TimerEnd(timer3,"Focus");

  printf("[DEBUG-UI] Entering local message modal loop...\n");
  MSG msg;
  int loop_counter = 0;
  UI_PreparePopupPump();
  while (!selection_complete) {
    if (GetMessage(&msg, NULL, 0, 0)) {
      if (UI_ShouldSkipPopupKey(&msg)) continue;
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      loop_counter++;
    } else {
      break; 
    }
  }
  printf("[DEBUG-UI] Left local loop. Processed %d messages.\n", loop_counter);

  if (WH_TO_HWND(hPopupWnd) != NULL && IsWindow(WH_TO_HWND(hPopupWnd))) {
    KillTimer(WH_TO_HWND(hPopupWnd), 999);
    DestroyWindow(WH_TO_HWND(hPopupWnd));
    hPopupWnd = WH_NULL;
  }
  ResetNumberShortcutState();

  printf("[DEBUG-UI] Flushing trailing queue entries...\n");
  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  struct feature_data* data = GetFeatureData(g_current_feature);
  if (data) {
    char* new_name = _strdup(g_current_feature);
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
  if (lb_ptr) free(lb_ptr);
  selection_complete = false;
  g_refresh_callback = NULL;
  g_enter_callback = NULL;
  g_ctrl_enter_callback = NULL;
  g_tab_callback = NULL;
  g_copy_callback = NULL;
  g_render_callback = NULL;
  g_edit_callback = NULL;
  g_filter_callback = NULL;
  g_help_callback = NULL;

  if (g_item_index_map) {
    HashMap_Destroy(g_item_index_map);
    g_item_index_map = NULL;
  }

  printf("[DEBUG-UI] Returning from Platform_PromptUser. Buffer Length: %zu\n", strlen(output_buffer));
  return (strlen(output_buffer) > 0) ? output_buffer : NULL;
}

char* Platform_SimplePromptUser(const SimplePromptSpec* spec) {
  const char* prompt = spec->prompt;
  const char* window_title = spec->title;
  int height = spec->height;
  int width = spec->width;
  int item_height = spec->item_height;

  HINSTANCE hInstance = GetModuleHandle(NULL);
  selection_complete = false;
  output_buffer[0] = '\0';
  g_title = window_title;
  {
    const char* p = strchr(window_title, ' ');
    g_current_feature = (char*)(p ? p+1 : window_title);
  }
  SetCurrentFeatureName(g_current_feature);

  SetMode(MODE_INSERT);

  WindowLayout layout;
  CalculateWindowMetrics(1, &layout, height, width, item_height);

  HFONT hCustomFont = CreateCustomNerdFont(FontSize, UsedFont);

  hPopupWnd = WH_FROM_HWND(CreateWindowExA(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, 
      "QuickSwitcherModalClass", NULL, WS_POPUP | WS_CLIPCHILDREN, 
      layout.x, layout.y, layout.w, layout.h, NULL, NULL, hInstance, NULL
      ));


  SetTimer(WH_TO_HWND(hPopupWnd), 999, 50, NULL);

  if (WH_TO_HWND(hPopupWnd) == NULL) {
    DWORD err = GetLastError();
    if (err == 1407) {
    }
    DeleteObject(hCustomFont);
    return NULL;
  }

  SimpleCreateChildControls(hPopupWnd, hCustomFont, prompt, 1, &layout);

  SetLayeredWindowAttributes(WH_TO_HWND(hPopupWnd), 0, WindowOpacity, LWA_ALPHA);

  ForceWindowFocus(hPopupWnd, hEditControl);

  MSG msg;
  int loop_counter = 0;
  UI_PreparePopupPump();
  while (!selection_complete) {
    if (GetMessage(&msg, NULL, 0, 0)) {
      if (UI_ShouldSkipPopupKey(&msg)) continue;
      loop_counter++;
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      break;
    }
  }

  if (WH_TO_HWND(hPopupWnd) != NULL && IsWindow(WH_TO_HWND(hPopupWnd))) {
    KillTimer(WH_TO_HWND(hPopupWnd), 999);
    DestroyWindow(WH_TO_HWND(hPopupWnd));
    hPopupWnd = WH_NULL;
  }

  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  DeleteObject(hCustomFont); 
  selection_complete = false;
  printf("[DEBUG-UI] Returning from Platform_SimplePromptUser. Buffer %s\n", output_buffer);
  return (strlen(output_buffer) > 0) ? output_buffer : NULL;
}

void Platform_DisplayMessage(const char* message, int centerX, int centerY, int maxWidth, int maxHeight, int timeout, ColorRef color) {
  if (WH_TO_HWND(hGlobalTrayWnd) != NULL && GetCurrentThreadId() != g_main_thread_id) {
    DisplayMessageParams params = { message, centerX, centerY, maxWidth, maxHeight, timeout, color };
    SendMessageA(WH_TO_HWND(hGlobalTrayWnd), WM_DISPLAY_MESSAGE, 0, (LPARAM)&params);
    return;
  }
  Platform_CreateDisplayMessageWindow(message, centerX, centerY, maxWidth, maxHeight, timeout, color);
}

void Platform_CreateDisplayMessageWindow(const char* message, int centerX, int centerY, int maxWidth, int maxHeight, int timeout, ColorRef color) {
  HINSTANCE hInstance = GetModuleHandle(NULL);

  DestroyWindow(WH_TO_HWND(hDisplayMessage));
  KillTimer(WH_TO_HWND(hDisplayMessage),999);

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
  g_timeout = timeout * 20;

  int topLeftX = centerX - (winWidth / 2);
  int topLeftY = centerY - (winHeight / 2);

  HWND hPopupWnd = CreateWindowExA(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE, 
      "DisplayMessageModalClass", NULL, WS_POPUP, 
      topLeftX, topLeftY, winWidth, winHeight, NULL, NULL, hInstance, pWndData
      );

  if (hPopupWnd == NULL) {
    printf("[DEBUG-UI] ERROR: CreateWindowExA failed. GetLastError: %lu\n", GetLastError());
    Platform_DestroyFont(pWndData->hFont);
    free(pWndData);
    return;
  }

  SetLayeredWindowAttributes(hPopupWnd, 0, pWndData->alpha, LWA_ALPHA);

  ShowWindow(hPopupWnd, SW_SHOWNOACTIVATE);
  UpdateWindow(hPopupWnd);

  SetTimer(hPopupWnd, 999, 50, NULL);

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

  hPopupWnd = WH_FROM_HWND(CreateWindowExA(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_COMPOSITED,
      "SettingsPopupClass", NULL, WS_POPUP | WS_CLIPCHILDREN, 
      510, 210, width, height, NULL, NULL, hInstance, NULL
      ));

  if (WH_TO_HWND(hPopupWnd) == NULL) {
    DWORD err = GetLastError();
    printf("CreateWindowExA failed: %lu\n", err);
    DeleteObject(hCustomFont);
    return;
  }

  hSettings = hPopupWnd;
  SetTimer(WH_TO_HWND(hPopupWnd), 999, 50, NULL);

  SettingsCreateChildControls(hPopupWnd, hCustomFont,&layout);

  SetLayeredWindowAttributes(WH_TO_HWND(hPopupWnd), 0, WindowOpacity, LWA_ALPHA);

  ForceWindowFocus(hPopupWnd, hEditControl);

  MSG msg;
  UI_PreparePopupPump();
  while (!selection_complete) {
    if (GetMessage(&msg, NULL, 0, 0)) {
      if (UI_ShouldSkipPopupKey(&msg)) continue;
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      break; 
    }
  }

  if (WH_TO_HWND(hPopupWnd) != NULL && IsWindow(WH_TO_HWND(hPopupWnd))) { 
    KillTimer(WH_TO_HWND(hPopupWnd), 999);
    DestroyWindow(WH_TO_HWND(hPopupWnd));
    hPopupWnd = WH_NULL;
  }
  hSettings = WH_NULL;

  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  DeleteObject(hCustomFont); 
}

HHOOK hKeyboardHook = NULL;

int Platform_InitializeTrayIcon() {
  HINSTANCE hInstance = GetModuleHandle(NULL);
  if (Tray) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "TrayAppClass";

    if (!RegisterClass(&wc)) return -1;


    hGlobalTrayWnd = WH_FROM_HWND(CreateWindow(
        wc.lpszClassName, "TrayApp", 
        WS_OVERLAPPEDWINDOW, 
        CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, 
        NULL, NULL, hInstance, NULL
        ));

    if (WH_TO_HWND(hGlobalTrayWnd) == NULL) return -1;
  }
  return 0;
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode < 0) {
    return CallNextHookEx(NULL, nCode, wParam, lParam);
  }

  if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
    KBDLLHOOKSTRUCT* pKeyInfo = (KBDLLHOOKSTRUCT*)lParam;

    bool alt_down   = (pKeyInfo->flags & LLKHF_ALTDOWN) || (GetKeyState(VK_MENU) & 0x8000);
    bool ctrl_down  = (GetKeyState(VK_CONTROL) & 0x8000);
    bool shift_down = (GetKeyState(VK_SHIFT) & 0x8000) || 
      (GetKeyState(VK_LSHIFT) & 0x8000) || 
      (GetKeyState(VK_RSHIFT) & 0x8000);

    double seconds = Platform_GetUniversalTimeSeconds();
    if (seconds >= 300) {
      Platform_ResetUniversalTimer();
      printf("SUPPOSED TO CLEAR FILE");
    }

    HWND hForeground = GetForegroundWindow();
    char fgTitle[256];
    if (hForeground != NULL) {
      GetWindowTextA(hForeground, fgTitle, sizeof(fgTitle));
    }

    int pressed_vk = (int)pKeyInfo->vkCode;
    int modifiers = HOTKEY_MOD_NONE;
    if (alt_down) modifiers |= HOTKEY_MOD_ALT;
    if (shift_down) modifiers |= HOTKEY_MOD_SHIFT;
    if (ctrl_down) modifiers |= HOTKEY_MOD_CTRL;

    WinTimer timer = TimerStart();
    if (ProcessHotkey(G_HOTKEY_BINDINGS, HOTKEY_BINDING_COUNT, 
          pressed_vk, modifiers, fgTitle)) {
      return 1;
    }
    TimerEnd(timer, "ProcessHotkey");

    if (shift_down && !IsInsertMode() && WH_TO_HWND(hPopupWnd) != NULL) {
      if (WH_TO_HWND(g_hEditPopup) == NULL) {
        if (pressed_vk == COMMAND_MODE_KEY) {
          SetMode(MODE_INSERT);
          char* output = Platform_EditTextPopup("", 960, 540, 1000, 300);
          if (output) {
            HandleCommandExecution(NULL, output);
          } else {
          }
          return 1;
        }
      }
    }
    if (WH_TO_HWND(g_hSearchPopup) == NULL && WH_TO_HWND(hPopupWnd) != NULL && IsNormalMode()) {
      if (pressed_vk == SEARCH_PROGRAM_KEY) {
        Platform_SearchPopup();
      }
    }

    WinTimer timer2 = TimerStart();
    if (ctrl_down) {
      if (pressed_vk == HELP_SCREEN_KEY) {
        if (g_help_callback != NULL) {
          g_help_callback();
          TimerEnd(timer2, "HelpScreen");
        }
      }
    }
    TimerEnd(timer2, "HelpScreenCheck");

    MacroAction matchedAction = MACRO_NONE;
    int matchedDelay = 0;
    const char* snippet_text = NULL;

    MotionAction vimAction = MOTION_NONE;
    const char* matchedSequence = NULL;
    int typedCount = 0;

    WinTimer timer3 = TimerStart();

    if (IsNormalMode()) {
      if (g_waiting_for_key) {
        g_waiting_for_key = false;
        if (g_key_callback) {
          g_key_callback(pKeyInfo->vkCode);
          TimerEnd(timer3, "KeyCallback");
        }
        return 1;
      }       
    }
    TimerEnd(timer3, "NormalModeCheck");

    WinTimer timer4 = TimerStart();
    if (CheckVimMotion(pressed_vk, modifiers, &vimAction, &matchedSequence, &typedCount)) {
      ExecuteMotion(vimAction, matchedSequence, typedCount);
      TimerEnd(timer4, "ExecuteMotion");
      return 1;
    }
    TimerEnd(timer4, "CheckVimMotion");

    WinTimer timer5 = TimerStart();
    if (CheckTextSnippetBindings(pressed_vk, modifiers)) {
      return 1;
    }
    TimerEnd(timer5, "CheckTextSnippetBindings");

    if (CheckMacroBindings(pressed_vk, modifiers)) {
      return 1;
    }

    WinTimer timer7 = TimerStart();
    if (CheckAppHotkeyBindings(pressed_vk, modifiers)) {
      return 1;
    }
    TimerEnd(timer7, "CheckAppHotkeyBindings");

    WinTimer timer8 = TimerStart();
    if (CheckMiscHotkeyBindings(pressed_vk, modifiers)) {
      return 1;
    }
    TimerEnd(timer8, "CheckMiscHotkeyBindings");

    WinTimer timer9 = TimerStart();
    if (CheckQuickLinkHotkeyBindings(pressed_vk, modifiers)) {
      return 1;
    }
    TimerEnd(timer9, "CheckQuickLinkHotkeyBindings");

    WinTimer timer10 = TimerStart();
    if (CheckTerminalCommandHotkeyBindings(pressed_vk, modifiers)) {
      return 1;
    }
    TimerEnd(timer10, "HandleTerminalCommand");

    WinTimer timer11 = TimerStart();
    if (HandleFocusSwitcher(pressed_vk, modifiers, fgTitle)) {
      return 1;
    }
    TimerEnd(timer11, "HandleFocusSwitcher");

    WinTimer timer12 = TimerStart();
    if (HandleMusicPlayer(pressed_vk, modifiers, fgTitle)) {
      return 1;
    }
    TimerEnd(timer12, "HandleMusicPlayer");

    WinTimer timer13 = TimerStart();
    if (HandleVideoViewer(pressed_vk, modifiers, fgTitle)) {
      return 1;
    }
    TimerEnd(timer13, "HandleVideoViewer");

    WinTimer timer14 = TimerStart();
    if (HandleBulkKillProcess(pressed_vk, modifiers, fgTitle)) {
      return 1;
    }
    TimerEnd(timer14, "HandleBulkKillProcess");

  }
  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void Platform_RunHotKeyMessageLoop() {
  HINSTANCE hInstance = GetModuleHandle(NULL);

  hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);
  if (!hKeyboardHook) {
    printf("[ERROR] Failed to install keyboard hook! Error: %lu\n", GetLastError());
    return;
  }

  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  if (hKeyboardHook) UnhookWindowsHookEx(hKeyboardHook);
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
    printf("[SYSTEM-BOOT] QuickSwitcherModalClass registered successfully with OS.\n");
  } else {
    printf("[SYSTEM-BOOT] CRITICAL WARNING: Failed to register window class! Error: %lu\n", GetLastError());
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
}

void Platform_ShutdownUI() {
  if (g_gdiplusToken != 0) {
    GdiplusShutdown(g_gdiplusToken);
  }
}
