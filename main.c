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
#include "minecraft.h"
#include "pixel_equals_handler.h"
#include "wait_for_pixel_handler.h"
#include "region_handler.h"
#include "google_search.h"
#include "factorio.h"
#include "process_killer.h"
#include "number_shortcut.h"
#include "helpers.h"
#include "todo.h"
#include "hotkey_definitions.h"
#include "volume.h"
#pragma comment(lib, "Shlwapi.lib") // Tells the compiler to link the shlwapi library
#define ID_TRAY_ICON 1001
#define WM_TRAYICON (WM_USER + 1)
#define WM_TRIGGER_UNIVERSAL_MACRO (WM_USER + 3)
#define ID_EXIT 2001
#define IDC_INPUT_BOX 101
#define IDC_LIST_BOX 102
#define COLOR_BG      RGB(30, 34, 42)    // A pretty slate-gray/dark background
#define COLOR_TEXT    RGB(220, 223, 228)  // A crisp, clean off-white text
#define TODO_FILE_PATH "todo.txt"
#define WM_TRIGGER_IMG_CAPTURE (WM_USER + 2)
HWND hGlobalTrayWnd = NULL; // Tracks the hidden tray window
HWND hPopupWnd = NULL;
HWND hEditControl = NULL;
HWND hListBox = NULL;
char output_buffer[256] = {0};
bool selection_complete = false;
int region_step = 0; 
long saved_x1 = 0;
long saved_y1 = 0;
long img_x2 = 0, img_y2 = 0; // Add this line if it isn't there!
                             // 
static ULONG_PTR g_gdiplusToken = 0;
#pragma comment(lib, "gdiplus.lib")

RefreshCallback g_refresh_callback = NULL;
EnterCallback g_enter_callback = NULL;
CtrlEnterCallback g_ctrl_enter_callback = NULL;
TabCallback g_tab_callback = NULL;
RenderCallback g_render_callback = NULL;
const char** g_initial_items = NULL;
int g_initial_count = 0;
const char* g_title = NULL;
// --- GLOBAL VARIABLES FOR THE IMAGE CAPTURE ---
int img_region_step = 0;
long img_x1 = 0, img_y1 = 0;

// SPECIFY YOUR CHOSEN PATH HERE (Make sure the path ends with a backslash!)
const char* BASE_SAVE_FOLDER_PATH = "C:\\Users\\HP\\Pictures\\Screenshots\\";

static ULONGLONG g_timer_start_ms = 0;

// Call this once at the very beginning of your main() function
void StartUniversalTimer() {
    g_timer_start_ms = GetTickCount64();
}

// Resets the timer back to 0.0 seconds
void ResetUniversalTimer() {
    g_timer_start_ms = GetTickCount64();
    printf("[TIMER] Reset successfully.\n");
}

// Modifies the timer by adding or subtracting seconds (can pass negative values)
void ModifyUniversalTimer(double seconds_to_add) {
    // Convert seconds to milliseconds and offset our baseline
    LONGLONG ms_offset = (LONGLONG)(seconds_to_add * 1000.0);
    g_timer_start_ms -= ms_offset; 
    printf("[TIMER] Modified baseline by %.2f seconds.\n", seconds_to_add);
}

// Returns the total elapsed seconds as a high-precision decimal (e.g., 12.345)
double GetUniversalTimeSeconds() {
    ULONGLONG current_ms = GetTickCount64();
    
    // Safety guard in case the baseline was modified past current time
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

    // Fast memory-only checks
    if (!IsWindowVisible(hwnd) || GetWindowTextLengthA(hwnd) == 0) {
        return TRUE;
    }

    if (myList->count >= myList->capacity) {
        return FALSE; 
    }

    WindowData* currentItem = &myList->items[myList->count];
    currentItem->hwnd = hwnd;

    // FAST: Grabs title string directly from internal window manager cache
    GetWindowTextA(hwnd, currentItem->title, sizeof(currentItem->title));
    
    // LEAVE EXE BLANK FOR NOW
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
        // Ignore modifier keys
        if (wParam == VK_CONTROL || wParam == VK_SHIFT || wParam == VK_MENU) {
            return DefSubclassProc(hwnd, msg, wParam, lParam);
        }

        // NUMBER SHORTCUTS (0-9) with Ctrl
        if (wParam >= '0' && wParam <= '9') {
            if (ctrl_pressed) {
              HandleNumberShortcut(wParam, hListBox, hEditControl);
              return 0;
            }
            
        }

        // --- ARROW KEY ROUTING ---
        if (wParam == VK_UP || wParam == VK_DOWN) {
            if (hListBox != NULL) {
                int count = SendMessageA(hListBox, LB_GETCOUNT, 0, 0);
                int sel = SendMessageA(hListBox, LB_GETCURSEL, 0, 0);
                
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
                    SendMessageA(hListBox, LB_SETCURSEL, sel, 0);
                    SendMessageA(hParent, WM_COMMAND, MAKEWPARAM(IDC_LIST_BOX, LBN_SELCHANGE), (LPARAM)hListBox);
                }
            }
            return 0;
        }

        // --- ENTER KEY ---
        if (wParam == VK_RETURN) {
            bool ctrl_pressed = (GetKeyState(VK_CONTROL) & 0x8000);
            output_buffer[0] = '\0';
            int selected_index = LB_ERR;

            if (hListBox != NULL) {
                selected_index = SendMessageA(hListBox, LB_GETCURSEL, 0, 0);
            }

            if (selected_index != LB_ERR && hListBox != NULL) {
                SendMessageA(hListBox, LB_GETTEXT, selected_index, (LPARAM)output_buffer);
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

        // --- ESCAPE KEY ---
        if (wParam == VK_ESCAPE) {
            output_buffer[0] = '\0'; 
            selection_complete = true;  
            return 0;
        }

        // --- TAB KEY ---
        if (wParam == VK_TAB) {
            if (g_tab_callback != NULL) {
                g_tab_callback();
            }
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
        case WM_COMMAND: {
            if (LOWORD(wParam) == IDC_INPUT_BOX && HIWORD(wParam) == EN_CHANGE) {
                if (hEditControl && hListBox) {
                    char input_buffer[256] = {0};
                    GetWindowTextA(hEditControl, input_buffer, sizeof(input_buffer));
                    
                    if (g_refresh_callback != NULL) {
                        g_refresh_callback(input_buffer, hListBox);
                    } else {
                        // Fallback: generic filtering
                        SendMessageA(hListBox, LB_RESETCONTENT, 0, 0);
                        if (strlen(input_buffer) == 0) {
                            for (int i = 0; i < g_initial_count; i++) {
                                SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)g_initial_items[i]);
                            }
                        } else {
                            for (int i = 0; i < g_initial_count; i++) {
                                if (StrStrIA(g_initial_items[i], input_buffer) != NULL) {
                                    SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)g_initial_items[i]);
                                }
                            }
                        }
                        SendMessageA(hListBox, LB_SETCURSEL, 0, 0);
                    }
                }
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
              char textBuffer[512];
              SendMessageA(pdis->hwndItem, LB_GETTEXT, pdis->itemID, (LPARAM)textBuffer);
              
              // Draw background
              HBRUSH hBgBrush;
              if (pdis->itemState & ODS_SELECTED) {
                  hBgBrush = CreateSolidBrush(RGB(50, 56, 68));
              } else {
                  hBgBrush = CreateSolidBrush(COLOR_BG);
              }
              FillRect(pdis->hDC, &pdis->rcItem, hBgBrush);
              DeleteObject(hBgBrush);
              
              SetBkMode(pdis->hDC, TRANSPARENT);
              
              RECT textRect = pdis->rcItem;
              
              // ONLY show numbers if Ctrl is held
              if (pdis->itemID < 20) {
                  char num_str[8];
                  snprintf(num_str, sizeof(num_str), "%d.", pdis->itemID + 1);
                  
                  RECT numRect = pdis->rcItem;
                  numRect.left += 5;
                  numRect.right = numRect.left + 25;
                  
                  HFONT hNumFont = CreateFontA(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "0xProto Nerd Font");
                  HFONT hOldFont = (HFONT)SelectObject(pdis->hDC, hNumFont);
                  
                  // Rainbow color
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
                  DeleteObject(hNumFont);
                  
                  textRect.left += 35;
              }
              
              if (g_render_callback != NULL) {
                  g_render_callback(pdis->hDC, textBuffer, &textRect, (pdis->itemState & ODS_SELECTED));
              } else {
                  SetTextColor(pdis->hDC, pdis->itemState & ODS_SELECTED ? RGB(255,255,255) : COLOR_TEXT);
                  DrawTextA(pdis->hDC, textBuffer, -1, &textRect, DT_SINGLELINE | DT_VCENTER);
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
          
          if (g_title != NULL && strlen(g_title) > 0) {
              // Convert the title to wide characters (Unicode)
              wchar_t wtitle[256];
              MultiByteToWideChar(CP_UTF8, 0, g_title, -1, wtitle, 256);
              
              // Use Segoe UI Emoji for colored emojis
              HFONT hTitleFont = CreateFontW(24, 0, 0, 0, FW_NORMAL,
                                             FALSE, FALSE, FALSE,
                                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                             L"0x Proto Nerd Font");
              HFONT hOldFont = (HFONT)SelectObject(hdc, hTitleFont);
              
              SetBkMode(hdc, TRANSPARENT);
              SetTextColor(hdc, RGB(200, 200, 210));
              
              RECT rect;
              GetClientRect(hwnd, &rect);
              
              SIZE sz;
              GetTextExtentPoint32W(hdc, wtitle, (int)wcslen(wtitle), &sz);
              
              RECT titleRect;
              titleRect.left = rect.right - sz.cx - 20;
              titleRect.top = 10;
              titleRect.right = rect.right - 10;
              titleRect.bottom = rect.top + sz.cy + 15;
              
              // Use DrawTextW instead of DrawTextA
              DrawTextW(hdc, wtitle, -1, &titleRect, DT_LEFT | DT_TOP);
              
              SelectObject(hdc, hOldFont);
              DeleteObject(hTitleFont);
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

    // Dynamically scale window height based on exact todo item counts
    layout->h = 95 + (count > 0 ? (count * layout->item_height) : 0); 

    // Cap the maximum layout frame size so it never stretches past screen bounds
    if (layout->h < height) layout->h = height;
    if (layout->h > width) layout->h = width; 

    layout->x = mi.rcMonitor.left + (monitorWidth - layout->w) / 2;
    layout->y = mi.rcMonitor.top + (monitorHeight - layout->h) / 2;
}// 2. Safely generate font engine assets
LRESULT CALLBACK ListBoxSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (msg == WM_NCCALCSIZE && wParam == TRUE) {
        // Intercepting this completely hides the physical scrollbar track 
        // without disabling the underlying keyboard scrolling mechanics.
        return 0; 
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}
// 3. Spawn child elements (Edit field + Listbox options)
void CreateChildControls(HWND hwndParent, HFONT hFont, const char* list[], int count, const WindowLayout* layout) {
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrA(hwndParent, GWLP_HINSTANCE);

    // Create the search input box
    hEditControl = CreateWindowExA(
        0, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        20, 34, layout->w - 40, 40, hwndParent, (HMENU)IDC_INPUT_BOX, hInstance, NULL
    );
    SendMessageA(hEditControl, WM_SETFONT, (WPARAM)hFont, TRUE);
    SetWindowSubclass(hEditControl, EditSubclassProc, 0, 0);

    // Create and fill the list options container
    if (count > 0) {
        // Keep standard dimensions, keep WS_VSCROLL active so arrow indices shift pages
        hListBox = CreateWindowExA(
            0, "LISTBOX", "", 
            WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL | LBS_HASSTRINGS | LBS_OWNERDRAWFIXED,
            20, 105, layout->w - 40, layout->h - 115, hwndParent, (HMENU)IDC_LIST_BOX, hInstance, NULL
        );
        // BIND SUBCLASS HERE: Kills the visual scroll track on calculations
        SetWindowSubclass(hListBox, ListBoxSubclassProc, 0, 0);

        SendMessageA(hListBox, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageA(hListBox, LB_SETITEMHEIGHT, 0, layout->item_height);

        // Find the maximum string length to map the horizontal scrolling range accurately
        int max_width = layout->w - 40;
        HDC hdc = GetDC(hListBox);
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
        
        for (int i = 0; i < count; i++) {
            SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)list[i]);
            
            SIZE sz;
            GetTextExtentPoint32A(hdc, list[i], (int)strlen(list[i]), &sz);
            if (sz.cx + 40 > max_width) {
                max_width = sz.cx + 40; 
            }
        }
        
        SelectObject(hdc, hOldFont);
        ReleaseDC(hListBox, hdc);

        SendMessageA(hListBox, LB_SETHORIZONTALEXTENT, max_width, 0);
    }
}
void ForceWindowFocus(HWND hwndTarget, HWND hwndInput) {
    HWND hForegroundWnd = GetForegroundWindow();
    DWORD foregroundThreadID = GetWindowThreadProcessId(hForegroundWnd, NULL);
    DWORD currentThreadID = GetCurrentThreadId();

    if (foregroundThreadID != currentThreadID && foregroundThreadID != 0) {
        AttachThreadInput(currentThreadID, foregroundThreadID, TRUE);
    }

    ShowWindow(hwndTarget, SW_SHOW);
    SetForegroundWindow(hwndTarget);
    SetActiveWindow(hwndTarget);
    
    SetFocus(hwndInput);
    SendMessageA(hwndInput, WM_ACTIVATE, WA_ACTIVE, 0);

    if (foregroundThreadID != currentThreadID && foregroundThreadID != 0) {
        AttachThreadInput(currentThreadID, foregroundThreadID, FALSE);
    }
}

void SaveRegionToPNG(int x1, int y1, int x2, int y2, const char* fullPath) {
    int width = abs(x2 - x1);
    int height = abs(y2 - y1);
    if (width == 0 || height == 0) return;

    // 1. Capture specific screen coordinates out of the global desktop DC context
    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    HGDIOBJ hOldBitmap = SelectObject(hMemoryDC, hBitmap);

    // Transfer pixels from video memory instantly
    BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, (x1 < x2 ? x1 : x2), (y1 < y2 ? y1 : y2), SRCCOPY);

    // 2. Convert HBITMAP structures into a real C-flat GpBitmap object
    GpBitmap* pBitmap = NULL;
    GdipCreateBitmapFromHBITMAP(hBitmap, NULL, &pBitmap);

    if (pBitmap != NULL) {
        // 3. Look up standard PNG Encoder CLSID system parameters
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

        // 4. Convert filename path string format into standard structural Wide-Chars
        wchar_t wPath[512];
        MultiByteToWideChar(CP_ACP, 0, fullPath, -1, wPath, 512);

        // Save image asset safely down onto disk using Flat C API function calls
        GdipSaveImageToFile((GpImage*)pBitmap, wPath, &pngClsid, NULL);
        
        // Dispose of the bitmap object handle directly
        GdipDisposeImage((GpImage*)pBitmap);
    }

    // 5. Complete memory clean up sweeps to protect RAM profiles
    SelectObject(hMemoryDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);
}

char* PromptUser(const char* window_list[],const char* window_title,int count, int height, int width, int item_height,
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

    printf("[DEBUG-UI] PromptUser called. Count: %d\n", count);

    WindowLayout layout;
    CalculateWindowMetrics(count, &layout, height, width, item_height);
    printf("[DEBUG-UI] Calculated Metrics -> x: %d, y: %d, w: %d, h: %d\n", layout.x, layout.y, layout.w, layout.h);
    
    HFONT hCustomFont = CreateCustomNerdFont(24, "0xProto Nerd Font");

    printf("[DEBUG-UI] Attempting to create window with class 'QuickSwitcherModalClass'...\n");
    hPopupWnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, 
        "QuickSwitcherModalClass", NULL, WS_POPUP, 
        layout.x, layout.y, layout.w, layout.h, NULL, NULL, hInstance, NULL
    );
    SetTimer(hPopupWnd, 999, 50, NULL);

    if (hPopupWnd == NULL) {
        DWORD err = GetLastError();
        printf("[DEBUG-UI] CRITICAL FAILURE: CreateWindowExA returned NULL! System Error Code: %lu\n", err);
        if (err == 1407) {
            printf("[DEBUG-UI] -> Error 1407 means 'Cannot find window class'. Your main() registration failed or was unregistered!\n");
        }
        DeleteObject(hCustomFont);
        return NULL;
    }
    printf("[DEBUG-UI] Window created successfully. Handle: %p\n", (void*)hPopupWnd);

    SetLayeredWindowAttributes(hPopupWnd, 0, 245, LWA_ALPHA);
    CreateChildControls(hPopupWnd, hCustomFont, window_list, count, &layout);
    ForceWindowFocus(hPopupWnd, hEditControl);

    // ============================================================
    // DRAW THE window_title IN THE TOP-RIGHT CORNER
    // ============================================================
    if (window_title != NULL && strlen(window_title) > 0) {
        HDC hdc = GetDC(hPopupWnd);
        HFONT hwindow_titleFont = CreateCustomNerdFont(20, "0xProto Nerd Font");
        HFONT hOldFont = (HFONT)SelectObject(hdc, hwindow_titleFont);
        
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        
        RECT rect;
        GetClientRect(hPopupWnd, &rect);
        
        // Calculate text size
        SIZE sz;
        GetTextExtentPoint32A(hdc, window_title, (int)strlen(window_title), &sz);
        
        // Position: top-right with some padding
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
        } else {
            Sleep(10);
        }
    }
    printf("[DEBUG-UI] Left local loop. Processed %d messages.\n", loop_counter);

    // ============================================================
    // CRITICAL FIX: Destroy the popup window!
    // ============================================================
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
            char* raw_name = PromptUser(empty_list,"Boring", 0, 220, 550,32,NULL,NULL,NULL,NULL,NULL);
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
            
            // Find the action and execute its handler directly
            for (int i = 0; i < MACRO_BINDING_COUNT; i++) {
                if (G_MACRO_BINDINGS[i].action_id == macroAction && 
                    G_MACRO_BINDINGS[i].handler != NULL) {
                    G_MACRO_BINDINGS[i].handler();  // Calls the function directly!
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
            PostQuitMessage(0);
            break;
        }
        
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}


void ClearMdFile(const char* FILE_PATH) {
    // Opening with "w" instantly deletes everything inside the file
    FILE* file = fopen(FILE_PATH, "w");
    
    if (!file) {
        printf("[ERROR] Failed to open file for clearing: %s\n", FILE_PATH);
        return;
    }

    // Closing it immediately leaves it perfectly blank (0 bytes)
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

        // Save the window handle to the global variable
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
                *write_ptr = ','; // Only add a comma if it's NOT the first item
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

        // Check modifiers
        bool alt_down   = (pKeyInfo->flags & LLKHF_ALTDOWN) || (GetKeyState(VK_MENU) & 0x8000);
        bool ctrl_down  = (GetKeyState(VK_CONTROL) & 0x8000);
        bool shift_down = (GetKeyState(VK_SHIFT) & 0x8000) || 
                          (GetKeyState(VK_LSHIFT) & 0x8000) || 
                          (GetKeyState(VK_RSHIFT) & 0x8000);
        
        // Timer cleanup
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

        // ============================================================
        // STEP 1: Check ALL hotkey bindings (handles ALL modifier combos)
        // ============================================================
        if (ProcessHotkey(G_HOTKEY_BINDINGS, HOTKEY_BINDING_COUNT, 
                          pressed_vk, modifiers, fgTitle)) {
            return 1;
        }

        // ============================================================
        // STEP 2: Universal bindings (Macros, App, Misc, QuickLink, etc.)
        // ============================================================
        MacroAction matchedAction = MACRO_NONE;
        int matchedDelay = 0;
        const char* snippet_text = NULL;
        
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

    // Register window class
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
    
    // ============================================================
    // FIXED: Use 0 padding instead of 4096
    // ============================================================
    for (int i = 0; i < FILE_ENTRY_COUNT; i++) {
        SanitizeFile(G_FILE_ENTRIES[i].path);
    }

    for (int i = 0; i < FILE_ENTRY_COUNT; i++) {
        G_FILE_ENTRIES[i].mf = (MappedFile*)malloc(sizeof(MappedFile));
        const char* path = G_FILE_ENTRIES[i].path;
        
        // CRITICAL FIX: 0 padding = NO GARBAGE!
        if (!FileInit(path, G_FILE_ENTRIES[i].mf, 0)) {
            printf("[SYSTEM] Failed to open %s. Creating new.\n", G_FILE_ENTRIES[i].name);
            HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                CloseHandle(hFile);
                FileInit(path, G_FILE_ENTRIES[i].mf, 0);  // <-- 0 padding here too!
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
    
    return 0;
}
