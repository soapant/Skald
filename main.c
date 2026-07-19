#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <commctrl.h>
#include <psapi.h>
#include <gdiplus.h>
#include <stdio.h>
#include <shellapi.h>
#include <stdbool.h>
#include <process.h>
#include <shlwapi.h> 
#include "types.h"
#include "config.h"
#include "calc.h"
#include "video_player.h"
#include "music_player.h"
#include "timer.h"
#include "bindings.h"
#include "explorer.h"
#include "screenshot.h"
#include "terminal_command.h"
#include "zoxide.h"
#include "nvim_zoxide.h"
#include "yazi_zoxide.h"
#include "pos_handler.h"
#include "focus_switcher.h"
#include "color_handler.h"
#include "pixel_equals_handler.h"
#include "wait_for_pixel_handler.h"
#include "region_handler.h"
#include "google_search.h"
#include "process_killer.h"
#include "number_shortcut.h"
#include "helpers.h"
#include "common_ui.h"
#include "todo.h"
#include "hotkey_definitions.h"
#include "modal.h"
#pragma comment(lib, "Shlwapi.lib") 
#define ID_TRAY_ICON 1001
#define WM_TRAYICON (WM_USER + 1)
#define WM_TRIGGER_UNIVERSAL_MACRO (WM_USER + 3)
#define ID_EXIT 2001
#define IDC_INPUT_BOX 101
#define IDC_LIST_BOX 102
#define COLOR_BG      RGB(30, 34, 42)   
#define COLOR_TEXT    RGB(220, 223, 228)
#define TODO_FILE_PATH "todo.txt"
#define WM_TRIGGER_IMG_CAPTURE (WM_USER + 2)
HWND hGlobalTrayWnd = NULL;
HWND hPopupWnd = NULL;
HWND hEditControl = NULL;
HWND hListBox = NULL;
char output_buffer[256] = {0};
bool selection_complete = false;
int region_step = 0; 
long saved_x1 = 0;
long saved_y1 = 0;
long img_x2 = 0, img_y2 = 0; 
static ULONG_PTR g_gdiplusToken = 0;
#pragma comment(lib, "gdiplus.lib")

RefreshCallback g_refresh_callback = NULL;
EnterCallback g_enter_callback = NULL;
CtrlEnterCallback g_ctrl_enter_callback = NULL;
TabCallback g_tab_callback = NULL;
RenderCallback g_render_callback = NULL;
const char** g_initial_items = NULL;
int g_initial_count = 0;
int g_SelectedIndex = -1; 
int g_TotalMatches = 0; 
#define MAX_ITEMS 1000
char g_populated_cache[MAX_ITEMS][512];
int g_populated_count = 0;
int g_view_offset = 0;
const char* g_title = NULL;
int img_region_step = 0;
long img_x1 = 0, img_y1 = 0;
HFONT g_hNumFont = NULL;
HFONT g_hMainFont = NULL;

const char* BASE_SAVE_FOLDER_PATH = "C:\\Users\\HP\\Pictures\\Screenshots\\";

static ULONGLONG g_timer_start_ms = 0;

void StartUniversalTimer() {
    g_timer_start_ms = GetTickCount64();
}

void ResetUniversalTimer() {
    g_timer_start_ms = GetTickCount64();
}

void ModifyUniversalTimer(double seconds_to_add) {
    LONGLONG ms_offset = (LONGLONG)(seconds_to_add * 1000.0);
    g_timer_start_ms -= ms_offset; 
    printf("[TIMER] Modified baseline by %.2f seconds.\n", seconds_to_add);
}

double GetUniversalTimeSeconds() {
    ULONGLONG current_ms = GetTickCount64();
    
    if (current_ms < g_timer_start_ms) {
        return 0.0;
    }
    
    ULONGLONG elapsed_ms = current_ms - g_timer_start_ms;
    return (double)elapsed_ms / 1000.0;
}
static bool IsWindowBlacklisted(const char* window_title, const char* blacklist[], int count) {
    for (int i = 0; i < count; i++) {
        if (StrStrIA(window_title, blacklist[i]) != NULL) {
            return true;
        }
    }
    return false;
}
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    WindowList* myList = (WindowList*)lParam;

    if (!IsWindowVisible(hwnd) || GetWindowTextLengthA(hwnd) == 0) {
        return TRUE;
    }

    if (myList->count >= myList->capacity) {
        return FALSE; 
    }

    WindowData* currentItem = &myList->items[myList->count];
    currentItem->hwnd = hwnd;

    GetWindowTextA(hwnd, currentItem->title, sizeof(currentItem->title));
    
    currentItem->exe[0] = '\0'; 

    myList->count++;
    return TRUE;
}
LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    HWND hParent = GetParent(hwnd);
    HWND hListBox = GetDlgItem(hParent, IDC_LIST_BOX);
    HWND hEditControl = hwnd;
    bool ctrl_pressed = (GetKeyState(VK_CONTROL) & 0x8000);

    if (msg == WM_KEYDOWN) {
        if (wParam == VK_CONTROL || wParam == VK_SHIFT || wParam == VK_MENU) {
            return DefSubclassProc(hwnd, msg, wParam, lParam);
        }

        if (wParam >= '0' && wParam <= '9') {
            if (ctrl_pressed) {
              HandleNumberShortcut(wParam, hListBox, hEditControl);
              return 0;
            }
            
        }

        if (wParam == VK_UP || wParam == VK_DOWN) {
            if (hListBox != NULL) {
                int count = UI_GetCount(hListBox);
                int sel = UI_GetSelection(hListBox);
                
                if (count > 0) {
                    if (sel == LB_ERR) {
                        sel = 0;
                    } else {
                        if (wParam == VK_UP) {
                            sel = (sel > 0) ? sel - 1 : 0;
                        } else {
                            sel = (sel < count - 1) ? sel + 1 : count - 1;
                        }
                    }
                    
                    g_SelectedIndex = sel;
                    UI_SetSelection(hListBox, g_SelectedIndex);
                    
                    int visibleCount = 13;
                    int scrollTo = sel - (visibleCount / 2);
                    if (scrollTo < 0) scrollTo = 0;
                    SendMessageA(hListBox, LB_SETTOPINDEX, scrollTo, 0);
                }
            }
            return 0;
        }

        if (wParam == VK_RETURN) {
            bool ctrl_pressed = (GetKeyState(VK_CONTROL) & 0x8000);
            output_buffer[0] = '\0';
            int selected_index = LB_ERR;

            if (hListBox != NULL) {
                selected_index = UI_GetSelection(hListBox);
            }

            if (selected_index != LB_ERR && hListBox != NULL) {
                UI_GetText(hListBox, selected_index, output_buffer, sizeof(output_buffer));
            } else {
                GetWindowTextA(hwnd, output_buffer, sizeof(output_buffer));
            }

            if (ctrl_pressed) {
                if (g_ctrl_enter_callback != NULL) {
                    g_ctrl_enter_callback(output_buffer);
                }
                if (g_refresh_callback != NULL) {
                    char current_filter[256] = {0};
                    GetWindowTextA(hEditControl, current_filter, sizeof(current_filter));
                    g_refresh_callback(current_filter, hListBox);
                }
                return 0;
            } else {
                if (g_enter_callback != NULL) {
                    g_enter_callback(output_buffer);
                }
                selection_complete = true; 
                return 0;
            }
        }

        if (wParam == VK_ESCAPE) {
            output_buffer[0] = '\0'; 
            selection_complete = true;  
            return 0;
        }
        
        if (wParam == VK_TAB) {
            if (g_tab_callback != NULL) {
                g_tab_callback();
            }
            return 0;
        }
    } 
    
    return DefSubclassProc(hwnd, msg, wParam, lParam);
} 

LRESULT CALLBACK SimpleEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    HWND hParent = GetParent(hwnd);
    HWND hListBox = GetDlgItem(hParent, IDC_LIST_BOX);
    HWND hEditControl = hwnd;

    if (msg == WM_KEYDOWN) {
        if (wParam == VK_CONTROL || wParam == VK_SHIFT || wParam == VK_MENU) {
            return DefSubclassProc(hwnd, msg, wParam, lParam);
        }
        if (wParam == VK_RETURN) {
            GetWindowTextA(hEditControl, output_buffer, sizeof(output_buffer));
            
            selection_complete = true; 
            
            return 0; 
        }

        if (wParam == VK_ESCAPE) {
            output_buffer[0] = '\0'; 
            selection_complete = true;  
            return 0;
        }
        
    } 
    
    return DefSubclassProc(hwnd, msg, wParam, lParam);
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
LRESULT CALLBACK PopupWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
      case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, COLOR_TEXT);
            SetBkColor(hdc, COLOR_BG);
            return (LRESULT)CreateSolidBrush(COLOR_BG);
        }
        case WM_CREATE: {
                          
                if (!g_hNumFont) {
                    g_hNumFont = CreateFontA(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "0xProto Nerd Font");
                }
                if (!g_hMainFont) {
                    g_hMainFont = CreateFontA(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "0xProto Nerd Font");
                }
                break;
        }
        case WM_COMMAND: {
            int wmId   = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);

            if (wmId == IDC_INPUT_BOX && wmEvent == EN_CHANGE) {
                if (GetKeyState(VK_UP) & 0x8000 || 
                    GetKeyState(VK_DOWN) & 0x8000 || 
                    GetKeyState(VK_RETURN) & 0x8000 ||
                    GetKeyState(VK_ESCAPE) & 0x8000) 
                {
                    break;
                }

                if (hEditControl && hListBox) {
                    char input_buffer[256] = {0};
                    GetWindowTextA(hEditControl, input_buffer, sizeof(input_buffer));
                    
                    if (g_refresh_callback != NULL) {
                        g_refresh_callback(input_buffer, hListBox);
                    }
                    if (strlen(input_buffer) == 0 || g_populated_count == 0) {
                        g_SelectedIndex = -1; 
                    } else {
                        g_SelectedIndex = 0;  
                    }
                    SendMessageA(hListBox, LB_SETTOPINDEX, 0, 0);
                    UI_Redraw(hListBox);
                }
            }
            
            if (wmId == IDC_LIST_BOX && wmEvent == LBN_SELCHANGE) {
                break; 
            }
            
            break;
        }
        case WM_MEASUREITEM: {
            MEASUREITEMSTRUCT* pmis = (MEASUREITEMSTRUCT*)lParam;
            if (pmis->CtlID == IDC_LIST_BOX) {
                pmis->itemHeight = 32;
                return TRUE;
            }
            break;
        }
      case WM_DRAWITEM: {
          DRAWITEMSTRUCT* pdis = (DRAWITEMSTRUCT*)lParam;
          if (pdis->CtlID == IDC_LIST_BOX && pdis->itemID != -1) {
            
              BOOL isSelected = (pdis->itemID == g_SelectedIndex);
              
              HBRUSH hBgBrush;
              if (isSelected) {
                  hBgBrush = CreateSolidBrush(RGB(50, 56, 68));
              } else {
                  hBgBrush = CreateSolidBrush(COLOR_BG);
              }
              FillRect(pdis->hDC, &pdis->rcItem, hBgBrush);
              DeleteObject(hBgBrush);
              
              if (pdis->itemID == -1 || pdis->itemID >= g_populated_count) {
                  return TRUE; 
              }
              
              const char* textBuffer = "";
              if (pdis->itemID < g_populated_count) {
                  textBuffer = g_populated_cache[pdis->itemID];
              }
              if (!textBuffer) textBuffer = "";
              
              SetBkMode(pdis->hDC, TRANSPARENT);
              
              RECT textRect = pdis->rcItem;
              
              if (pdis->itemID < 99) {
                  char num_str[8];
                  snprintf(num_str, sizeof(num_str), "%d.", pdis->itemID + 1);
                  
                  RECT numRect = pdis->rcItem;
                  numRect.left += 5;
                  numRect.right = numRect.left + 25;
                  
                  HFONT hOldFont = (HFONT)SelectObject(pdis->hDC, g_hNumFont);
                  
                  float hue = (float)pdis->itemID / 20.0f * 240.0f;
                  float h = hue / 60.0f;
                  float s = 0.85f;
                  float v = 0.85f;
                  int i = (int)h;
                  float f = h - i;
                  float p = v * (1 - s);
                  float q = v * (1 - s * f);
                  float t = v * (1 - s * (1 - f));
                  float r, g, b;
                  switch (i) {
                      case 0: r = v; g = t; b = p; break;
                      case 1: r = q; g = v; b = p; break;
                      case 2: r = p; g = v; b = t; break;
                      case 3: r = p; g = q; b = v; break;
                      case 4: r = t; g = p; b = v; break;
                      default: r = v; g = p; b = q; break;
                  }
                  
                  SetTextColor(pdis->hDC, RGB((BYTE)(r * 255), (BYTE)(g * 255), (BYTE)(b * 255)));
                  DrawTextA(pdis->hDC, num_str, -1, &numRect, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
                  
                  SelectObject(pdis->hDC, hOldFont);
                  
                  textRect.left += 35;
                  
              } else {
                  textRect.left += 10;
              }
              
              if (g_render_callback != NULL) {
                  g_render_callback(pdis->hDC, textBuffer, &textRect, isSelected);
              } else {
                  HFONT hOldMainFont = (HFONT)SelectObject(pdis->hDC, g_hMainFont);
                  SetTextColor(pdis->hDC, RGB(255, 255, 255));
                  DrawTextA(pdis->hDC, textBuffer, -1, &textRect, DT_SINGLELINE | DT_VCENTER);
                  SelectObject(pdis->hDC, hOldMainFont);
              }
              
              return TRUE;
          }
          break;
      }
      case WM_PAINT: {
          PAINTSTRUCT ps;
          HDC hdc = BeginPaint(hwnd, &ps);
          
          HBRUSH hBrush = CreateSolidBrush(COLOR_BG);
          FillRect(hdc, &ps.rcPaint, hBrush);
          DeleteObject(hBrush);
          
          RECT rect;
          GetClientRect(hwnd, &rect);
          
          if (g_vim_mode != NULL) {
              wchar_t wmode[256];
              MultiByteToWideChar(CP_UTF8, 0, g_vim_mode, -1, wmode, 256);
              
              HFONT hOldFont = (HFONT)SelectObject(hdc, g_hMainFont);
              
              SetBkMode(hdc, TRANSPARENT);
              
              if (g_current_mode == MODE_NORMAL) {
                  SetTextColor(hdc, RGB(100, 200, 255));
              } else if (g_current_mode == MODE_INSERT) {
                  SetTextColor(hdc, RGB(100, 255, 100));
              } else {
                  SetTextColor(hdc, RGB(173, 3, 222));
              }
              
              SIZE sz;
              GetTextExtentPoint32W(hdc, wmode, (int)wcslen(wmode), &sz);
              
              RECT modeRect;
              modeRect.left = 15;
              modeRect.top = 10;
              modeRect.right = modeRect.left + sz.cx + 10;
              modeRect.bottom = modeRect.top + sz.cy + 10;
              
              DrawTextW(hdc, wmode, -1, &modeRect, DT_LEFT | DT_TOP);
              
              SelectObject(hdc, hOldFont);
          }
          
          if (g_title != NULL && strlen(g_title) > 0) {
              wchar_t wtitle[256];
              MultiByteToWideChar(CP_UTF8, 0, g_title, -1, wtitle, 256);
              
              HFONT hOldFont = (HFONT)SelectObject(hdc, g_hMainFont);
              
              SetBkMode(hdc, TRANSPARENT);
              SetTextColor(hdc, RGB(255, 255, 255));
              
              SIZE sz;
              GetTextExtentPoint32W(hdc, wtitle, (int)wcslen(wtitle), &sz);
              
              RECT titleRect;
              titleRect.left = rect.right - sz.cx - 20;
              titleRect.top = 10;
              titleRect.right = rect.right - 10;
              titleRect.bottom = rect.top + sz.cy + 15;
              
              DrawTextW(hdc, wtitle, -1, &titleRect, DT_LEFT | DT_TOP);
              
              SelectObject(hdc, hOldFont);
          }
          
          EndPaint(hwnd, &ps);
          break;
      }
      case WM_TIMER:
        if (wParam == 999) {
            static bool last_ctrl = false;
            bool current_ctrl = IsCtrlPressed();
            if (current_ctrl != last_ctrl) {
                last_ctrl = current_ctrl;
                if (hListBox != NULL && IsWindow(hListBox)) {
                    InvalidateRect(hListBox, NULL, TRUE);
                }
            }
        }
        break;              
      case WM_SETCURSOR: {
            if (LOWORD(lParam) == HTCLIENT) {
                HCURSOR hCursor;
                if (IsInsertMode()) {
                    hCursor = LoadCursor(NULL, IDC_IBEAM);  
                } else if (IsNormalMode()) {
                    hCursor = LoadCursor(NULL, IDC_ARROW);  
                } else {
                    hCursor = LoadCursor(NULL, IDC_CROSS);  
                }
                SetCursor(hCursor);
                return TRUE;
            }
            break;
          }                
        default:
            return DefWindowProcA(hwnd, msg, wParam, lParam);
            
    }
    return 0;
}
    POINT ptPrimary = { 0, 0 }; 

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
LRESULT CALLBACK ListBoxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (uMsg) {
        case WM_LBUTTONDOWN: {
            LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);
            int x = (short)LOWORD(lParam);
            int y = (short)HIWORD(lParam);

            DWORD itemInfo = (DWORD)SendMessageA(hWnd, LB_ITEMFROMPOINT, 0, MAKELPARAM(x, y));
            int clickedIndex = (int)LOWORD(itemInfo);
            int isOutsideClient = (int)HIWORD(itemInfo);

            if (isOutsideClient == 0 && clickedIndex >= 0 && clickedIndex < g_populated_count) {
                g_SelectedIndex = clickedIndex;
                InvalidateRect(hWnd, NULL, TRUE);
                SendMessageA(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hWnd), LBN_SELCHANGE), (LPARAM)hWnd);
            }
            return res;
        }

        case WM_NCDESTROY:
            RemoveWindowSubclass(hWnd, ListBoxSubclassProc, uIdSubclass);
            break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}
void CreateChildControls(HWND hwndParent, HFONT hFont, const char* list[], int count, const WindowLayout* layout) {
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrA(hwndParent, GWLP_HINSTANCE);
    
    hEditControl = CreateWindowExA(
        0, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        20, 50, layout->w - 40, 40, hwndParent, (HMENU)IDC_INPUT_BOX, hInstance, NULL
    );
    
    if (!hEditControl) return;
    
    SendMessageA(hEditControl, WM_SETFONT, (WPARAM)hFont, FALSE);
    SetWindowSubclass(hEditControl, EditSubclassProc, 0, 0);


    if (count > 0) {

        hListBox = CreateWindowExA(
            0, "LISTBOX", "", 
            WS_CHILD | WS_VISIBLE | LBS_NODATA | LBS_OWNERDRAWFIXED,
            20, 105, layout->w - 40, layout->h - 115, hwndParent, (HMENU)IDC_LIST_BOX, hInstance, NULL
        );

        if (!hListBox) return;
        
        SetWindowSubclass(hListBox, ListBoxSubclassProc, 0, 0);
        SendMessageA(hListBox, WM_SETFONT, (WPARAM)hFont, FALSE);
        SendMessageA(hListBox, LB_SETITEMHEIGHT, 0, layout->item_height);

        int max_width = layout->w - 40;
        HDC hdc = GetDC(hListBox);
        if (!hdc) return;
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
        
        g_initial_items = list;        
        g_initial_count = count;
        
        UI_SetItemCount(hListBox, count);
        int countCheck = UI_GetCount(hListBox);
        
        UI_Redraw(hListBox);
        UpdateWindow(hListBox); 
        
        GdiFlush(); 
        
        SelectObject(hdc, hOldFont);
        ReleaseDC(hListBox, hdc);
        UI_SetHorizontalExtent(hListBox, max_width);
        
        g_refresh_callback("", hListBox);
    }
}
void SimpleCreateChildControls(HWND hwndParent, HFONT hFont, const char* string, int count, const WindowLayout* layout) {
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrA(hwndParent, GWLP_HINSTANCE);
    
    hEditControl = CreateWindowExA(
        0, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        20, 50, layout->w - 40, 40, hwndParent, (HMENU)IDC_INPUT_BOX, hInstance, NULL
    );
    
    if (!hEditControl) return;
    
    SendMessageA(hEditControl, WM_SETFONT, (WPARAM)hFont, FALSE);
    SetWindowSubclass(hEditControl, SimpleEditSubclassProc, 0, 0);

    if (count > 0) {
        hListBox = CreateWindowExA(
            0, "LISTBOX", "", 
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOSEL, 
            20, 105, layout->w - 40, layout->h - 115, hwndParent, (HMENU)IDC_LIST_BOX, hInstance, NULL
        );

        if (!hListBox) return;
        
        SetWindowSubclass(hListBox, ListBoxSubclassProc, 0, 0);
        SendMessageA(hListBox, WM_SETFONT, (WPARAM)hFont, FALSE);
        SendMessageA(hListBox, LB_SETITEMHEIGHT, 0, layout->item_height);

        int max_width = layout->w - 40;
        HDC hdc = GetDC(hListBox);
        if (!hdc) return;
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
        
        SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)string);
        UpdateWindow(hListBox); 
        
        GdiFlush(); 
        
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
        SendMessageA(hwndInput, WM_UPDATEUISTATE, MAKEWPARAM(UIS_CLEAR, UISF_HIDEFOCUS), 0);
        SendMessageA(hwndInput, EM_SETSEL, 0, 0);
    }

    if (foregroundThreadID != currentThreadID)
        AttachThreadInput(currentThreadID, foregroundThreadID, FALSE);
    if (targetThreadID != currentThreadID)
        AttachThreadInput(currentThreadID, targetThreadID, FALSE);
}

void SaveRegionToPNG(int x1, int y1, int x2, int y2, const char* fullPath) {
    int width = abs(x2 - x1);
    int height = abs(y2 - y1);
    if (width == 0 || height == 0) return;

    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    HGDIOBJ hOldBitmap = SelectObject(hMemoryDC, hBitmap);

    BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, (x1 < x2 ? x1 : x2), (y1 < y2 ? y1 : y2), SRCCOPY);

    GpBitmap* pBitmap = NULL;
    GdipCreateBitmapFromHBITMAP(hBitmap, NULL, &pBitmap);

    if (pBitmap != NULL) {
        CLSID pngClsid;
        UINT num = 0, size = 0;
        GdipGetImageEncodersSize(&num, &size); 
        
        ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
        if (pImageCodecInfo != NULL) {
            GdipGetImageEncoders(num, size, pImageCodecInfo); 
            for (UINT j = 0; j < num; ++j) {
                if (wcscmp(pImageCodecInfo[j].MimeType, L"image/png") == 0) {
                    pngClsid = pImageCodecInfo[j].Clsid;
                    break;
                }
            }
            free(pImageCodecInfo);
        }

        wchar_t wPath[512];
        MultiByteToWideChar(CP_ACP, 0, fullPath, -1, wPath, 512);

        GdipSaveImageToFile((GpImage*)pBitmap, wPath, &pngClsid, NULL);
        
        GdipDisposeImage((GpImage*)pBitmap);
    }

    
    SelectObject(hMemoryDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);
}

char* PromptUser(const char* window_list[], const char* window_title, int count, int height, int width, int item_height,
                 RefreshCallback refresh_callback, 
                 EnterCallback enter_callback,
                 CtrlEnterCallback ctrl_enter_callback,
                 TabCallback tab_callback,
                 RenderCallback render_callback) {
    HINSTANCE hInstance = GetModuleHandle(NULL);
    selection_complete = false;
    output_buffer[0] = '\0';
    g_initial_items = window_list;
    g_initial_count = count;
    g_refresh_callback = refresh_callback;
    g_enter_callback = enter_callback;
    g_ctrl_enter_callback = ctrl_enter_callback;
    g_tab_callback = tab_callback;
    g_render_callback = render_callback;
    g_title = window_title;
    
    SetMode(MODE_INSERT);

    WindowLayout layout;
    CalculateWindowMetrics(count, &layout, height, width, item_height);
    
    HFONT hCustomFont = CreateCustomNerdFont(24, "0xProto Nerd Font");

    hPopupWnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, 
        "QuickSwitcherModalClass", NULL, WS_POPUP | WS_CLIPCHILDREN, 
        layout.x, layout.y, layout.w, layout.h, NULL, NULL, hInstance, NULL
    );
    
    UpdateCursorForMode(MODE_INSERT);
    SetTimer(hPopupWnd, 999, 50, NULL);

    if (hPopupWnd == NULL) {
        DWORD err = GetLastError();
        if (err == 1407) {
        }
        DeleteObject(hCustomFont);
        return NULL;
    }

    CreateChildControls(hPopupWnd, hCustomFont, window_list, count, &layout);
    
    SetLayeredWindowAttributes(hPopupWnd, 0, 245, LWA_ALPHA);

    ForceWindowFocus(hPopupWnd, hEditControl);

    if (window_title != NULL && strlen(window_title) > 0) {
        HDC hdc = GetDC(hPopupWnd);
        HFONT hwindow_titleFont = CreateCustomNerdFont(20, "0xProto Nerd Font");
        HFONT hOldFont = (HFONT)SelectObject(hdc, hwindow_titleFont);
        
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        
        RECT rect;
        GetClientRect(hPopupWnd, &rect);
        
        SIZE sz;
        GetTextExtentPoint32A(hdc, window_title, (int)strlen(window_title), &sz);
        
        rect.left = rect.right - sz.cx - 20;
        rect.top = 10;
        rect.bottom = rect.top + sz.cy + 10;
        
        DrawTextA(hdc, window_title, -1, &rect, DT_LEFT | DT_TOP);
        
        SelectObject(hdc, hOldFont);
        DeleteObject(hwindow_titleFont);
        ReleaseDC(hPopupWnd, hdc);
    }

    printf("[DEBUG-UI] Entering local message modal loop...\n");
    MSG msg;
    int loop_counter = 0;
    while (!selection_complete) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            loop_counter++;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
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

    DeleteObject(hCustomFont); 
    selection_complete = false;
    g_refresh_callback = NULL;
    g_enter_callback = NULL;
    g_ctrl_enter_callback = NULL;
    g_tab_callback = NULL;
    g_render_callback = NULL;

    printf("[DEBUG-UI] Returning from PromptUser. Buffer Length: %zu\n", strlen(output_buffer));
    return (strlen(output_buffer) > 0) ? output_buffer : NULL;
}
char* SimplePromptUser(const char* prompt, const char* window_title,int height, int width, int item_height) {
    HINSTANCE hInstance = GetModuleHandle(NULL);
    selection_complete = false;
    output_buffer[0] = '\0';
    g_title = window_title;
    
    SetMode(MODE_INSERT);

    WindowLayout layout;
    CalculateWindowMetrics(1, &layout, height, width, item_height);
    
    HFONT hCustomFont = CreateCustomNerdFont(24, "0xProto Nerd Font");

    hPopupWnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, 
        "QuickSwitcherModalClass", NULL, WS_POPUP | WS_CLIPCHILDREN, 
        layout.x, layout.y, layout.w, layout.h, NULL, NULL, hInstance, NULL
    );
    
    UpdateCursorForMode(MODE_INSERT);
    
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
    
    if (window_title != NULL && strlen(window_title) > 0) {
        HDC hdc = GetDC(hPopupWnd);
        HFONT hwindow_titleFont = CreateCustomNerdFont(20, "0xProto Nerd Font");
        HFONT hOldFont = (HFONT)SelectObject(hdc, hwindow_titleFont);
        
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        
        RECT rect;
        GetClientRect(hPopupWnd, &rect);
        
        SIZE sz;
        GetTextExtentPoint32A(hdc, window_title, (int)strlen(window_title), &sz);
        
        rect.left = rect.right - sz.cx - 20;
        rect.top = 10;
        rect.bottom = rect.top + sz.cy + 10;
        
        DrawTextA(hdc, window_title, -1, &rect, DT_LEFT | DT_TOP);
        
        SelectObject(hdc, hOldFont);
        DeleteObject(hwindow_titleFont);
        ReleaseDC(hPopupWnd, hdc);
    }

    MSG msg;
    int loop_counter = 0;
    while (!selection_complete) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            loop_counter++;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
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

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            NOTIFYICONDATA nid = {0};
            nid.cbSize = sizeof(NOTIFYICONDATA);
            nid.hWnd = hwnd;
            nid.uID = ID_TRAY_ICON;
            nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
            nid.uCallbackMessage = WM_TRAYICON;
            
            nid.hIcon = (HICON)LoadImage(NULL,
                             TEXT(IconPath), 
                             IMAGE_ICON, 
                             0, 0, 
                             LR_LOADFROMFILE | LR_DEFAULTSIZE);
            lstrcpy(nid.szTip, "Switch Window");
            


            Shell_NotifyIcon(NIM_ADD, &nid);
            break;
        }
        
        case WM_TRAYICON: {
            if (lParam == WM_RBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                
                HMENU hMenu = CreatePopupMenu();
                AppendMenu(hMenu, MF_STRING, ID_EXIT, "Exit");
                
                SetForegroundWindow(hwnd);
                TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0, hwnd, NULL);
                DestroyMenu(hMenu);
            }
            break;
        }
        case WM_TRIGGER_IMG_CAPTURE: {
            printf("[DEBUG-WNDPROC] Core tray window thread intercepted WM_TRIGGER_IMG_CAPTURE successfully!\n");
            const char* empty_list[1] = { "" };
            
            printf("[DEBUG-WNDPROC] Handing control off to PromptUser...\n");
            char* raw_name = PromptUser(empty_list,"Boring", 0, 520, 650,32,NULL,NULL,NULL,NULL,NULL);
            printf("[DEBUG-WNDPROC] Returned back to WndProc execution sequence.\n");

            if (raw_name != NULL && strlen(raw_name) > 0) {
                char final_file_path[512];
                snprintf(final_file_path, sizeof(final_file_path), "%s%s.png", BASE_SAVE_FOLDER_PATH, raw_name);
                
                printf("[DEBUG-WNDPROC] Saving pixel buffer bounds: (%ld,%ld) to (%ld,%ld)\n", img_x1, img_y1, img_x2, img_y2);
                SaveRegionToPNG((int)img_x1, (int)img_y1, (int)img_x2, (int)img_y2, final_file_path);
                printf("[IMG REGION] Successfully rendered output file to: %s\n", final_file_path);
            } else {
                printf("[DEBUG-WNDPROC] Screenshot save aborted because raw_name is null/empty.\n");
            }
            break;
        }
        
        case WM_TRIGGER_UNIVERSAL_MACRO: {
            MacroAction macroAction = (MacroAction)wParam;
            int macroDelay = (int)lParam;
            
            printf("[MACRO-WNDPROC] Received action=%d, delay=%d\n", macroAction, macroDelay);
            
            
            for (int i = 0; i < MACRO_BINDING_COUNT; i++) {
                if (G_MACRO_BINDINGS[i].action_id == macroAction && 
                    G_MACRO_BINDINGS[i].handler != NULL) {
                    G_MACRO_BINDINGS[i].handler();  
                    break;
                }
            }
            break;
        }
        
        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_EXIT) {
                PostQuitMessage(0);
            }
            break;
        }
        
        case WM_DESTROY: {
            NOTIFYICONDATA nid = {0};
            nid.cbSize = sizeof(NOTIFYICONDATA);
            nid.hWnd = hwnd;
            nid.uID = ID_TRAY_ICON;
            Shell_NotifyIcon(NIM_DELETE, &nid);
            if (g_hNumFont) {
                DeleteObject(g_hNumFont);
                g_hNumFont = NULL;
            }
            if (g_hMainFont) {
                DeleteObject(g_hMainFont);
                g_hMainFont = NULL;
            }
            PostQuitMessage(0);
            break;
        }
        
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}


void ClearMdFile(const char* FILE_PATH) {
    
    FILE* file = fopen(FILE_PATH, "w");
    
    if (!file) {
        printf("[ERROR] Failed to open file for clearing: %s\n", FILE_PATH);
        return;
    }

    
    fclose(file);
    
    printf("[DEBUG] File '%s' successfully cleared.\n", FILE_PATH);
}

HHOOK hKeyboardHook = NULL;
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

int InitializeTrayIcon() {
    HINSTANCE hInstance = GetModuleHandle(NULL);
    if (TRAY) {
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

void StripCheckboxes(char* str) {
    char* read_ptr = str;
    char* write_ptr = str;
    bool is_first = true;

    while (*read_ptr != '\0') {
        if (strncmp(read_ptr, "- [ ] ", 6) == 0) {
            if (!is_first) {
                *write_ptr = ','; 
                write_ptr++;
            }
            is_first = false; 
            read_ptr += 6;
        } else {
            *write_ptr = *read_ptr;
            write_ptr++;
            read_ptr++;
        }
    }
    *write_ptr = '\0';
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
            ClearMdFile(TEMP_FILE_PATH);
        }

        HWND hForeground = GetForegroundWindow();
        char fgTitle[256] = {0};
        if (hForeground != NULL) {
            GetWindowTextA(hForeground, fgTitle, sizeof(fgTitle));
        }

        int pressed_vk = (int)pKeyInfo->vkCode;
        int modifiers = HOTKEY_MOD_NONE;
        if (alt_down) modifiers |= HOTKEY_MOD_ALT;
        if (shift_down) modifiers |= HOTKEY_MOD_SHIFT;
        if (ctrl_down) modifiers |= HOTKEY_MOD_CTRL;

        if (ProcessHotkey(G_HOTKEY_BINDINGS, HOTKEY_BINDING_COUNT, 
                          pressed_vk, modifiers, fgTitle)) {
            return 1;
        }

        MacroAction matchedAction = MACRO_NONE;
        int matchedDelay = 0;
        const char* snippet_text = NULL;
        
        MotionAction vimAction = MOTION_NONE;
        const char* matchedSequence = NULL;
        int typedCount = 0;
        if (CheckVimMotion(pressed_vk, modifiers, &vimAction, &matchedSequence, &typedCount)) {
            ExecuteMotion(vimAction, matchedSequence, typedCount);
            return 1;
        }
        
        if (CheckTextSnippetBindings(pressed_vk, modifiers, &snippet_text)) {
            if (snippet_text != NULL) {
                printf("[SNIPPET] Copying to clipboard: \"%s\"\n", snippet_text);
                CopyToClipboard(snippet_text);
                return 1;
            }
        }
        
        if (CheckMacroBindings(pressed_vk, modifiers, &matchedAction, &matchedDelay)) {
            if (matchedAction != MACRO_NONE && hGlobalTrayWnd != NULL) {
                printf("[MACRO] ✅ Triggering action %d with delay %d\n", matchedAction, matchedDelay);
                PostMessage(hGlobalTrayWnd, WM_TRIGGER_UNIVERSAL_MACRO, (WPARAM)matchedAction, (LPARAM)matchedDelay);
                return 1;
            }
        }
        
        if (CheckAppHotkeyBindings(pressed_vk, modifiers)) return 1;
        if (CheckMiscHotkeyBindings(pressed_vk, modifiers)) return 1;
        if (CheckQuickLinkHotkeyBindings(pressed_vk, modifiers)) return 1;
        if (HandleFocusSwitcher(pressed_vk, modifiers, fgTitle)) return 1;
        if (HandleMusicPlayer(pressed_vk, modifiers, fgTitle)) return 1;
        if (HandleVideoPlayer(pressed_vk, modifiers, fgTitle)) return 1;
        if (HandleBulkKillProcess(pressed_vk, modifiers, fgTitle)) return 1;
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
    StartUniversalTimer();
    InitializeTrayIcon();
    
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
    wc.hbrBackground = CreateSolidBrush(COLOR_BG);
    wc.lpszClassName = "QuickSwitcherModalClass";
    
    if (RegisterClassA(&wc)) {
        printf("[SYSTEM-BOOT] QuickSwitcherModalClass registered successfully with OS.\n");
    } else {
        printf("[SYSTEM-BOOT] CRITICAL WARNING: Failed to register window class! Error: %lu\n", GetLastError());
    }
    
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
    
    return 0;
}
