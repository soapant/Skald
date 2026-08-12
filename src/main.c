#include "common.h"
#include "core/modal.h"
#include "helpers.h"
#include "core/common_ui.h"
#include "core/number_shortcut.h"
#include "core/feature_data.h"
#include "window/subclasses.h"
#include "bindings/bindings.h"
#include "bindings/hotkey_definitions.h"
#include "bindings/macro_bindings.h"
#include "bindings/vim_motions.h"
#include "features/focus_switcher.h"
#include "features/music_player.h"
#include "features/video_player.h"
#include "features/process_killer.h"
#include "features/app_launcher.h"
#include <gdiplus.h>

#define ID_TRAY_ICON 1001
#define WM_TRAYICON (WM_USER + 1)
#define ID_EXIT 2001

HWND hGlobalTrayWnd = NULL;
HWND hPopupWnd = NULL;
HWND hEditControl = NULL;
HWND hListBox = NULL;
HWND hDisplayMessage = NULL;
HWND hSettings = NULL;
HWND g_hEditPopup = NULL;
HWND g_hSearchPopup = NULL;
EditPopupData* g_EditData = NULL;
char output_buffer[256];
bool selection_complete = false;
bool setting_complete = false;
int region_step = 0;
long saved_x1 = 0;
long saved_y1 = 0;
long img_x2 = 0, img_y2 = 0;
static ULONG_PTR g_gdiplusToken = 0;
RefreshCallback g_refresh_callback = NULL;
EnterCallback g_enter_callback = NULL;
CtrlEnterCallback g_ctrl_enter_callback = NULL;
TabCallback g_tab_callback = NULL;
HelpCallback g_help_callback = NULL;
CopyCallback g_copy_callback = NULL;
RenderCallback g_render_callback = NULL;
AliasCallback g_alias_callback = NULL;
EditCallback g_edit_callback = NULL;
FilterCallback g_filter_callback = NULL;
HWND g_ActiveEditControl = NULL;
char* g_current_feature = NULL;
int g_timeout = 1;
bool g_waiting_for_key = false;
int g_key_pressed = 0;
WinTimer g_timer;
HANDLE g_key_event = NULL;
const char** g_initial_items = NULL;
int g_initial_count = 0;
int g_SelectedIndex = 0;
int g_TotalMatches = 0;
int g_last_match_count = 0;
const char** g_last_items = NULL;
int g_last_item_count = 0;
char g_last_filter[256] = {0};
char g_populated_cache[MAX_ITEMS][512];
int g_matching_indices[MAX_MATCHES];
int g_populated_count = 0;
int g_view_offset = 0;
const char* g_title = NULL;
int img_region_step = 0;
long img_x1 = 0, img_y1 = 0;
HFONT g_hNumFont = NULL;
HFONT g_hMainFont = NULL;
HashMap* feature_history = NULL;
HashMap* g_item_index_map = NULL;

void WaitForKeypressAsync(KeyCallback callback) {
  g_waiting_for_key = true;
  g_key_callback = callback;
}

char* PromptUser(const char* window_list[], const char* window_title, int count, int height, int width, int item_height,
    RefreshCallback refresh_callback, 
    EnterCallback enter_callback,
    CtrlEnterCallback ctrl_enter_callback,
    TabCallback tab_callback,
    CopyCallback copy_callback,
    RenderCallback render_callback,
    EditCallback edit_callback,
    FilterCallback filter_callback,
    HelpCallback help_callback) {
  TimerEnd(g_timer,window_title + 5);
  HINSTANCE hInstance = GetModuleHandle(NULL);
  selection_complete = false;
  output_buffer[0] = '\0';
  g_initial_items = window_list;
  g_initial_count = count;
  WinTimer timer = TimerStart();

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
  g_current_feature = (char*)window_title + 5;
  SetCurrentFeatureName(g_current_feature);
  BuildAliasDisplayItems(window_list, count);

  if (ModePersistsThroughFeatures) {
    SetMode(g_last_mode);
  } else {
    SetMode(MODE_INSERT);
  }

  WindowLayout layout;
  CalculateWindowMetrics(count, &layout, height, width, item_height);

  HFONT hCustomFont = CreateCustomNerdFont(FontSize, UsedFont);

  hPopupWnd = CreateWindowExA(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_COMPOSITED,
      "QuickSwitcherModalClass", NULL, WS_POPUP | WS_CLIPCHILDREN, 
      layout.x, layout.y, layout.w, layout.h, NULL, NULL, hInstance, NULL
      );

  if (hPopupWnd == NULL) {
    DeleteObject(hCustomFont);
    return NULL;
  }

  TimerEnd(timer,"Window Init");

  WinTimer timer1 = TimerStart();


  CreateChildControls(hPopupWnd, hCustomFont, window_list, count, &layout);

  TimerEnd(timer1,"CreateChildControl");

  WinTimer timer2 = TimerStart();

  SetLayeredWindowAttributes(hPopupWnd, 0, 245, LWA_ALPHA);

  TimerEnd(timer2,"WindowBs");

  WinTimer timer3 = TimerStart();

  if (IsNormalMode() && RememberLastIndex && last_index >= 0) {
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

  if (hPopupWnd != NULL && IsWindow(hPopupWnd)) {
    KillTimer(hPopupWnd, 999);
    DestroyWindow(hPopupWnd);
    hPopupWnd = NULL;
  }
  ResetNumberShortcutState();

  printf("[DEBUG-UI] Flushing trailing queue entries...\n");
  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  struct feature_data* data = GetFeatureData(g_current_feature);
  if (data) {
    data->name = _strdup(g_current_feature);
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

  printf("[DEBUG-UI] Returning from PromptUser. Buffer Length: %zu\n", strlen(output_buffer));
  return (strlen(output_buffer) > 0) ? output_buffer : NULL;
}

char* SimplePromptUser(const char* prompt, const char* window_title,int height, int width, int item_height) {
  HINSTANCE hInstance = GetModuleHandle(NULL);
  selection_complete = false;
  output_buffer[0] = '\0';
  g_title = window_title;
  g_current_feature = (char*)window_title + 4;
  SetCurrentFeatureName(g_current_feature);

  SetMode(MODE_INSERT);

  WindowLayout layout;
  CalculateWindowMetrics(1, &layout, height, width, item_height);

  HFONT hCustomFont = CreateCustomNerdFont(FontSize, UsedFont);

  hPopupWnd = CreateWindowExA(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, 
      "QuickSwitcherModalClass", NULL, WS_POPUP | WS_CLIPCHILDREN, 
      layout.x, layout.y, layout.w, layout.h, NULL, NULL, hInstance, NULL
      );


  SetTimer(hPopupWnd, 999, 50, NULL);

  if (hPopupWnd == NULL) {
    DWORD err = GetLastError();
    if (err == 1407) {
    }
    DeleteObject(hCustomFont);
    return NULL;
  }

  SimpleCreateChildControls(hPopupWnd, hCustomFont, prompt, 1, &layout);

  SetLayeredWindowAttributes(hPopupWnd, 0, 245, LWA_ALPHA);

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

  if (hPopupWnd != NULL && IsWindow(hPopupWnd)) {
    KillTimer(hPopupWnd, 999);
    DestroyWindow(hPopupWnd);
    hPopupWnd = NULL;
  }

  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  DeleteObject(hCustomFont); 
  selection_complete = false;
  printf("[DEBUG-UI] Returning from SimplePromptUser. Buffer %s\n", output_buffer);
  return (strlen(output_buffer) > 0) ? output_buffer : NULL;
}

void DisplayMessage(const char* message, int centerX, int centerY, int maxWidth, int maxHeight, int timeout) {
  HINSTANCE hInstance = GetModuleHandle(NULL);

  DestroyWindow(hDisplayMessage);
  KillTimer(hDisplayMessage,999);


  HDC hdc = GetDC(NULL);
  HFONT hFont = CreateCustomNerdFont(FontSize, UsedFont);
  HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

  RECT textRect = {0, 0, maxWidth - 30, maxHeight - 30};
  DrawTextA(hdc, message, -1, &textRect, DT_CENTER | DT_WORDBREAK | DT_CALCRECT);

  SelectObject(hdc, hOldFont);
  DeleteObject(hFont);
  ReleaseDC(NULL, hdc);

  int padding = 30;
  int winWidth = textRect.right + padding;
  int winHeight = textRect.bottom + padding;

  if (winWidth > maxWidth) winWidth = maxWidth;
  if (winHeight > maxHeight) winHeight = maxHeight;

  FadeWindowData* pWndData = (FadeWindowData*)malloc(sizeof(FadeWindowData));
  if (!pWndData) return;

  pWndData->message = _strdup(message);
  pWndData->hFont = CreateCustomNerdFont(FontSize, UsedFont);
  pWndData->alpha = 245;
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
    DeleteObject(pWndData->hFont);
    free(pWndData);
    return;
  }

  SetLayeredWindowAttributes(hPopupWnd, 0, pWndData->alpha, LWA_ALPHA);

  ShowWindow(hPopupWnd, SW_SHOWNOACTIVATE);
  UpdateWindow(hPopupWnd);

  SetTimer(hPopupWnd, 999, 50, NULL);

  return; 
}

void SettingsPopUp() {
  HINSTANCE hInstance = GetModuleHandle(NULL);
  int height = 700;
  int width = 900;
  int item_height = 32;

  setting_complete = false;

  SetMode(MODE_INSERT);

  WindowLayout layout;
  CalculateSettingsWindowMetrics(1, &layout, height, width, item_height);

  HFONT hCustomFont = CreateCustomNerdFont(FontSize, UsedFont);

  hPopupWnd = CreateWindowExA(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_COMPOSITED,
      "SettingsPopupClass", NULL, WS_POPUP | WS_CLIPCHILDREN, 
      510, 210, width, height, NULL, NULL, hInstance, NULL
      );

  if (hPopupWnd == NULL) {
    DWORD err = GetLastError();
    printf("CreateWindowExA failed: %lu\n", err);
    DeleteObject(hCustomFont);
    return;
  }

  hSettings = hPopupWnd;
  SetTimer(hPopupWnd, 999, 50, NULL);

  SettingsCreateChildControls(hPopupWnd, hCustomFont,&layout);

  SetLayeredWindowAttributes(hPopupWnd, 0, 245, LWA_ALPHA);

  ForceWindowFocus(hPopupWnd, hEditControl);

  MSG msg;
  UI_PreparePopupPump();
  while (!setting_complete) {
    if (GetMessage(&msg, NULL, 0, 0)) {
      if (UI_ShouldSkipPopupKey(&msg)) continue;
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      break; 
    }
  }

  if (hPopupWnd != NULL && IsWindow(hPopupWnd)) { 
    KillTimer(hPopupWnd, 999);
    DestroyWindow(hPopupWnd);
    hPopupWnd = NULL;
  }
  hSettings = NULL;

  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  DeleteObject(hCustomFont); 
}

char* EditTextPopup(const char* initial_text, int centerX, int centerY, int maxWidth, int maxHeight) {
  HINSTANCE hInstance = GetModuleHandle(NULL);
  int index = g_SelectedIndex;

  if (!initial_text) return NULL;

  if (g_hEditPopup && IsWindow(g_hEditPopup)) {
    DestroyWindow(g_hEditPopup);
    g_hEditPopup = NULL;
  }

  HDC hdc = GetDC(NULL);
  HFONT hFont = CreateCustomNerdFont(FontSize, UsedFont);
  HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

  RECT textRect = {0, 0, maxWidth - 40, maxHeight - 60};
  DrawTextA(hdc, initial_text, -1, &textRect, DT_LEFT | DT_WORDBREAK | DT_CALCRECT);

  SelectObject(hdc, hOldFont);
  DeleteObject(hFont);
  ReleaseDC(NULL, hdc);

  int padding = 30;
  int editHeight = 40;
  int winWidth = textRect.right + padding + EditPopUpWidthMin;
  int winHeight = textRect.bottom + padding + editHeight + 20;

  if (winWidth > maxWidth) winWidth = maxWidth;
  if (winHeight > maxHeight) winHeight = maxHeight;
  if (winWidth < 200) winWidth = 200;
  if (winHeight < 100) winHeight = 100;

  EditPopupData* pData = (EditPopupData*)malloc(sizeof(EditPopupData));
  if (!pData) return NULL;

  pData->initial_text = _strdup(initial_text);
  pData->result = NULL;
  pData->hFont = CreateCustomNerdFont(FontSize, UsedFont);
  pData->alpha = 255;
  pData->hEdit = NULL;

  int topLeftX = centerX - (winWidth / 2);
  int topLeftY = centerY - (winHeight / 2);

  HWND hPopupWnd = CreateWindowExA(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
      "EditPopupClass", NULL, WS_POPUP,
      topLeftX, topLeftY, winWidth, winHeight,
      NULL, NULL, hInstance, pData
      );

  if (hPopupWnd == NULL) {
    printf("[ERROR] EditPopup creation failed\n");
    DeleteObject(pData->hFont);
    free(pData);
    return NULL;
  }

  g_hEditPopup = hPopupWnd;
  g_EditData = pData;

  SetLayeredWindowAttributes(hPopupWnd, 0, pData->alpha, LWA_ALPHA);

  ShowWindow(hPopupWnd, SW_SHOWNOACTIVATE);
  UpdateWindow(hPopupWnd);

  UpdateTextCaret(g_ActiveEditControl);

  SetTimer(hPopupWnd, 999, 50, NULL);

  MSG msg;
  UI_PreparePopupPump();
  while (IsWindow(hPopupWnd) && g_EditData && g_EditData->result == NULL) {
    if (GetMessage(&msg, NULL, 0, 0)) {
      if (UI_ShouldSkipPopupKey(&msg)) continue;
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      break;
    }
  }

  char* result = NULL;
  if (g_EditData && g_EditData->result) {
    result = _strdup(g_EditData->result);
  }

  if (hPopupWnd && IsWindow(hPopupWnd)) {
    DestroyWindow(hPopupWnd);
  }
  g_hEditPopup = NULL;
  g_EditData = NULL;
  g_SelectedIndex = index;


  return result;
}

char* SearchPopup() {
  HINSTANCE hInstance = GetModuleHandle(NULL);
  int index = g_SelectedIndex;
  int winWidth = 280;
  int winHeight = 80;

  if (g_hSearchPopup && IsWindow(g_hSearchPopup)) {
    DestroyWindow(g_hSearchPopup);
    g_hSearchPopup = NULL;
  }

  HDC hdc = GetDC(NULL);
  HFONT hFont = CreateCustomNerdFont(FontSize, UsedFont);
  HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

  RECT textRect = {0, 0, winWidth - 40, winHeight - 60};
  DrawTextA(hdc, "", -1, &textRect, DT_LEFT | DT_WORDBREAK | DT_CALCRECT);

  SelectObject(hdc, hOldFont);
  DeleteObject(hFont);
  ReleaseDC(NULL, hdc);

  int topLeftX = 1270 - (winWidth / 2);
  int topLeftY = 920 - (winHeight / 2);

  HWND hPopupWnd = CreateWindowExA(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
      "SearchPopupClass", NULL, WS_POPUP,
      topLeftX, topLeftY, winWidth, winHeight,
      NULL, NULL, hInstance, NULL
      );

  if (hPopupWnd == NULL) {
    printf("[ERROR] SearchPopup creation failed\n");
    DeleteObject(hFont);
    return NULL;
  }

  SetLayeredWindowAttributes(hPopupWnd, 0, 255, LWA_ALPHA);

  ShowWindow(hPopupWnd,SW_SHOWNOACTIVATE);
  UpdateWindow(hPopupWnd);

  UpdateTextCaret(g_ActiveEditControl);

  SetTimer(hPopupWnd, 999, 50, NULL);

  MSG msg;
  UI_PreparePopupPump();
  while (IsWindow(hPopupWnd)) {
    if (GetMessage(&msg, NULL, 0, 0)) {
      if (UI_ShouldSkipPopupKey(&msg)) continue;
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      break;
    }
  }

  char* result = NULL;

  if (hPopupWnd && IsWindow(hPopupWnd)) {
    DestroyWindow(hPopupWnd);
  }

  g_hSearchPopup = NULL;
  g_SelectedIndex = index;


  return result;
}

HHOOK hKeyboardHook = NULL;

int InitializeTrayIcon() {
  HINSTANCE hInstance = GetModuleHandle(NULL);
  if (Tray) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "TrayAppClass";

    if (!RegisterClass(&wc)) return -1;


    hGlobalTrayWnd = CreateWindow(
        wc.lpszClassName, "TrayApp", 
        WS_OVERLAPPEDWINDOW, 
        CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, 
        NULL, NULL, hInstance, NULL
        );

    if (!hGlobalTrayWnd) return -1;
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

    double seconds = GetUniversalTimeSeconds();
    if (seconds >= 300) {
      ResetUniversalTimer();
      printf("SUPPOSED TO CLEAR FILE");
    }

    HWND hForeground = GetForegroundWindow();
    char fgTitle[256];
    if (hForeground != NULL) {
      UI_GetCurrentText(hForeground, fgTitle);
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

    if (shift_down && !IsInsertMode() && hPopupWnd != NULL) {
      if (!g_hEditPopup) {
        if (pressed_vk == COMMAND_MODE_KEY) {
          SetMode(MODE_INSERT);
          char* output = EditTextPopup("", 960, 540, 1000, 300);
          if (output) {
            HandleCommandExecution(output);
          } else {
          }
          return 1;
        }
      }
    }
    if (!g_hSearchPopup && hPopupWnd && IsNormalMode()) {
      if (pressed_vk == SEARCH_PROGRAM_KEY) {
        SearchPopup();
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
    if (HandleFocusSwitcher(pressed_vk, modifiers, fgTitle)) {
      return 1;
    }
    TimerEnd(timer10, "HandleFocusSwitcher");

    WinTimer timer11 = TimerStart();
    if (HandleMusicPlayer(pressed_vk, modifiers, fgTitle)) {
      return 1;
    }
    TimerEnd(timer11, "HandleMusicPlayer");

    WinTimer timer12 = TimerStart();
    if (HandleVideoPlayer(pressed_vk, modifiers, fgTitle)) {
      return 1;
    }
    TimerEnd(timer12, "HandleVideoPlayer");

    WinTimer timer13 = TimerStart();
    if (HandleBulkKillProcess(pressed_vk, modifiers, fgTitle)) {
      return 1;
    }
    TimerEnd(timer13, "HandleBulkKillProcess");
  }
  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void RunHotKeyMessageLoop() {
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

int main() {
  {
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, buf, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
      char* slash = strrchr(buf, '\\');
      if (slash) *slash = '\0';
      for (int depth = 0; depth < 4; ++depth) {
        char probe[MAX_PATH];
        snprintf(probe, sizeof(probe), "%s\\data", buf);
        DWORD attr = GetFileAttributesA(probe);
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
          SetCurrentDirectoryA(buf);
          break;
        }
        slash = strrchr(buf, '\\');
        if (!slash) break;
        *slash = '\0';
      }
    }
  }

  StartUniversalTimer();
  InitializeTrayIcon();
  initialize_app_cache();
  feature_history = HashMap_Create(32);
  InitFeatureDataSystem();

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


  for (int i = 0; i < FILE_ENTRY_COUNT; i++) {
    SanitizeFile(G_FILE_ENTRIES[i].path);
  }

  for (int i = 0; i < FILE_ENTRY_COUNT; i++) {
    G_FILE_ENTRIES[i].mf = (MappedFile*)malloc(sizeof(MappedFile));
    const char* path = G_FILE_ENTRIES[i].path;


    if (!FileInit(path, G_FILE_ENTRIES[i].mf, 0)) {
      printf("[SYSTEM] Failed to open %s. Creating new.\n", G_FILE_ENTRIES[i].name);
      HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
      if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
        FileInit(path, G_FILE_ENTRIES[i].mf, 0); 
      }
    }
  }

  printf("[SYSTEM] Low-Level Hotkey Services Running. GDI+ Online.\n");
  RunHotKeyMessageLoop();

  if (g_gdiplusToken != 0) {
    GdiplusShutdown(g_gdiplusToken);
  }

  for (int i = 0; i < FILE_ENTRY_COUNT; i++) {
    if (G_FILE_ENTRIES[i].mf != NULL) {
      FileClose(G_FILE_ENTRIES[i].mf);
      free(G_FILE_ENTRIES[i].mf);
      G_FILE_ENTRIES[i].mf = NULL;
    }
  }
  if (g_hNumFont) {
    DeleteObject(g_hNumFont);
    g_hNumFont = NULL;
  }
  if (g_hMainFont) {
    DeleteObject(g_hMainFont);
    g_hMainFont = NULL;
  }
  CleanupFeatureDataSystem();

  return 0;
}
