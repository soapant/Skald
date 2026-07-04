#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <commctrl.h>
#include <psapi.h>
#include <gdiplus.h>
#include <stdio.h>
#include <shellapi.h>
#include <stdbool.h>
#include <shlwapi.h> // REQUIRED for StrStrIA case-insensitive search
#include "calc.h"
#include "todo.h"
#include "pos_handler.h"
#include "focus_switcher.h"
#include "color_handler.h"
#include "minecraft.h"
#include "pixel_equals_handler.h"
#include "wait_for_pixel_handler.h"
#include "region_handler.h"
#include "google_search.h"
#include "factorio.h"
#include "app_launcher.h"
#include "nvim_zoxide.h"
#include "process_killer.h"
#pragma comment(lib, "Shlwapi.lib") // Tells the compiler to link the shlwapi library
#include "config.h"

#define ID_TRAY_ICON 1001
#define WM_TRAYICON (WM_USER + 1)
#define WM_TRIGGER_UNIVERSAL_MACRO (WM_USER + 3)
#define ID_EXIT 2001
#define IDC_INPUT_BOX 101
#define IDC_LIST_BOX 102
#define COLOR_BG      RGB(30, 34, 42)    // A pretty slate-gray/dark background
#define COLOR_TEXT    RGB(220, 223, 228)  // A crisp, clean off-white text
#define TODO_FILE_PATH "todo.md"
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
extern int img_region_step;
extern long img_x1, img_y1;
long img_x2 = 0, img_y2 = 0; // Add this line if it isn't there!
static ULONG_PTR g_gdiplusToken = 0;
typedef enum { MODE_NONE, MODE_CALCULATOR, MODE_LAUNCHER,MODE_TODO,MODE_KILLER} ModalMode;
static ModalMode g_current_mode = MODE_NONE;
#pragma comment(lib, "gdiplus.lib")


// --- GLOBAL VARIABLES FOR THE IMAGE CAPTURE ---
int img_region_step = 0;
long img_x1 = 0, img_y1 = 0;

// SPECIFY YOUR CHOSEN PATH HERE (Make sure the path ends with a backslash!)
const char* BASE_SAVE_FOLDER_PATH = "C:\\Users\\HP\\Pictures\\Screenshots\\";

typedef struct {
    int index;
    HWND hwnd;
    char title[256];
    char exe[256];
} SavedWindow;

typedef struct {
    HWND hwnd;
    char title[256]; // Dynamic allocation is better, but a fixed buffer keeps it simple
    char exe[256];
} WindowData;

// Structure to pass your list into the callback
typedef struct {
    WindowData* items;
    int count;
    int capacity;
} WindowList;
// Struct to keep metric management clean
typedef struct {
    int x, y, w, h;
    int item_height;
} WindowLayout;
// Global variable tracking the baseline start time
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

    if (msg == WM_KEYDOWN) {
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
                printf("[DEBUG-SUBCLASS] Enter caught. Pulled from ListBox: \"%s\"\n", output_buffer);
            } else {
                GetWindowTextA(hwnd, output_buffer, sizeof(output_buffer));
                printf("[DEBUG-SUBCLASS] Enter caught. Pulled from Edit box: \"%s\"\n", output_buffer);
            }

            // --- CTRL+ENTER: Flip the operation (calculator mode) ---
            if (ctrl_pressed) {
                printf("[DEBUG-SUBCLASS] Ctrl+Enter intercepted!\n");
                
                if (g_current_mode == MODE_TODO) {
                    remove_todo_item_from_file(output_buffer);
                    
                    if (hListBox && selected_index != LB_ERR) {
                        SendMessageA(hListBox, LB_DELETESTRING, selected_index, 0);
                        int new_count = SendMessageA(hListBox, LB_GETCOUNT, 0, 0);
                        if (new_count > 0) {
                            int new_sel = selected_index > 0 ? selected_index - 1 : 0;
                            SendMessageA(hListBox, LB_SETCURSEL, new_sel, 0);
                        }
                    }
                } 
                else if (g_current_mode == MODE_KILLER) {
                    terminate_process_by_name(output_buffer);
                    collect_running_processes();
                    if (hListBox) {
                        SendMessageA(hListBox, LB_RESETCONTENT, 0, 0);
                        for (int i = 0; i < G_PK_COUNT; i++) {
                            SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)G_PK_LIST[i]);
                        }
                        SendMessageA(hListBox, LB_SETCURSEL, selected_index > 0 ? selected_index - 1 : 0, 0);
                    }
                }
                // --- CALCULATOR FLIP: Put the result back into the edit box ---
                else if (g_current_mode == MODE_CALCULATOR) {
                    if (selected_index != LB_ERR && hListBox != NULL) {
                        char result_buffer[256] = {0};
                        SendMessageA(hListBox, LB_GETTEXT, selected_index, (LPARAM)result_buffer);
                        
                        char* clean_result = result_buffer;
                        if (strncmp(result_buffer, "= ", 2) == 0) {
                            clean_result = result_buffer + 2;
                        }
                        // Put the result back into the edit box
                        SetWindowTextA(hEditControl, clean_result);
                        int text_len = strlen(clean_result);
                        SendMessageA(hEditControl, EM_SETSEL, text_len, text_len);
                    }
                }
                else if (g_current_mode == MODE_LAUNCHER) {
                    if (selected_index != LB_ERR && hListBox != NULL) {
                        char app_name[256] = {0};
                        SendMessageA(hListBox, LB_GETTEXT, selected_index, (LPARAM)app_name);
                        
                        // Launch as administrator
                        launch_app_as_admin(app_name);
                        
                        // Close the window after launching
                        selection_complete = true;
                        return 0;
                    }
                }
                
                output_buffer[0] = '\0'; 
                return 0;
            }

            // --- Regular Enter: Close the window ---
            selection_complete = true;  
            return 0;
        }

        // --- ESCAPE: Close the window ---
        if (wParam == VK_ESCAPE) {
            output_buffer[0] = '\0'; 
            selection_complete = true;  
            return 0;
        }

        if (wParam == VK_TAB) {
            char result_buffer[256] = {0};
            
            if (g_current_mode == MODE_CALCULATOR) {
                // Get the flipped expression string
                char flip_expr[256] = {0};
                get_flip_expression(flip_expr, sizeof(flip_expr));
                
                // Put it in the edit box
                SetWindowTextA(hEditControl, flip_expr);
                int text_len = strlen(flip_expr);
                SendMessageA(hEditControl, EM_SETSEL, text_len, text_len);
                
                // Trigger the output to update
                SendMessageA(hParent, WM_COMMAND, MAKEWPARAM(IDC_INPUT_BOX, EN_CHANGE), (LPARAM)hEditControl);
            } 
            else if (g_current_mode == MODE_LAUNCHER) {
                if (hListBox != NULL) {
                    int sel = SendMessageA(hListBox, LB_GETCURSEL, 0, 0);
                    if (sel != LB_ERR) {
                        SendMessageA(hListBox, LB_GETTEXT, sel, (LPARAM)result_buffer);
                        SetWindowTextA(hEditControl, result_buffer);
                        int text_len = strlen(result_buffer);
                        SendMessageA(hEditControl, EM_SETSEL, text_len, text_len);
                    }
                }
            }
    return 0; 
}
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
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

                            if (g_current_mode == MODE_CALCULATOR) {
                                char result_buffer[256] = {0};
                                live_evaluate(input_buffer, result_buffer, sizeof(result_buffer));
                                
                                SendMessageA(hListBox, LB_RESETCONTENT, 0, 0);
                                SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)result_buffer);
                                SendMessageA(hListBox, LB_SETCURSEL, 0, 0);
                            } 
                            else if (g_current_mode == MODE_LAUNCHER) {
                                SendMessageA(hListBox, LB_RESETCONTENT, 0, 0);
                                for (int i = 0; i < G_APP_COUNT; i++) {
                                    if (strlen(input_buffer) == 0 || StrStrIA(G_APPS[i].name, input_buffer) != NULL) {
                                        SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)G_APPS[i].name);
                                    }
                                }
                                SendMessageA(hListBox, LB_SETCURSEL, 0, 0);
                            }
                            else if (g_current_mode == MODE_KILLER) {
                              char filter_text[128];
                              GetWindowTextA(hEditControl, filter_text, sizeof(filter_text));

                              // Clear the ListBox completely to prepare for filtered results
                              SendMessageA(hListBox, LB_RESETCONTENT, 0, 0);
                              // Loop through our process cache and only add items that match the text
                              for (int i = 0; i < G_PK_COUNT; i++) {
                                  // StrStrIA does a case-insensitive substring search (from shlwapi.h)
                                  if (filter_text[0] == '\0' || StrStrIA(G_PK_LIST[i], filter_text) != NULL) {
                                      SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)G_PK_LIST[i]);
                                  }
                              }
                              // Auto-highlight the top filtered item
                              SendMessageA(hListBox, LB_SETCURSEL, 0, 0);
                          }
                        }
                    }
                    break;
                }

            // Execute application choice when chosen from list view
            if (LOWORD(wParam) == IDC_LIST_BOX && HIWORD(wParam) == LBN_DBLCLK) {
                int index = SendMessageA(hListBox, LB_GETCURSEL, 0, 0);
                if (index != LB_ERR && g_current_mode == MODE_LAUNCHER) {
                    char selection[128] = {0};
                    SendMessageA(hListBox, LB_GETTEXT, index, (LPARAM)selection);
                    launch_app_by_name(selection);
                    PostMessage(hwnd, WM_CLOSE, 0, 0); // Close window after executing
                }
            }
            break;
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
                char textBuffer[256];
                SendMessageA(pdis->hwndItem, LB_GETTEXT, pdis->itemID, (LPARAM)textBuffer);

                static int horizontal_scroll_shift = 0;
                static int last_selected_id = -1;

                if (pdis->itemState & ODS_SELECTED) {
                    if (last_selected_id != pdis->itemID) {
                        last_selected_id = pdis->itemID;
                        horizontal_scroll_shift = 0; 
                    }

                    if (GetKeyState(VK_RIGHT) & 0x8000) {
                        horizontal_scroll_shift += 8;
                    } else if (GetKeyState(VK_LEFT) & 0x8000) {
                        horizontal_scroll_shift -= 8;
                        if (horizontal_scroll_shift < 0) horizontal_scroll_shift = 0;
                    }
                }

                HBRUSH hBgBrush;
                if (pdis->itemState & ODS_SELECTED) {
                    hBgBrush = CreateSolidBrush(RGB(50, 56, 68)); 
                    SetTextColor(pdis->hDC, RGB(255, 255, 255)); 
                } else {
                    hBgBrush = CreateSolidBrush(COLOR_BG);
                    SetTextColor(pdis->hDC, COLOR_TEXT);
                }

                FillRect(pdis->hDC, &pdis->rcItem, hBgBrush);
                DeleteObject(hBgBrush);

                SetBkMode(pdis->hDC, TRANSPARENT);
                
                HRGN hRgn = CreateRectRgn(pdis->rcItem.left + 10, pdis->rcItem.top, pdis->rcItem.right - 10, pdis->rcItem.bottom);
                SelectClipRgn(pdis->hDC, hRgn);

                RECT textRect = pdis->rcItem;
                
                if (pdis->itemState & ODS_SELECTED) {
                    SIZE sz;
                    GetTextExtentPoint32A(pdis->hDC, textBuffer, (int)strlen(textBuffer), &sz);
                    int max_allowed_shift = sz.cx - (pdis->rcItem.right - pdis->rcItem.left - 40);
                    if (max_allowed_shift < 0) max_allowed_shift = 0;
                    if (horizontal_scroll_shift > max_allowed_shift) horizontal_scroll_shift = max_allowed_shift;
                    textRect.left += (10 - horizontal_scroll_shift);
                } else {
                    textRect.left += 10;
                }
                
                DrawTextA(pdis->hDC, textBuffer, -1, &textRect, DT_SINGLELINE | DT_VCENTER);
                
                SelectClipRgn(pdis->hDC, NULL);
                DeleteObject(hRgn);
                
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
            EndPaint(hwnd, &ps);
            break;
        }
                       
        default:
            return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    return 0;
}
    POINT ptPrimary = { 0, 0 }; 

void CalculateWindowMetrics(int count, WindowLayout* layout,int height , int width) {
    POINT ptPrimary = { 0, 0 }; 
    HMONITOR hMonitor = MonitorFromPoint(ptPrimary, MONITOR_DEFAULTTOPRIMARY);

    MONITORINFO mi = {0};
    mi.cbSize = sizeof(MONITORINFO);
    GetMonitorInfoA(hMonitor, &mi);

    int monitorWidth = mi.rcMonitor.right - mi.rcMonitor.left;
    int monitorHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;
    
    layout->w = 900;          
    layout->item_height = 32; 

    // Dynamically scale window height based on exact todo item counts
    layout->h = 95 + (count > 0 ? (count * layout->item_height) : 0); 

    // Cap the maximum layout frame size so it never stretches past screen bounds
    if (layout->h < height) layout->h = height;
    if (layout->h > width) layout->h = width; 

    layout->x = mi.rcMonitor.left + (monitorWidth - layout->w) / 2;
    layout->y = mi.rcMonitor.top + (monitorHeight - layout->h) / 2;
}// 2. Safely generate font engine assets
HFONT CreateCustomNerdFont(int height, const char* face_name) {
    return CreateFontA(
        height, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        face_name
    );
}
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
        20, 20, layout->w - 40, 40, hwndParent, (HMENU)IDC_INPUT_BOX, hInstance, NULL
    );
    SendMessageA(hEditControl, WM_SETFONT, (WPARAM)hFont, TRUE);
    SetWindowSubclass(hEditControl, EditSubclassProc, 0, 0);

    // Create and fill the list options container
    if (count > 0) {
        // Keep standard dimensions, keep WS_VSCROLL active so arrow indices shift pages
        hListBox = CreateWindowExA(
            0, "LISTBOX", "", 
            WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL | LBS_HASSTRINGS | LBS_OWNERDRAWFIXED,
            20, 75, layout->w - 40, layout->h - 95, hwndParent, (HMENU)IDC_LIST_BOX, hInstance, NULL
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

char* PromptQuickSwitcher(const char* window_list[], int count,int height,int width) {
    HINSTANCE hInstance = GetModuleHandle(NULL);
    selection_complete = false;
    output_buffer[0] = '\0';

    printf("[DEBUG-UI] PromptQuickSwitcher called. Count: %d\n", count);

    WindowLayout layout;
    CalculateWindowMetrics(count, &layout,height,width);
    printf("[DEBUG-UI] Calculated Metrics -> x: %d, y: %d, w: %d, h: %d\n", layout.x, layout.y, layout.w, layout.h);
    
    HFONT hCustomFont = CreateCustomNerdFont(24, "0xProto Nerd Font");

    printf("[DEBUG-UI] Attempting to create window with class 'QuickSwitcherModalClass'...\n");
    hPopupWnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, 
        "QuickSwitcherModalClass", NULL, WS_POPUP, 
        layout.x, layout.y, layout.w, layout.h, NULL, NULL, hInstance, NULL
    );

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

    printf("[DEBUG-UI] Entering local message modal loop...\n");
    MSG msg;
    int loop_counter = 0;
    while (!selection_complete) {
        // Use PeekMessage so we can exit the millisecond selection_complete is true
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            loop_counter++;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            Sleep(10); // Prevents the thread from consuming 100% CPU while idling
        }
    }
    printf("[DEBUG-UI] Left local loop. Processed %d messages.\n", loop_counter);

    if (hPopupWnd != NULL && IsWindow(hPopupWnd)) {
        printf("[DEBUG-UI] Teardown: Destroying hPopupWnd actively...\n");
        DestroyWindow(hPopupWnd);
        hPopupWnd = NULL;
    }

    printf("[DEBUG-UI] Flushing trailing queue entries...\n");
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DeleteObject(hCustomFont); 
    selection_complete = false;

    printf("[DEBUG-UI] Returning from PromptQuickSwitcher. Buffer Length: %zu\n", strlen(output_buffer));
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
                             TEXT("D:\\retard games\\miscellaneous\\switch_window.ico"), 
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
            
            printf("[DEBUG-WNDPROC] Handing control off to PromptQuickSwitcher...\n");
            char* raw_name = PromptQuickSwitcher(empty_list, 0, 220, 550);
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
            
            printf("[MACRO-WNDPROC] Received WM_TRIGGER_UNIVERSAL_MACRO in WndProc! action=%d, delay=%d\n", macroAction, macroDelay);
            printf("[MACRO-WNDPROC] hwnd=%p\n", hwnd);
            
            switch(macroAction) {
            case MACRO_NONE: {
                printf("[MACRO] ACTION_NONE received - ignoring\n");
                break;
            }
              case MACRO_FACTORIO_MAKE_TAG: {
                    create_tag();
                  }
                                            
              case MACRO_FACTORIO_SEARCH_FACTORY: {
                    search_factory();
                  }
              // In WndProc WM_TRIGGER_UNIVERSAL_MACRO case:
              case MACRO_MINECRAFT_COPY: {
                  minecraft_type_command("//copy");
                  break;
              }

              case MACRO_MINECRAFT_PASTE: {
                  minecraft_type_command("//paste -a");
                  break;
              }

              case MACRO_MINECRAFT_UNDO: {
                  minecraft_type_command("//undo");
                  break;
              }

              case MACRO_MINECRAFT_UP: {
                  minecraft_type_command("//up 1");
                  break;
              }

              case MACRO_MINECRAFT_ROTATE: {
                  minecraft_type_command("//rotate 90");
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
HHOOK hMouseHook = NULL;  // ADD THIS LINE
#include <shlwapi.h> // REQUIRED for StrStrIA case-insensitive search
#pragma comment(lib, "Shlwapi.lib") // Tells the compiler to link the shlwapi library

// Move the heavy lookup logic into a dedicated micro-helper
void FetchExeNameOnDemand(HWND hwnd, char* outExe, DWORD maxLen) {
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != 0) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
        if (hProcess) {
            char path[MAX_PATH] = {0};
            if (GetProcessImageFileNameA(hProcess, path, sizeof(path))) {
                char* lastBackslash = strrchr(path, '\\');
                if (lastBackslash) {
                    strncpy(outExe, lastBackslash + 1, maxLen);
                } else {
                    strncpy(outExe, path, maxLen);
                }
            }
            CloseHandle(hProcess);
            return;
        }
    }
    strncpy(outExe, "Unknown.exe", maxLen);
}

char* FilterAndPromptSwitcher(WindowList myWindows) {
    char *list_of_not_wanteds[] = {"Ray", "Input", "Manager", "Settings"};
    size_t size_of_list = sizeof(list_of_not_wanteds) / sizeof(list_of_not_wanteds[0]);

    SavedWindow tracked_windows[100]; 
    int saved_count = 0;
    static char formatted_ui_strings[100][512]; 
    const char* ui_list[100];

    for (int i = 0; i < myWindows.count; i++) {
        HWND targetHwnd = myWindows.items[i].hwnd;
        char* targetTitle = myWindows.items[i].title;
        bool not_wanted = false;
        
        // 1. Filter out the hundreds of junk windows using Title string ONLY
        for (size_t j = 0; j < size_of_list; j++) {
            if (strstr(targetTitle, list_of_not_wanteds[j]) != NULL) {
                not_wanted = true;
                break; 
            }
        }
        if (not_wanted) continue; 

        // 2. SURVIVOR ZONE: This block only hits ~10 times instead of 200 times!
        tracked_windows[saved_count].index = saved_count;
        tracked_windows[saved_count].hwnd = targetHwnd;
        strncpy(tracked_windows[saved_count].title, targetTitle, sizeof(tracked_windows[saved_count].title) - 1);
        tracked_windows[saved_count].title[sizeof(tracked_windows[saved_count].title) - 1] = '\0'; 

        // NOW execute the heavy file path querying selectively
        FetchExeNameOnDemand(targetHwnd, tracked_windows[saved_count].exe, sizeof(tracked_windows[saved_count].exe) - 1);

        // Format for layout
        snprintf(formatted_ui_strings[saved_count], sizeof(formatted_ui_strings[saved_count]), 
                 "%s (%s)", tracked_windows[saved_count].title, tracked_windows[saved_count].exe);
        
        ui_list[saved_count] = formatted_ui_strings[saved_count];
        saved_count++;
        if (saved_count >= 100) break;
    }
    

    printf("[DEBUG] Filter complete. Kept %d windows out of %d.\n", saved_count, myWindows.count);

    if (saved_count == 0) {
        printf("[DEBUG] Exiting: 0 windows passed the filter.\n");
        return NULL;
    }

    for (int i = 0; i < saved_count; i++) {
        ui_list[i] = tracked_windows[i].title;
    }

    printf("[DEBUG] Launching UI Selection Prompt...\n");
    char* selected_title = PromptQuickSwitcher(ui_list, saved_count,220,550);

    if (selected_title == NULL) {
        printf("[DEBUG] UI Closed. Selection was NULL.\n");
        return NULL;
    }

    printf("[DEBUG] UI Returned Selection: \"%s\"\n", selected_title);

    // --- FIX: MATCH AGAINST BOTH TITLE AND EXE NAME ---
    bool found_match = false;
    for (int i = 0; i < saved_count; i++) {
        
        // Check if what you typed matches the Window Title OR the Executable Name string
        if (StrStrIA(tracked_windows[i].title, selected_title) != NULL || 
            StrStrIA(tracked_windows[i].exe, selected_title) != NULL) {
            
            found_match = true;
            HWND target_hwnd = tracked_windows[i].hwnd;
            printf("[DEBUG] Found match at index [%d]: \"%s\" (%s)\n", i, tracked_windows[i].title, tracked_windows[i].exe);

            if (!IsWindow(target_hwnd)) {
                printf("[ERROR] Target HWND is no longer a valid window handle!\n");
                break;
            }

            // Highlight UI before close
            if (hListBox != NULL) {
                int lb_index = SendMessageA(hListBox, LB_FINDSTRINGEXACT, -1, (LPARAM)tracked_windows[i].title);
                if (lb_index != LB_ERR) {
                    SendMessageA(hListBox, LB_SETCURSEL, lb_index, 0);
                }
            }
            Sleep(80); 

            // Thread attaching black magic
            HWND hForegroundWnd = GetForegroundWindow();
            DWORD foregroundThreadID = GetWindowThreadProcessId(hForegroundWnd, NULL);
            DWORD targetThreadID = GetWindowThreadProcessId(target_hwnd, NULL);
            DWORD currentThreadID = GetCurrentThreadId();

            if (foregroundThreadID != currentThreadID) {
                AttachThreadInput(currentThreadID, foregroundThreadID, TRUE);
            }
            if (targetThreadID != currentThreadID) {
                AttachThreadInput(currentThreadID, targetThreadID, TRUE);
            }

            // Handle Minimized State
            if (IsIconic(target_hwnd)) {
                ShowWindow(target_hwnd, SW_RESTORE);
            } else {
                ShowWindow(target_hwnd, SW_SHOW);
            }

            // Force Focus Actions
            SetForegroundWindow(target_hwnd);
            SetFocus(target_hwnd);
            SetActiveWindow(target_hwnd);
            BringWindowToTop(target_hwnd);

            // Cleanup thread states
            if (foregroundThreadID != currentThreadID) {
                AttachThreadInput(currentThreadID, foregroundThreadID, FALSE);
            }
            if (targetThreadID != currentThreadID) {
                AttachThreadInput(currentThreadID, targetThreadID, FALSE);
            }
            
            printf("[DEBUG] --- SWITCHING OPERATION COMPLETE ---\n");
            break;
        }
    }

    if (!found_match) {
        printf("[ERROR] Selected title matched nothing inside our saved struct tracking database!\n");
    }

    return selected_title;
}
int init_tray() {
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

bool CheckAppHotkeyBindings(int pressed_vk, int modifiers) {
    HWND hActiveWnd = GetForegroundWindow();
    if (!hActiveWnd) return false;

    char activeExe[256] = {0};
    FetchExeNameOnDemand(hActiveWnd, activeExe, sizeof(activeExe) - 1);
    
    // Initialize the cache if it's empty
    if (G_APP_COUNT == 0) {
        initialize_app_cache();
    }
    
    for (int i = 0; i < APP_HOTKEY_COUNT; i++) {
        if (G_APP_HOTKEYS[i].vk_code == pressed_vk && 
            G_APP_HOTKEYS[i].modifiers == modifiers) {
            
            if (strcmp(G_APP_HOTKEYS[i].exe_name, "*") == 0 ||
                StrStrIA(activeExe, G_APP_HOTKEYS[i].exe_name) != NULL) {
                
                printf("[APP-HOTKEY] Launching: %s\n", G_APP_HOTKEYS[i].app_name);
                launch_app_by_name(G_APP_HOTKEYS[i].app_name);
                return true;
            }
        }
    }
    return false;
}

bool CheckMacroBindings(int pressed_vk, int modifiers, MacroAction* out_action, int* out_delay) {
    // Get the active window
    HWND hActiveWnd = GetForegroundWindow();
    if (!hActiveWnd) return false;

    char activeExe[256] = {0};
    FetchExeNameOnDemand(hActiveWnd, activeExe, sizeof(activeExe) - 1);
    
    // Debug: Show what was pressed
    printf("[DEBUG] Key: 0x%X (%d), Modifiers: 0x%X, Active exe: '%s'\n", 
           pressed_vk, pressed_vk, modifiers, activeExe);

    // Scan bindings
    for (size_t i = 0; i < MACRO_BINDING_COUNT; i++) {
        if (G_MACRO_BINDINGS[i].vk_code == pressed_vk) {
            // Check if modifiers match
            if (G_MACRO_BINDINGS[i].modifiers != modifiers) {
                continue;
            }
            
            // Check global or app-specific
            if (strcmp(G_MACRO_BINDINGS[i].exe_name, "*") == 0) {
                *out_action = G_MACRO_BINDINGS[i].action_id;
                *out_delay = G_MACRO_BINDINGS[i].delay;
                printf("[DEBUG] GLOBAL binding matched! action=%d, delay=%d\n", *out_action, *out_delay);
                return true;
            }
            
            if (StrStrIA(activeExe, G_MACRO_BINDINGS[i].exe_name) != NULL) {
                *out_action = G_MACRO_BINDINGS[i].action_id;
                *out_delay = G_MACRO_BINDINGS[i].delay;
                printf("[DEBUG] APP-SPECIFIC binding matched! exe='%s' contains '%s', action=%d, delay=%d\n", 
                       activeExe, G_MACRO_BINDINGS[i].exe_name, *out_action, *out_delay);
                return true;
            }
        }
    }
    return false;
}

void TriggerLiveCalculator() {
    if (hPopupWnd != NULL && IsWindow(hPopupWnd)) return;

    g_current_mode = MODE_CALCULATOR;
    const char* default_msg[1] = { "Enter operation (e.g. 5 x 5 or 10 hours in minutes)" };
    PromptQuickSwitcher(default_msg, 1,220,550);
    g_current_mode = MODE_NONE;
}

void TriggerProcessKiller() {
    if (hPopupWnd != NULL && IsWindow(hPopupWnd)) return;

    // Populate our newly created process list cache
    collect_running_processes();
    g_current_mode = MODE_KILLER;

    if (G_PK_COUNT == 0) {
        const char* fallback[1] = { "No targeted processes running..." };
        PromptQuickSwitcher(fallback, 1, 220, 250);
        g_current_mode = MODE_NONE;
        return;
    }

    // Allocate listing array for the UI prompt manager
    const char** list_items = (const char**)malloc(G_PK_COUNT * sizeof(char*));
    if (!list_items) {
        g_current_mode = MODE_NONE;
        return;
    }

    for (int i = 0; i < G_PK_COUNT; i++) {
        list_items[i] = G_PK_LIST[i];
    }

    // Open your UI window with your custom dimensions
    char* selected = PromptQuickSwitcher(list_items, G_PK_COUNT, 220, 250);
    free(list_items);
    
    // If standard Enter was used, close the window and kill the app
    if (selected && strlen(selected) > 0 && g_current_mode == MODE_KILLER) {
        terminate_process_by_name(selected);
    }
    g_current_mode = MODE_NONE;
}

void TriggerAppLauncher() {
    if (hPopupWnd != NULL && IsWindow(hPopupWnd)) return;

    g_current_mode = MODE_LAUNCHER;
    initialize_app_cache();

    if (G_APP_COUNT == 0) {
        g_current_mode = MODE_NONE;
        return;
    }

    // Allocate an array of string pointers for all cached app names
    const char** initial_items = (const char**)malloc(G_APP_COUNT * sizeof(char*));
    if (!initial_items) {
        g_current_mode = MODE_NONE;
        return;
    }
    for (int i = 0; i < G_APP_COUNT; i++) {
        initial_items[i] = G_APPS[i].name;
    }

    // Open the modal pre-seeded with all matching shortcuts
    char* selected_app = PromptQuickSwitcher(initial_items, G_APP_COUNT,220,250); 
    free(initial_items);
    g_current_mode = MODE_NONE; 

    // Handle return action execution
    if (selected_app != NULL && strlen(selected_app) > 0) {
        launch_app_by_name(selected_app);
    }
}

bool CheckTextSnippetBindings(int pressed_vk, int modifiers, const char** out_text) {
    HWND hActiveWnd = GetForegroundWindow();
    if (!hActiveWnd) return false;

    char activeExe[256] = {0};
    FetchExeNameOnDemand(hActiveWnd, activeExe, sizeof(activeExe) - 1);
    
    for (int i = 0; i < TEXT_SNIPPET_COUNT; i++) {
        if (G_TEXT_SNIPPETS[i].vk_code == pressed_vk && 
            G_TEXT_SNIPPETS[i].modifiers == modifiers) {
            
            if (strcmp(G_TEXT_SNIPPETS[i].exe_name, "*") == 0) {
                *out_text = G_TEXT_SNIPPETS[i].text;
                return true;
            }
            
            if (StrStrIA(activeExe, G_TEXT_SNIPPETS[i].exe_name) != NULL) {
                *out_text = G_TEXT_SNIPPETS[i].text;
                return true;
            }
        }
    }
    return false;
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
        // ============================================================
        // KEY REMAPPINGS (Manual)
        // ============================================================

        // CapsLock -> Backspace
        if (pKeyInfo->vkCode == VK_CAPITAL && !shift_down && !ctrl_down && !alt_down) {
            INPUT inputs[2] = {0};
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki.wVk = VK_BACK;
            inputs[1].type = INPUT_KEYBOARD;
            inputs[1].ki.wVk = VK_BACK;
            inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(2, inputs, sizeof(INPUT));
            return 1;
        }

        // Shift+CapsLock -> Escape
        if (pKeyInfo->vkCode == VK_CAPITAL && shift_down && !ctrl_down && !alt_down) {
            // Release Shift first
            INPUT release_shift = {0};
            release_shift.type = INPUT_KEYBOARD;
            release_shift.ki.wVk = VK_SHIFT;
            release_shift.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &release_shift, sizeof(INPUT));
            
            // Send Escape
            INPUT inputs[2] = {0};
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki.wVk = VK_ESCAPE;
            inputs[1].type = INPUT_KEYBOARD;
            inputs[1].ki.wVk = VK_ESCAPE;
            inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(2, inputs, sizeof(INPUT));
            return 1;
        }
        
        if (pKeyInfo->vkCode == VK_RSHIFT && !alt_down && !ctrl_down && !shift_down) {
            INPUT inputs[2] = {0};
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki.wVk = VK_MEDIA_PLAY_PAUSE;
            inputs[1].type = INPUT_KEYBOARD;
            inputs[1].ki.wVk = VK_MEDIA_PLAY_PAUSE;
            inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(2, inputs, sizeof(INPUT));
            return 1;
        }
        // ============================================================
        // KEY REMAPPING: Shift + Right Alt -> Enter
        // ============================================================
        if (pKeyInfo->vkCode == VK_RMENU) {
            // Check if Shift is actually held down
            bool shift_held = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            
            if (shift_held && !ctrl_down) {
                // Release Shift and Right Alt, then press Enter
                INPUT inputs[6] = {0};
                int idx = 0;
                
                // Release Shift
                inputs[idx].type = INPUT_KEYBOARD;
                inputs[idx].ki.wVk = VK_SHIFT;
                inputs[idx].ki.dwFlags = KEYEVENTF_KEYUP;
                idx++;
                
                // Release Right Alt
                inputs[idx].type = INPUT_KEYBOARD;
                inputs[idx].ki.wVk = VK_RMENU;
                inputs[idx].ki.dwFlags = KEYEVENTF_KEYUP;
                idx++;
                
                // Press Enter
                inputs[idx].type = INPUT_KEYBOARD;
                inputs[idx].ki.wVk = VK_RETURN;
                idx++;
                
                // Release Enter
                inputs[idx].type = INPUT_KEYBOARD;
                inputs[idx].ki.wVk = VK_RETURN;
                inputs[idx].ki.dwFlags = KEYEVENTF_KEYUP;
                idx++;
                
                SendInput(idx, inputs, sizeof(INPUT));
                return 1;
            }
        }
        if (pKeyInfo->vkCode == 'Q' && ctrl_down && !alt_down && !shift_down) {
            HWND hForeground = GetForegroundWindow();
            if (hForeground != NULL) {
                DWORD processId = 0;
                GetWindowThreadProcessId(hForeground, &processId);
                if (processId != 0) {
                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
                    if (hProcess != NULL) {
                        TerminateProcess(hProcess, 0);
                        CloseHandle(hProcess);
                        printf("[END TASK] Terminated process ID: %lu\n", processId);
                    }
                }
            }
            return 1;
        }
        // ============================================================
        // KEY REMAPPING: Ctrl+Alt+WASD -> Arrow Keys (scancode)
        // ============================================================
        if ((pKeyInfo->vkCode == 'W' || pKeyInfo->vkCode == 'A' || 
             pKeyInfo->vkCode == 'S' || pKeyInfo->vkCode == 'D') && 
            ctrl_down && alt_down && !shift_down) {
            
            // Map virtual keys to scancodes
            int arrow_scancode = 0;
            switch (pKeyInfo->vkCode) {
                case 'W': arrow_scancode = 0x48; break; // Up
                case 'A': arrow_scancode = 0x4B; break; // Left
                case 'S': arrow_scancode = 0x50; break; // Down
                case 'D': arrow_scancode = 0x4D; break; // Right
            }
            
            INPUT inputs[4] = {0};
            int idx = 0;
            
            // Release Ctrl
            inputs[idx].type = INPUT_KEYBOARD;
            inputs[idx].ki.wVk = VK_CONTROL;
            inputs[idx].ki.dwFlags = KEYEVENTF_KEYUP;
            idx++;
            
            // Release Alt
            inputs[idx].type = INPUT_KEYBOARD;
            inputs[idx].ki.wVk = VK_MENU;
            inputs[idx].ki.dwFlags = KEYEVENTF_KEYUP;
            idx++;
            
            // Arrow down (using scancode)
            inputs[idx].type = INPUT_KEYBOARD;
            inputs[idx].ki.wScan = arrow_scancode;
            inputs[idx].ki.dwFlags = KEYEVENTF_SCANCODE;
            idx++;
            
            // Arrow up
            inputs[idx].type = INPUT_KEYBOARD;
            inputs[idx].ki.wScan = arrow_scancode;
            inputs[idx].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
            idx++;
            
            SendInput(idx, inputs, sizeof(INPUT));
            return 1;
        }
        
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

        // ============================================================
        // SWITCHER KEY (Alt + Space)
        // ============================================================
        if (pKeyInfo->vkCode == SWITCHER_KEY) {
            bool is_blacklisted = false;
            for (int i = 0; i < switcher_blacklist_count; i++) {
                if (StrStrIA(fgTitle, switcher_blacklist[i]) != NULL) {
                    is_blacklisted = true;
                    break;
                }
            }
            if (is_blacklisted) {
                return CallNextHookEx(NULL, nCode, wParam, lParam);
            }

            if (alt_down && !ctrl_down && !shift_down) {
                if (hPopupWnd != NULL && IsWindow(hPopupWnd)) {
                    return 1;
                }

                WindowList myWindows;
                myWindows.count = 0;
                myWindows.capacity = 10;
                myWindows.items = (WindowData*)malloc(myWindows.capacity * sizeof(WindowData));

                if (myWindows.items != NULL) {
                    EnumWindows(EnumWindowsProc, (LPARAM)&myWindows);
                    FilterAndPromptSwitcher(myWindows);
                    free(myWindows.items);
                }
                return 1;
            }
        }
        if (pKeyInfo->vkCode == GOOGLE_SEARCH_KEY) {
            bool is_blacklisted = false;
            for (int i = 0; i < google_search_blacklist_count; i++) {
                if (StrStrIA(fgTitle, google_search_blacklist[i]) != NULL) {
                    is_blacklisted = true;
                    break;
                }
            }
            if (is_blacklisted) {
                return CallNextHookEx(NULL, nCode, wParam, lParam);
            }

            if (alt_down && !ctrl_down && !shift_down) {
                if (hPopupWnd != NULL && IsWindow(hPopupWnd)) {
                    return 1;
                }
                printf("[GOOGLE] Opening Google Search...\n");
                TriggerGoogleSearch();
                return 1;
            }
        }

        // ============================================================
        // TODO KEY (Alt + T)
        // ============================================================
        if (pKeyInfo->vkCode == TODO_KEY) {
            bool is_blacklisted = false;
            for (int i = 0; i < todo_blacklist_count; i++) {
                if (StrStrIA(fgTitle, todo_blacklist[i]) != NULL) {
                    is_blacklisted = true;
                    break;
                }
            }
            if (is_blacklisted) {
                return CallNextHookEx(NULL, nCode, wParam, lParam);
            }

            if (alt_down && !ctrl_down && !shift_down) {
                printf("[TODO] Opening Todo overlay...\n");
                TriggerTodo(hPopupWnd, (int*)&g_current_mode);
                return 1;
            }
        }

        if (pKeyInfo->vkCode == POS_KEY) {
            bool is_blacklisted = false;
            for (int i = 0; i < pos_blacklist_count; i++) {
                if (StrStrIA(fgTitle, pos_blacklist[i]) != NULL) {
                    is_blacklisted = true;
                    break;
                }
            }
            if (is_blacklisted) {
                return CallNextHookEx(NULL, nCode, wParam, lParam);
            }

            if (HandlePosKey(alt_down, ctrl_down, shift_down)) {
                return 1;
            }
        }

        if (pKeyInfo->vkCode == COLOR_KEY) {
            bool is_blacklisted = false;
            for (int i = 0; i < color_blacklist_count; i++) {
                if (StrStrIA(fgTitle, color_blacklist[i]) != NULL) {
                    is_blacklisted = true;
                    break;
                }
            }
            if (is_blacklisted) {
                return CallNextHookEx(NULL, nCode, wParam, lParam);
            }

            if (HandleColorKey(alt_down, ctrl_down, shift_down)) {
                return 1;
            }
        }

        if (pKeyInfo->vkCode == PIXEL_EQUALS_KEY) {
            bool is_blacklisted = false;
            for (int i = 0; i < pixel_equals_blacklist_count; i++) {
                if (StrStrIA(fgTitle, pixel_equals_blacklist[i]) != NULL) {
                    is_blacklisted = true;
                    break;
                }
            }
            if (is_blacklisted) {
                return CallNextHookEx(NULL, nCode, wParam, lParam);
            }

            if (HandlePixelEqualsKey(alt_down, ctrl_down, shift_down)) {
                return 1;
            }
        }

        if (pKeyInfo->vkCode == WAIT_FOR_PIXEL_KEY) {
            bool is_blacklisted = false;
            for (int i = 0; i < wait_for_pixel_blacklist_count; i++) {
                if (StrStrIA(fgTitle, wait_for_pixel_blacklist[i]) != NULL) {
                    is_blacklisted = true;
                    break;
                }
            }
            if (is_blacklisted) {
                return CallNextHookEx(NULL, nCode, wParam, lParam);
            }

            if (HandleWaitForPixelKey(alt_down, ctrl_down, shift_down)) {
                return 1;
            }
        }

        if (pKeyInfo->vkCode == REGION_KEY) {
            bool is_blacklisted = false;
            for (int i = 0; i < region_blacklist_count; i++) {
                if (StrStrIA(fgTitle, region_blacklist[i]) != NULL) {
                    is_blacklisted = true;
                    break;
                }
            }
            if (is_blacklisted) {
                return CallNextHookEx(NULL, nCode, wParam, lParam);
            }

            if (HandleRegionKey(alt_down, ctrl_down, shift_down)) {
                return 1;
            }
        }
        // ============================================================
        // CALCULATOR_KEY (C) - Launch Calculator
        // ============================================================
        if (pKeyInfo->vkCode == CALCULATOR_KEY) {
            bool is_blacklisted = false;
            for (int i = 0; i < calculator_blacklist_count; i++) {
                if (StrStrIA(fgTitle, calculator_blacklist[i]) != NULL) {
                    is_blacklisted = true;
                    break;
                }
            }
            if (is_blacklisted) {
                return CallNextHookEx(NULL, nCode, wParam, lParam);
            }
            
            if (alt_down && !ctrl_down && !shift_down) {
                if (hPopupWnd != NULL && IsWindow(hPopupWnd)) {
                    return 1; 
                }
                printf("[CALCULATOR] Launching calculator...\n");
                TriggerLiveCalculator(); 
                return 1;
            }
        }

        // ============================================================
        // EXE_KEY (E) - Launch App Launcher
        // ============================================================
        if (pKeyInfo->vkCode == EXE_KEY) {
            bool is_blacklisted = false;
            for (int i = 0; i < exe_blacklist_count; i++) {
                if (StrStrIA(fgTitle, exe_blacklist[i]) != NULL) {
                    is_blacklisted = true;
                    break;
                }
            }
            if (is_blacklisted) {
                return CallNextHookEx(NULL, nCode, wParam, lParam);
            }

            if (alt_down && !ctrl_down && !shift_down) {
                if (hPopupWnd != NULL && IsWindow(hPopupWnd)) {
                    return 1; 
                }
                printf("[EXE] Launching app launcher...\n");
                TriggerAppLauncher();
                return 1;
            }
        }

        // ============================================================
        // KILL_PROCESS_KEY (K) - Kill Process
        // ============================================================
        if (pKeyInfo->vkCode == KILL_PROCESS_KEY) {
            bool is_blacklisted = false;
            for (int i = 0; i < kill_process_blacklist_count; i++) {
                if (StrStrIA(fgTitle, kill_process_blacklist[i]) != NULL) {
                    is_blacklisted = true;
                    break;
                }
            }
            if (is_blacklisted) {
                return CallNextHookEx(NULL, nCode, wParam, lParam);
            }

            if (alt_down && !ctrl_down && !shift_down) {
                if (hPopupWnd != NULL && IsWindow(hPopupWnd)) return 1;
                printf("[KILL] Triggering process killer...\n");
                TriggerProcessKiller();
                return 1;
            }
        }
        if (pKeyInfo->vkCode == NVIM_KEY) {
            bool is_blacklisted = false;
            for (int i = 0; i < nvim_blacklist_count; i++) {
                if (StrStrIA(fgTitle, nvim_blacklist[i]) != NULL) {
                    is_blacklisted = true;
                    break;
                }
            }
            if (is_blacklisted) {
                return CallNextHookEx(NULL, nCode, wParam, lParam);
            }

            if (alt_down && !ctrl_down && !shift_down) {
                if (hPopupWnd != NULL && IsWindow(hPopupWnd)) {
                    return 1;
                }
                printf("[NVIM] Opening nvim with zoxide...\n");
                TriggerNvimZoxide();
                return 1;
            }
        }

        // ============================================================
        // UNIVERSAL HOTKEY BINDINGS CHECK (NEW SYSTEM WITH MODIFIERS)
        // ============================================================
        int pressed_vk = (int)pKeyInfo->vkCode;
        
        // Build modifier mask from current key states
        int modifiers = HOTKEY_MOD_NONE;
        if (alt_down) modifiers |= HOTKEY_MOD_ALT;
        if (shift_down) modifiers |= HOTKEY_MOD_SHIFT;
        if (ctrl_down) modifiers |= HOTKEY_MOD_CTRL;
        
        MacroAction matchedAction = MACRO_NONE;
        int matchedDelay = 0;
        
        // Debug: Show what was pressed
        printf("[DEBUG] Checking universal binding: VK=0x%X, Modifiers=0x%X, Exe='%s'\n", 
               pressed_vk, modifiers, fgTitle);
        const char* snippet_text = NULL;
        if (CheckTextSnippetBindings(pressed_vk, modifiers, &snippet_text)) {
            if (snippet_text != NULL) {
                printf("[SNIPPET] Copying to clipboard: \"%s\"\n", snippet_text);
                CopyToClipboard(snippet_text);
                return 1; // Swallow the keypress
            }
        }
        
        if (CheckMacroBindings(pressed_vk, modifiers, &matchedAction, &matchedDelay)) {
            if (matchedAction != MACRO_NONE && hGlobalTrayWnd != NULL) {
                printf("[MACRO] ✅ Triggering action %d with delay %d\n", matchedAction, matchedDelay);
                PostMessage(hGlobalTrayWnd, WM_TRIGGER_UNIVERSAL_MACRO, (WPARAM)matchedAction, (LPARAM)matchedDelay);
                return 1; // Swallow the keypress
            }
        }
        if (CheckAppHotkeyBindings(pressed_vk, modifiers)) {
            return 1;
        }
        
        if (HandleFocusSwitcher(pKeyInfo->vkCode, modifiers, fgTitle)) {
            return 1;
        }
            }
    
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void wait_for_hotkeys() {
    HINSTANCE hInstance = GetModuleHandle(NULL);
    
    // Install keyboard hook
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
    if (hMouseHook) UnhookWindowsHookEx(hMouseHook);
}

int main() {
    StartUniversalTimer();
  
    init_tray();
    struct GdiplusStartupInput gdiplusStartupInput;
    gdiplusStartupInput.GdiplusVersion = 1;
    gdiplusStartupInput.DebugEventCallback = NULL;
    gdiplusStartupInput.SuppressBackgroundThread = FALSE;
    gdiplusStartupInput.SuppressExternalCodecs = FALSE;
    GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);

    // --- INSTRUMENTED SYSTEM REGISTRATION BLOCK ---
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
    // ----------------------------------------------

    printf("[SYSTEM] Low-Level Hotkey Services Running. GDI+ Online.\n");

    wait_for_hotkeys();

    if (g_gdiplusToken != 0) {
        GdiplusShutdown(g_gdiplusToken);
    }
    return 0;
}
