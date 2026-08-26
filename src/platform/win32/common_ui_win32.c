// Win32-only UI helpers: child control creation, window metrics, caret.
// Excluded from Linux builds by the build script ( *_win32.c filter ).

#include "common.h"
#include "config.h"
#include "helpers.h"
#include "core/common_ui.h"
#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>

HFONT CreateCustomNerdFont(int height, const char* face_name) {
  return FH_TO_HFONT(Platform_CreateFont(height, face_name));
}

void UpdateTextCaret(WindowHandle hwndEditControl)
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

  if (caretWidth != lastWidth || WH_TO_HWND(hwndEditControl) != lastHwnd) {
    DestroyCaret();
    CreateCaret(WH_TO_HWND(hwndEditControl), NULL, caretWidth, caretHeight);
    ShowCaret(WH_TO_HWND(hwndEditControl));

    SetCaretBlinkTime(INFINITE);

    lastWidth = caretWidth;
    lastHwnd = WH_TO_HWND(hwndEditControl);
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
void CalculateSettingsWindowMetrics(int count, WindowLayout* layout,int height , int width,int item_height) {
  CalculateWindowMetrics(count, layout, height, width, item_height);
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


    if (lb) {
        ListBoxInfo_Init(lb);
        lb->populated_count = count;
        lb->total_matches = count;
        lb->last_items = list;
        lb->last_item_count = count;
        lb->selected_index = -1;
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
