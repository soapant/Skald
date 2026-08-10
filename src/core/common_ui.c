#include "common.h"
#include "config.h"
#include "helpers.h"
#include "core/common_ui.h"
#include <commctrl.h>

bool g_add_alias_to_string = true;
char g_alias_display[MAX_ITEMS][512] = {0};
int g_alias_display_count = 0;
bool g_alias_display_active = false;

static char g_last_filter_state[256] = {0};
static int g_last_filter_state_count = 0;
static bool g_last_filter_state_valid = false;

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

void PopulateListBox(HWND hListBox, const char* filter, 
    const char** items, int item_count,
    bool (*filter_match)(const char* item, const char* filter)) {

  WinTimer timer_alias = TimerStart();
  if (!hListBox) return;
  if (g_seq_state.waiting) return;
  if (!filter) filter = "";
  TimerEnd(timer_alias,"PopulateListBox - Aliases");


  WinTimer timer_total = TimerStart();
  int old_count = g_TotalMatches;

  WinTimer timer_current_sel = TimerStart();
  int current_sel = g_SelectedIndex; 
  char current_text[256] = {0};
  if (current_sel >= 0 && current_sel < g_populated_count) {
    strncpy(current_text, g_populated_cache[current_sel], sizeof(current_text) - 1);
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
      g_last_items == items && 
      g_last_item_count == item_count &&
      g_last_filter_state_valid &&
      strlen(clean_filter) > strlen(g_last_filter_state) &&
      strncmp(clean_filter, g_last_filter_state, strlen(g_last_filter_state)) == 0) {

    use_previous = 1;
    prev_count = g_last_filter_state_count;
  }
  TimerEnd(timer_inc, "  PopulateListBox - Incremental check");

  WinTimer timer_filter = TimerStart();
  if (use_previous && prev_count == 0) {
    total_matches = 0;
    printf("[SKIP] No previous matches for '%s', skipping filter for '%s'\n",
        g_last_filter_state, clean_filter);
  } else if (use_previous && prev_count > 0 && g_history_count > 0) {
    int* prev_matches = g_match_history[g_history_count - 1];
    printf("[OPTIMIZE] Incremental: '%s' -> '%s'\n", g_filter_history[g_history_count - 1], clean_filter);

    for (int i = 0; i < prev_count && total_matches < 4000; i++) {
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
        match = (StrStrIA(search_text, clean_filter) != NULL);
      }

      if (match) {
        g_matching_indices[total_matches] = idx;
        total_matches++;
      }
    }
  } else {
    bool empty_filter = (clean_filter[0] == '\0');
    for (int i = 0; i < item_count && total_matches < 4000; i++) {
      if (!items[i]) continue;

      if (empty_filter) {
        g_matching_indices[total_matches] = i;
        total_matches++;
        continue;
      }

      const char* search_text = items[i];
      if (g_alias_display_active && i < g_alias_display_count && g_alias_display[i][0] != '\0') {
        search_text = g_alias_display[i];
      }
      bool match = filter_match ? filter_match(search_text, clean_filter) : (StrStrIA(search_text, clean_filter) != NULL);

      if (match) {
        g_matching_indices[total_matches] = i;
        total_matches++;
      }
    }
  }
  TimerEnd(timer_filter, "  PopulateListBox - Filtering");

  WinTimer timer_history = TimerStart();
  if (total_matches > 0 && strlen(clean_filter) > 0 && g_history_count < 20) {
    strncpy(g_filter_history[g_history_count], clean_filter, sizeof(g_filter_history[0]) - 1);
    g_match_count_history[g_history_count] = total_matches;
    memcpy(g_match_history[g_history_count], g_matching_indices, total_matches * sizeof(int));
    g_history_count++;
    printf("[HISTORY] Saved '%s' (%d matches)\n", clean_filter, total_matches);
  }
  TimerEnd(timer_history, "  PopulateListBox - History save");

  WinTimer timer_matches = TimerStart();
  if (total_matches == 0 && item_count > 0 && strlen(clean_filter) == 0) {
    int force_count = (item_count < 30) ? item_count : 30;
    for (int i = 0; i < force_count; i++) {
      g_matching_indices[total_matches] = i;
      total_matches++;
    }
    g_history_count = 0;
  }
  TimerEnd(timer_matches," PopulateListBox - Matches");

  WinTimer timer_pool = TimerStart();
  int pool_max = MAX_VISIBLE_ITEMS;
  if (total_matches > pool_max) {
    total_matches = pool_max;
  }
  TimerEnd(timer_pool, "  PopulateListBox - Pool adjust");

  WinTimer timer_cache = TimerStart();
  g_populated_count = 0;

  for (int i = 0; i < total_matches; i++) {
    int raw_data_index = g_matching_indices[i];
    if (!items[raw_data_index]) continue;

    strncpy(g_populated_cache[g_populated_count], items[raw_data_index], 511);
    g_populated_cache[g_populated_count][511] = '\0';
    g_populated_cache[g_populated_count][strcspn(g_populated_cache[g_populated_count], "\r\n")] = '\0';
    g_populated_count++;
  }
  TimerEnd(timer_cache, "  PopulateListBox - Cache build");

  WinTimer timer_ui = TimerStart();
  UI_LB_SetCount(hListBox, total_matches);
  if (!g_seq_state.waiting) {
    if (IsNormalMode()) {
      int keep;
      if (RememberLastIndex) {
        keep = g_SelectedIndex;
        if (keep < 0 && last_index >= 0) {
          keep = last_index;
        }
        if (total_matches == 0) keep = -1;
        else if (keep < 0) keep = 0;
        else if (keep >= total_matches) keep = total_matches - 1;
        UI_NavigateTo(keep);
      } else {
        keep = (total_matches > 0) ? 0 : -1;
        UI_SetSelection(hListBox, keep, g_SelectedIndex);
      }
    } else if (total_matches != 0 && strlen(filter) > 0) {
      UI_SetSelection(hListBox, 0, g_SelectedIndex);
      draw_background = false;
    } else {
      UI_SetSelection(hListBox, -1, g_SelectedIndex);
      draw_background = true;
    }
    if (total_matches == 0) {
      if (hDisplayMessage != NULL) {
        KillTimer(hDisplayMessage, 999);
        DestroyWindow(hDisplayMessage);
        hDisplayMessage = NULL;
      }
    }
  }
  TimerEnd(timer_ui, "  PopulateListBox - UI update");

  WinTimer timer_cleanup = TimerStart();
  if (strlen(clean_filter) > 0) {
    strncpy(g_last_filter_state, clean_filter, sizeof(g_last_filter_state) - 1);
    g_last_filter_state[sizeof(g_last_filter_state) - 1] = '\0';
    g_last_filter_state_count = total_matches;
    g_last_filter_state_valid = true;
  } else {
    g_last_filter_state_valid = false;
  }
  g_last_items = items;
  g_last_item_count = item_count;
  g_TotalMatches = total_matches;
  if (total_matches != old_count) {
    draw_background = true;
  }
  TimerEnd(timer_cleanup, "  PopulateListBox - Cleanup");

  TimerEnd(timer_total, "PopulateListBox TOTAL");
}

COLORREF GetRainbowColor(float t, float s, float v) {
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
    return RGB((BYTE)(r * 255), (BYTE)(g * 255), (BYTE)(b * 255));
  } else {
    return COLOR_TEXT;
  }
}

HFONT CreateCustomNerdFont(int height, const char* face_name) {
  return CreateFontA(
      height, 0, 0, 0, FW_NORMAL,
      FALSE, FALSE, FALSE,
      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
      face_name
      );
}

void UpdateTextCaret(HWND hwndEditControl)
{
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

  if (caretWidth != lastWidth || hwndEditControl != lastHwnd) {
    DestroyCaret();
    CreateCaret(hwndEditControl, NULL, caretWidth, caretHeight);
    ShowCaret(hwndEditControl);

    SetCaretBlinkTime(INFINITE);

    lastWidth = caretWidth;
    lastHwnd = hwndEditControl;
  }
}

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

void SimpleCreateChildControls(HWND hwndParent, HFONT hFont, const char* string, int count, const WindowLayout* layout) {
  HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrA(hwndParent, GWLP_HINSTANCE);

  hEditControl = CreateWindowExA(
      0, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
      20, 50, layout->w - 40, 40, hwndParent, (HMENU)IDC_INPUT_BOX, hInstance, NULL
      );

  if (!hEditControl) return;

  g_ActiveEditControl = hEditControl;
  UI_Edit_SetFont(hEditControl, hFont);
  SetWindowSubclass(hEditControl, SimpleEditSubclassProc, 0, 0);

  if (count > 0) {
    hListBox = CreateWindowExA(
        0, "LISTBOX", "", 
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOSEL, 
        20, 105, layout->w - 40, layout->h - 115, hwndParent, (HMENU)IDC_LIST_BOX, hInstance, NULL
        );

    if (!hListBox) return;

    SetWindowSubclass(hListBox, SimpleListBoxSubclassProc, 0, 0);
    UI_LB_SetFont(hListBox, hFont);
    UI_LB_SetItemHeight(hListBox, layout->item_height);

    int max_width = layout->w - 40;
    HDC hdc = GetDC(hListBox);
    if (!hdc) return;
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)string);
    SendMessage(hListBox, LB_SETCURSEL, (WPARAM)-1, 0);
    UpdateWindow(hListBox); 

    SelectObject(hdc, hOldFont);
    ReleaseDC(hListBox, hdc);
  }
}

void ForceWindowFocus(HWND hwndTarget, HWND hwndInput) {
  ShowWindow(hwndTarget, SW_SHOW);

  SetWindowPos(hwndTarget, HWND_TOPMOST, 0, 0, 0, 0,
      SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW | SWP_ASYNCWINDOWPOS);

  HWND hForeground = GetForegroundWindow();
  DWORD foregroundThreadID = GetWindowThreadProcessId(hForeground, NULL);
  DWORD targetThreadID = GetWindowThreadProcessId(hwndTarget, NULL);
  DWORD currentThreadID = GetCurrentThreadId();

  if (foregroundThreadID != currentThreadID)
    AttachThreadInput(currentThreadID, foregroundThreadID, TRUE);
  if (targetThreadID != currentThreadID)
    AttachThreadInput(currentThreadID, targetThreadID, TRUE);

  SetForegroundWindow(hwndTarget);
  SetActiveWindow(hwndTarget);
  BringWindowToTop(hwndTarget);

  if (hwndInput != NULL) {
    SetFocus(hwndInput);
    UI_Edit_UpdateUIState(hwndInput, UIS_CLEAR, UISF_HIDEFOCUS);
    UI_Edit_SetSel(hwndInput, 0, 0);
  }

  if (foregroundThreadID != currentThreadID)
    AttachThreadInput(currentThreadID, foregroundThreadID, FALSE);
  if (targetThreadID != currentThreadID)
    AttachThreadInput(currentThreadID, targetThreadID, FALSE);
}

void CreateChildControls(HWND hwndParent, HFONT hFont, const char* list[], int count, const WindowLayout* layout) {
  printf("[DEBUG] CreateChildControls: Starting with count=%d\n", count);
  
  HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrA(hwndParent, GWLP_HINSTANCE);
  printf("[DEBUG] hInstance = %p\n", hInstance);

  printf("[DEBUG] Creating edit control...\n");
  hEditControl = CreateWindowExA(
      0, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
      20, 50, layout->w - 40, 40, hwndParent, (HMENU)IDC_INPUT_BOX, hInstance, NULL
      );
  if (!hEditControl) {
    printf("[ERROR] Failed to create edit control! GetLastError=%lu\n", GetLastError());
    return;
  }
  printf("[DEBUG] Edit control created: %p\n", hEditControl);

  g_ActiveEditControl = hEditControl;
  UI_Edit_SetFont(hEditControl, hFont);
  SetWindowSubclass(hEditControl, EditSubclassProc, 0, 0);
  printf("[DEBUG] Edit control initialized\n");

  if (count >= 0) {
    printf("[DEBUG] Creating list box with count=%d\n", count);
    hListBox = CreateWindowExA(
        0, "LISTBOX", "", 
        WS_CHILD | WS_VISIBLE | LBS_NODATA | LBS_OWNERDRAWFIXED,
        20, 105, layout->w - 40, layout->h - 115, hwndParent, (HMENU)IDC_LIST_BOX, hInstance, NULL
        );

    if (!hListBox) {
      printf("[ERROR] Failed to create list box! GetLastError=%lu\n", GetLastError());
      return;
    }
    printf("[DEBUG] List box created: %p\n", hListBox);

    SetWindowSubclass(hListBox, ListBoxSubclassProc, 0, 0);
    UI_LB_SetFont(hListBox, hFont);
    UI_LB_SetItemHeight(hListBox, layout->item_height);
    printf("[DEBUG] List box configured: item_height=%d\n", layout->item_height);

    int max_width = layout->w - 40;
    printf("[DEBUG] max_width=%d\n", max_width);
    
    HDC hdc = GetDC(hListBox);
    if (!hdc) {
      printf("[ERROR] Failed to get DC for list box! GetLastError=%lu\n", GetLastError());
      return;
    }
    printf("[DEBUG] DC obtained: %p\n", hdc);
    
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    printf("[DEBUG] Old font selected: %p\n", hOldFont);

    g_initial_items = list;        
    g_initial_count = count;
    printf("[DEBUG] Stored initial items: list=%p, count=%d\n", list, count);

    printf("[DEBUG] Setting item count to %d...\n", count);
    UI_SetItemCount(hListBox, count);
    int countCheck = UI_GetCount(hListBox);
    printf("[DEBUG] UI_GetCount after set = %d\n", countCheck);

    printf("[DEBUG] Redrawing list box...\n");
    UI_RedrawNow(hListBox);
    UpdateWindow(hListBox); 
    printf("[DEBUG] List box redrawn\n");

    SelectObject(hdc, hOldFont);
    ReleaseDC(hListBox, hdc);
    printf("[DEBUG] DC released\n");
    
    UI_SetHorizontalExtent(hListBox, max_width);
    printf("[DEBUG] Horizontal extent set to %d\n", max_width);
  } else {
    printf("[DEBUG] count < 0, skipping list box creation\n");
  }
  
  printf("[DEBUG] CreateChildControls: Completed successfully\n");
}
