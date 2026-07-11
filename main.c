#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <commctrl.h>
#include <psapi.h>
#include <gdiplus.h>
#include <stdio.h>
#include <shellapi.h>
#include <stdbool.h>
#include <shlwapi.h> 
#include "calc.h"
#include "window_switcher.h"
#include "types.h"
#include "video_player.h"
#include "music_player.h"
#include "timer.h"
#include "bindings.h"
#include "explorer.h"
#include "screenshot.h"
#include "terminal_command.h"
#include "todo.h"
#include "misc_launcher.h"
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
#include "app_launcher.h"
#include "process_killer.h"
#include "config.h"
#pragma comment(lib, "Shlwapi.lib") // Tells the compiler to link the shlwapi library
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
#pragma comment(lib, "gdiplus.lib")

static RefreshCallback g_refresh_callback = NULL;
static EnterCallback g_enter_callback = NULL;
static CtrlEnterCallback g_ctrl_enter_callback = NULL;
static TabCallback g_tab_callback = NULL;
static const char** g_initial_items = NULL;
static int g_initial_count = 0;
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
            } else {
                GetWindowTextA(hwnd, output_buffer, sizeof(output_buffer));
            }

            if (ctrl_pressed) {
                if (g_ctrl_enter_callback != NULL) {
                    g_ctrl_enter_callback(output_buffer);
                }
                // DON'T close the window on Ctrl+Enter!
                // Keep selection_complete = false so the window stays open
                // Refresh the list if needed
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

char* PromptUser(const char* window_list[], int count, int height, int width, 
                          RefreshCallback refresh_callback, 
                          EnterCallback enter_callback,
                          CtrlEnterCallback ctrl_enter_callback,
                          TabCallback tab_callback) {
    HINSTANCE hInstance = GetModuleHandle(NULL);
    selection_complete = false;
    output_buffer[0] = '\0';
    g_initial_items = window_list;
    g_initial_count = count;
    g_refresh_callback = refresh_callback;
    g_enter_callback = enter_callback;
    g_ctrl_enter_callback = ctrl_enter_callback;
    g_tab_callback = tab_callback;

    printf("[DEBUG-UI] PromptUser called. Count: %d\n", count);

    WindowLayout layout;
    CalculateWindowMetrics(count, &layout, height, width);
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
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            loop_counter++;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            Sleep(10);
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
    g_refresh_callback = NULL;
    g_enter_callback = NULL;
    g_ctrl_enter_callback = NULL;
    g_tab_callback = NULL;

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
            char* raw_name = PromptUser(empty_list, 0, 220, 550,NULL,NULL,NULL,NULL);
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
                    TriggerWindowSwitcher(hPopupWnd,myWindows);
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
                TriggerTodo(hPopupWnd);
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
                TriggerCalculator(hPopupWnd); 
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
                TriggerAppLauncher(hPopupWnd);
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
                TriggerProcessKiller(hPopupWnd);
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
                printf("[NEOVIDE] Opening neovide with zoxide...\n");
                TriggerNvimZoxide(0,hPopupWnd);
                return 1;
            }
        }
        
        if (pKeyInfo->vkCode == YAZI_KEY) {
            bool is_blacklisted = false;
            for (int i = 0; i < yazi_blacklist_count; i++) {
                if (StrStrIA(fgTitle, yazi_blacklist[i]) != NULL) {
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
                printf("[YAZI] Opening yazi with zoxide...\n");
                TriggerYaziZoxide(hPopupWnd);
                return 1;
            }
        }
        if (pKeyInfo->vkCode == TERMINAL_KEY) {
            bool is_blacklisted = false;
            for (int i = 0; i < terminal_blacklist_count; i++) {
                if (StrStrIA(fgTitle, terminal_blacklist[i]) != NULL) {
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
                printf("[terminal] Opening terminal with zoxide...\n");
                TriggerZoxide(hPopupWnd);
                return 1;
            }
        }
        if (pKeyInfo->vkCode == TIMER_KEY) {
            bool is_blacklisted = false;
            for (int i = 0; i < timer_blacklist_count; i++) {
                if (StrStrIA(fgTitle, timer_blacklist[i]) != NULL) {
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
                TriggerTimer(hPopupWnd);
                return 1;
            }
        }
        if (pKeyInfo->vkCode == EXPLORER_KEY) {
            bool is_blacklisted = false;
            for (int i = 0; i < explorer_blacklist_count; i++) {
                if (StrStrIA(fgTitle, explorer_blacklist[i]) != NULL) {
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
                TriggerExplorer(hPopupWnd);
                return 1;
            }
        }
        if (pKeyInfo->vkCode == TERMINAL_COMMAND_KEY) {
            bool is_blacklisted = false;
            for (int i = 0; i < terminal_command_blacklist_count; i++) {
                if (StrStrIA(fgTitle, terminal_command_blacklist[i]) != NULL) {
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
                TriggerTerminalCommand(hPopupWnd);
                return 1;
            }
        }
        if (pKeyInfo->vkCode == SCREENSHOT_KEY) {
            bool is_blacklisted = false;
            for (int i = 0; i < screenshot_blacklist_count; i++) {
                if (StrStrIA(fgTitle, screenshot_blacklist[i]) != NULL) {
                    is_blacklisted = true;
                    break;
                }
            }
            if (is_blacklisted) {
                return CallNextHookEx(NULL, nCode, wParam, lParam);
            }

            if (alt_down && !shift_down && !ctrl_down) {
                POINT cursorPos;
                if (!GetCursorPos(&cursorPos)) return 1;

                if (img_region_step == 0) {
                    img_x1 = cursorPos.x;
                    img_y1 = cursorPos.y;
                    printf("[SCREENSHOT] Top-Left: %ld,%ld\n", img_x1, img_y1);
                    img_region_step = 1;
                } else if (img_region_step == 1) {
                    img_x2 = cursorPos.x;
                    img_y2 = cursorPos.y;
                    printf("[SCREENSHOT] Bottom-Right: %ld,%ld\n", img_x2, img_y2);
                    img_region_step = 0;

                    TriggerScreenshot(hPopupWnd);
                }
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
        
        if (CheckMiscHotkeyBindings(pressed_vk, modifiers)) {
            return 1;
        }
        
        if (HandleFocusSwitcher(pKeyInfo->vkCode, modifiers, fgTitle)) {
            return 1;
        }
        if (HandleMusicPlayer(pKeyInfo->vkCode, modifiers, fgTitle)) {
            return 1;
        }
        if (HandleVideoPlayer(pKeyInfo->vkCode, modifiers, fgTitle)) {
            return 1;
        }
        if (HandleBulkKillProcess(pKeyInfo->vkCode, modifiers, fgTitle)) {
            return 1;
        }
    }
    
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void RunHotKeyMessageLoop() {
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

    RunHotKeyMessageLoop();

    if (g_gdiplusToken != 0) {
        GdiplusShutdown(g_gdiplusToken);
    }
    return 0;
}
