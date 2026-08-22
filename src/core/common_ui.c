#include "common.h"
#include "config.h"
#include "helpers.h"
#include "core/common_ui.h"
#include "platform/platform.h"
#if defined(_WIN32)
#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#endif
#if !defined(_WIN32)
#include <strings.h>
#include <ctype.h>
#include <string.h>
// Case-insensitive substring search for Linux (like StrStrIA on Windows)
static const char* Linux_StrStrIA(const char* haystack, const char* needle) {
    if (!haystack || !needle) return NULL;
    if (!*needle) return haystack;
    size_t nlen = strlen(needle);
    for (const char* p = haystack; *p; ++p) {
        if (strncasecmp(p, needle, nlen) == 0) return p;
    }
    return NULL;
}
#endif

bool g_add_alias_to_string = true;
char g_alias_display[MAX_ITEMS][512] = {0};
int g_alias_display_count = 0;
bool g_alias_display_active = false;

void BuildAliasDisplayItems(const char** items, int count) {
  g_alias_display_active = false;
  g_alias_display_count = 0;
  if (!items || count <= 0) return;
  if (count > MAX_ITEMS) count = MAX_ITEMS;
  for (int i = 0; i < count; i++) {
    g_alias_display[i][0] = '\0';
    if (!items[i]) continue;
    size_t len = strlen(items[i]);
    if (len >= 512) len = 511;
    memcpy(g_alias_display[i], items[i], len);
    g_alias_display[i][len] = '\0';
    while (len > 0 && (g_alias_display[i][len - 1] == '\r' || g_alias_display[i][len - 1] == '\n')) {
      g_alias_display[i][--len] = '\0';
    }
    const char* alias = get_alias_for_item(g_alias_display[i]);
    if (alias) {
      snprintf(g_alias_display[i] + len, (size_t)(512 - len), " [%s]", alias);
    }
    g_alias_display_count = i + 1;
  }
  if (g_alias_display_count > 0) g_alias_display_active = true;
}

void PopulateListBox(ListBoxInfo* lb, WindowHandle hListBox, const char* filter, 
    const char** items, int item_count,
    bool (*filter_match)(const char* item, const char* filter)) {

  if (!lb) return;
  WinTimer timer_alias = TimerStart();
#if defined(_WIN32)
  if (WH_TO_HWND(hListBox) == NULL) return;
#endif
  if (g_seq_state.waiting) return;
  if (!filter) filter = "";
  BuildAliasDisplayItems(items, item_count);
  TimerEnd(timer_alias,"PopulateListBox - Aliases");


  WinTimer timer_total = TimerStart();
  int old_count = lb->total_matches;

  WinTimer timer_current_sel = TimerStart();
  int current_sel = lb->selected_index; 
  char current_text[256] = {0};
  if (current_sel >= 0 && current_sel < lb->populated_count) {
    strncpy(current_text, lb->populated_cache[current_sel], sizeof(current_text) - 1);
    current_text[sizeof(current_text) - 1] = '\0';
  }
  TimerEnd(timer_current_sel, "  PopulateListBox - Get current selection");

  WinTimer timer_clean_filter = TimerStart();
  int total_matches = 0;
  char clean_filter[256] = {0};
  if (filter) {
    strncpy(clean_filter, filter, sizeof(clean_filter) - 1);
    clean_filter[sizeof(clean_filter) - 1] = '\0';
  }
  TimerEnd(timer_clean_filter, "  PopulateListBox - Clean filter");

  WinTimer timer_inc = TimerStart();
  int use_previous = 0;
  int prev_count = 0;
  if (strlen(clean_filter) > 0 && 
      lb->last_items == items && 
      lb->last_item_count == item_count &&
      lb->last_filter_valid &&
      strlen(clean_filter) > strlen(lb->last_filter) &&
#if defined(_WIN32)
      StrStrIA(clean_filter, lb->last_filter) == clean_filter) {
#else
      strncasecmp(clean_filter, lb->last_filter, strlen(lb->last_filter)) == 0) {
#endif

    use_previous = 1;
    prev_count = lb->last_filter_full_count;
  }
  TimerEnd(timer_inc, "  PopulateListBox - Incremental check");

  WinTimer timer_filter = TimerStart();
  if (use_previous && prev_count == 0) {
    total_matches = 0;
    printf("[SKIP] No previous matches for '%s', skipping filter for '%s'\n",
        lb->last_filter, clean_filter);
  } else if (use_previous && prev_count > 0) {
    int* prev_matches = lb->prev_matches;
    printf("[OPTIMIZE] Incremental: '%s' -> '%s'\n", lb->last_filter, clean_filter);

    for (int i = 0; i < prev_count && total_matches < MAX_MATCHES; i++) {
      int idx = prev_matches[i];
      if (!items[idx]) continue;

      const char* search_text = items[idx];
      if (g_alias_display_active && idx >= 0 && idx < g_alias_display_count && g_alias_display[idx][0] != '\0') {
        search_text = g_alias_display[idx];
      }
      bool match = false;
      if (filter_match) {
        match = filter_match(search_text, clean_filter);
      } else {
#if defined(_WIN32)
        match = (StrStrIA(search_text, clean_filter) != NULL);
#else
        match = (Linux_StrStrIA(search_text, clean_filter) != NULL);
#endif
      }

      if (match) {
        lb->matching_indices[total_matches] = idx;
        total_matches++;
      }
    }
  } else {
    bool empty_filter = (clean_filter[0] == '\0');
    for (int i = 0; i < item_count && total_matches < MAX_MATCHES; i++) {
      if (!items[i]) continue;

      if (empty_filter) {
        lb->matching_indices[total_matches] = i;
        total_matches++;
        continue;
      }

      const char* search_text = items[i];
      if (g_alias_display_active && i < g_alias_display_count && g_alias_display[i][0] != '\0') {
        search_text = g_alias_display[i];
      }
#if defined(_WIN32)
      bool match = filter_match ? filter_match(search_text, clean_filter) : (StrStrIA(search_text, clean_filter) != NULL);
#else
      bool match = filter_match ? filter_match(search_text, clean_filter) : (Linux_StrStrIA(search_text, clean_filter) != NULL);
#endif

      if (match) {
        lb->matching_indices[total_matches] = i;
        total_matches++;
      }
    }
  }
  TimerEnd(timer_filter, "  PopulateListBox - Filtering");

  WinTimer timer_history = TimerStart();
  if (total_matches > 0 && strlen(clean_filter) > 0) {
    memcpy(lb->prev_matches, lb->matching_indices, total_matches * sizeof(int));
    lb->last_filter_full_count = total_matches;
    printf("[HISTORY] Saved '%s' (%d matches)\n", clean_filter, total_matches);
  } else {
    lb->last_filter_full_count = 0;
  }
  TimerEnd(timer_history, "  PopulateListBox - History save");

  WinTimer timer_matches = TimerStart();
  if (total_matches == 0 && item_count > 0 && strlen(clean_filter) == 0) {
    int force_count = (item_count < 30) ? item_count : 30;
    for (int i = 0; i < force_count; i++) {
      lb->matching_indices[total_matches] = i;
      total_matches++;
    }
  }
  TimerEnd(timer_matches," PopulateListBox - Matches");

  WinTimer timer_pool = TimerStart();
  int pool_max = MAX_VISIBLE_ITEMS;
  if (total_matches > pool_max) {
    total_matches = pool_max;
  }
  TimerEnd(timer_pool, "  PopulateListBox - Pool adjust");

  WinTimer timer_cache = TimerStart();
  lb->populated_count = 0;

  for (int i = 0; i < total_matches; i++) {
    int raw_data_index = lb->matching_indices[i];
    if (!items[raw_data_index]) continue;

    strncpy(lb->populated_cache[lb->populated_count], items[raw_data_index], 511);
    lb->populated_cache[lb->populated_count][511] = '\0';
    lb->populated_cache[lb->populated_count][strcspn(lb->populated_cache[lb->populated_count], "\r\n")] = '\0';
    lb->populated_count++;
  }
  TimerEnd(timer_cache, "  PopulateListBox - Cache build");

  UI_LB_SetCount(lb, hListBox, total_matches);

  WinTimer timer_ui = TimerStart();
  if (!g_seq_state.waiting) {
    if (IsNormalMode()) {
      int keep;
      if (RememberLastIndex) {
        keep = lb->selected_index;
        if (keep < 0 && last_index >= 0) {
          keep = last_index;
        }
        if (total_matches == 0) keep = -1;
        else if (keep < 0) keep = 0;
        else if (keep >= total_matches) keep = total_matches - 1;
        UI_NavigateTo(lb, hListBox, keep);
      } else {
        keep = (total_matches > 0) ? 0 : -1;
        UI_SetSelection(lb, hListBox, keep);
      }
    } else if (total_matches != 0 && strlen(filter) > 0) {
      UI_SetSelection(lb, hListBox, 0);
      draw_background = false;
    } else {
      UI_SetSelection(lb, hListBox, -1);
      draw_background = true;
    }
    if (total_matches == 0) {
      if (WH_TO_HWND(hDisplayMessage) != NULL) {
#if defined(_WIN32)
        KillTimer(WH_TO_HWND(hDisplayMessage), 999);
        DestroyWindow(WH_TO_HWND(hDisplayMessage));
#endif
        hDisplayMessage = WH_NULL;
      }
    }
  }

  if (total_matches != old_count || clean_filter[0] != '\0') {
#if defined(_WIN32)
    InvalidateRect(WH_TO_HWND(hListBox), NULL, FALSE);
#endif
  }
  TimerEnd(timer_ui, "  PopulateListBox - UI update");

  WinTimer timer_cleanup = TimerStart();
  if (strlen(clean_filter) > 0) {
    strncpy(lb->last_filter, clean_filter, sizeof(lb->last_filter) - 1);
    lb->last_filter[sizeof(lb->last_filter) - 1] = '\0';
    lb->last_filter_valid = true;
  } else {
    lb->last_filter_valid = false;
  }
  lb->last_items = items;
  lb->last_item_count = item_count;
  lb->total_matches = total_matches;
  if (total_matches != old_count) {
    draw_background = true;
  }
  TimerEnd(timer_cleanup, "  PopulateListBox - Cleanup");

  TimerEnd(timer_total, "PopulateListBox TOTAL");
}

ColorRef GetRainbowColor(float t, float s, float v) {
  if (RainbowText) {
    float hue = t * 240.0f;
    float h = hue / 60.0f;
    int i = ((int)h) % 6;
    float f = h - (int)h;
    float p = v * (1 - s);
    float q = v * (1 - s * f);
    float t_ = v * (1 - s * (1 - f));
    float r, g, b;
    switch (i) {
      case 0: r = v; g = t_; b = p; break;
      case 1: r = q; g = v; b = p; break;
      case 2: r = p; g = v; b = t_; break;
      case 3: r = p; g = q; b = v; break;
      case 4: r = t_; g = p; b = v; break;
      case 5: r = v; g = p; b = q; break;
    }
    return RGB((uint8_t)(r * 255), (uint8_t)(g * 255), (uint8_t)(b * 255));
  } else {
    return COLOR_TEXT;
  }
}

#if defined(_WIN32)
HFONT CreateCustomNerdFont(int height, const char* face_name) {
  return FH_TO_HFONT(Platform_CreateFont(height, face_name));
}
#endif

void UpdateTextCaret(WindowHandle hwndEditControl)
{
#if defined(_WIN32)
  int caretWidth = 1;   
  int caretHeight = FontSize; 

  if (IsInsertMode()) {
    caretWidth = 2;   
  } 
  else if (IsNormalMode()) {
    caretWidth = 12;  
  } 
  else {
    caretWidth = 12;  
  }

  static int lastWidth = -1;
  static HWND lastHwnd = NULL;

  if (caretWidth != lastWidth || WH_TO_HWND(hwndEditControl) != lastHwnd) {
    DestroyCaret();
    CreateCaret(WH_TO_HWND(hwndEditControl), NULL, caretWidth, caretHeight);
    ShowCaret(WH_TO_HWND(hwndEditControl));

    SetCaretBlinkTime(INFINITE);

    lastWidth = caretWidth;
    lastHwnd = WH_TO_HWND(hwndEditControl);
  }
#else
  (void)hwndEditControl;
#endif
}

#if defined(_WIN32)
void CalculateWindowMetrics(int count, WindowLayout* layout,int height , int width,int item_height) {
  POINT ptPrimary = { 0, 0 }; 
  HMONITOR hMonitor = MonitorFromPoint(ptPrimary, MONITOR_DEFAULTTOPRIMARY);

  MONITORINFO mi = {0};
  mi.cbSize = sizeof(MONITORINFO);
  GetMonitorInfoA(hMonitor, &mi);

  int monitorWidth = mi.rcMonitor.right - mi.rcMonitor.left;
  int monitorHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;

  layout->w = 900;          
  layout->item_height = item_height; 

  layout->h = 95 + (count > 0 ? (count * layout->item_height) : 0); 

  if (layout->h < height) layout->h = height;
  if (layout->h > width) layout->h = width; 

  layout->x = mi.rcMonitor.left + (monitorWidth - layout->w) / 2;
  layout->y = mi.rcMonitor.top + (monitorHeight - layout->h) / 2;
}
void CalculateSettingsWindowMetrics(int count, WindowLayout* layout,int height , int width,int item_height) {
  POINT ptPrimary = { 0, 0 }; 
  HMONITOR hMonitor = MonitorFromPoint(ptPrimary, MONITOR_DEFAULTTOPRIMARY);

  MONITORINFO mi = {0};
  mi.cbSize = sizeof(MONITORINFO);
  GetMonitorInfoA(hMonitor, &mi);

  int monitorWidth = mi.rcMonitor.right - mi.rcMonitor.left;
  int monitorHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;

  layout->w = 900;          
  layout->item_height = item_height; 

  layout->h = 95 + (count > 0 ? (count * layout->item_height) : 0); 

  if (layout->h < height) layout->h = height;
  if (layout->h > width) layout->h = width; 

  layout->x = mi.rcMonitor.left + (monitorWidth - layout->w) / 2;
  layout->y = mi.rcMonitor.top + (monitorHeight - layout->h) / 2;
}

void SimpleCreateChildControls(WindowHandle hwndParent, HFONT hFont, const char* string, int count, const WindowLayout* layout) {
  HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrA(WH_TO_HWND(hwndParent), GWLP_HINSTANCE);

  hEditControl = WH_FROM_HWND(CreateWindowExA(
      0, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
      20, 50, layout->w - 40, 40, WH_TO_HWND(hwndParent), (HMENU)IDC_INPUT_BOX, hInstance, NULL
      ));

  if (WH_TO_HWND(hEditControl) == NULL) return;

  g_ActiveEditControl = hEditControl;
  UI_Edit_SetFont(hEditControl, hFont);
  SetWindowSubclass(WH_TO_HWND(hEditControl), SimpleEditSubclassProc, 0, 0);

  if (count > 0) {
    hListBox = WH_FROM_HWND(CreateWindowExA(
        0, "LISTBOX", "", 
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOSEL, 
        20, 105, layout->w - 40, layout->h - 115, WH_TO_HWND(hwndParent), (HMENU)IDC_LIST_BOX, hInstance, NULL
        ));

    if (WH_TO_HWND(hListBox) == NULL) return;

    SetWindowSubclass(WH_TO_HWND(hListBox), SimpleListBoxSubclassProc, 0, 0);
    UI_LB_SetFont(hListBox, hFont);
    UI_LB_SetItemHeight(hListBox, layout->item_height);

    int max_width = layout->w - 40;
    HDC hdc = GetDC(WH_TO_HWND(hListBox));
    if (!hdc) return;
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    SendMessage(WH_TO_HWND(hListBox), LB_ADDSTRING, 0, (LPARAM)string);
    SendMessage(WH_TO_HWND(hListBox), LB_SETCURSEL, (WPARAM)-1, 0);
    UpdateWindow(WH_TO_HWND(hListBox)); 

    SelectObject(hdc, hOldFont);
    ReleaseDC(WH_TO_HWND(hListBox), hdc);
  }
}

void ForceWindowFocus(WindowHandle hwndTarget, WindowHandle hwndInput) {
  ShowWindow(WH_TO_HWND(hwndTarget), SW_SHOW);

  SetWindowPos(WH_TO_HWND(hwndTarget), HWND_TOPMOST, 0, 0, 0, 0,
      SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW | SWP_ASYNCWINDOWPOS);

  HWND hForeground = GetForegroundWindow();
  DWORD foregroundThreadID = GetWindowThreadProcessId(hForeground, NULL);
  DWORD targetThreadID = GetWindowThreadProcessId(WH_TO_HWND(hwndTarget), NULL);
  DWORD currentThreadID = GetCurrentThreadId();

  if (foregroundThreadID != currentThreadID)
    AttachThreadInput(currentThreadID, foregroundThreadID, TRUE);
  if (targetThreadID != currentThreadID)
    AttachThreadInput(currentThreadID, targetThreadID, TRUE);

  SetForegroundWindow(WH_TO_HWND(hwndTarget));
  SetActiveWindow(WH_TO_HWND(hwndTarget));
  BringWindowToTop(WH_TO_HWND(hwndTarget));

  if (WH_TO_HWND(hwndInput) != NULL) {
    SetFocus(WH_TO_HWND(hwndInput));
    UI_Edit_UpdateUIState(hwndInput, UIS_CLEAR, UISF_HIDEFOCUS);
    UI_Edit_SetSel(hwndInput, 0, 0);
  }

  if (foregroundThreadID != currentThreadID)
    AttachThreadInput(currentThreadID, foregroundThreadID, FALSE);
  if (targetThreadID != currentThreadID)
    AttachThreadInput(currentThreadID, targetThreadID, FALSE);
}

void CreateChildControls(WindowHandle hwndParent, HFONT hFont, const char* list[], int count, const WindowLayout* layout, ListBoxInfo* lb) {
  
  HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrA(WH_TO_HWND(hwndParent), GWLP_HINSTANCE);

  hEditControl = WH_FROM_HWND(CreateWindowExA(
      0, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
      20, 50, layout->w - 40, 40, WH_TO_HWND(hwndParent), (HMENU)IDC_INPUT_BOX, hInstance, NULL
      ));
  if (WH_TO_HWND(hEditControl) == NULL) {
    return;
  }

  g_ActiveEditControl = hEditControl;
  UI_Edit_SetFont(hEditControl, hFont);
  SetWindowSubclass(WH_TO_HWND(hEditControl), EditSubclassProc, 0, 0);

  if (count >= 0) {
    hListBox = WH_FROM_HWND(CreateWindowExA(
        0, "LISTBOX", "", 
        WS_CHILD | WS_VISIBLE | LBS_NODATA | LBS_OWNERDRAWFIXED,
        20, 105, layout->w - 40, layout->h - 115, WH_TO_HWND(hwndParent), (HMENU)IDC_LIST_BOX, hInstance, NULL
        ));

    if (WH_TO_HWND(hListBox) == NULL) {
      return;
    }

    SetWindowSubclass(WH_TO_HWND(hListBox), ListBoxSubclassProc, 0, 0);
    UI_LB_SetFont(hListBox, hFont);
    UI_LB_SetItemHeight(hListBox, layout->item_height);

    int max_width = layout->w - 40;
    
    HDC hdc = GetDC(WH_TO_HWND(hListBox));
    if (!hdc) {
      return;
    }
    
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    g_initial_items = list;        
    g_initial_count = count;

    if (lb) {
        ListBoxInfo_Init(lb);
        lb->populated_count = count;
        lb->total_matches = count;
        lb->last_items = list;
        lb->last_item_count = count;
        // selected_index will be set by PopulateListBox logic; keep -1 initially
        lb->selected_index = -1;
        // Also populate cache with initial items for early display
        for (int i = 0; i < count && i < MAX_ITEMS; i++) {
            if (list[i]) {
                strncpy(lb->populated_cache[i], list[i], 511);
                lb->populated_cache[i][511] = '\0';
                lb->populated_cache[i][strcspn(lb->populated_cache[i], "\r\n")] = '\0';
            }
        }
    }
    SendMessageA(WH_TO_HWND(hListBox), LB_SETCOUNT, count, 0);

    UpdateWindow(WH_TO_HWND(hListBox)); 

    SelectObject(hdc, hOldFont);
    ReleaseDC(WH_TO_HWND(hListBox), hdc);
    
    UI_SetHorizontalExtent(hListBox, max_width);
  }
}
void SettingsCreateChildControls(WindowHandle hwndParent, HFONT hFont,const WindowLayout* layout) {
  HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrA(WH_TO_HWND(hwndParent), GWLP_HINSTANCE);

  hEditControl = WH_FROM_HWND(CreateWindowExA(
      0, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
      20, 50, layout->w - 40, 40, WH_TO_HWND(hwndParent), (HMENU)IDC_INPUT_BOX, hInstance, NULL
      ));

  if (WH_TO_HWND(hEditControl) == NULL) return;

  g_ActiveEditControl = hEditControl;
  UI_Edit_SetFont(hEditControl, hFont);
  SetWindowSubclass(WH_TO_HWND(hEditControl), SettingsEditSubclassProc, 0, 0);
}
#endif
