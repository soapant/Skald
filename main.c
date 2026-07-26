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
#include "video_player.h"
#include "music_player.h"
#include "bindings.h"
#include "focus_switcher.h"
#include "process_killer.h"
#include "number_shortcut.h"
#include "helpers.h"
#include "common_ui.h"
#include "hotkey_definitions.h"
#include "modal.h"
#include "subclasses.h"
#pragma comment(lib, "Shlwapi.lib") 
#define ID_TRAY_ICON 1001
#define WM_TRAYICON (WM_USER + 1)
#define WM_TRIGGER_UNIVERSAL_MACRO (WM_USER + 3)
#define ID_EXIT 2001
#define IDC_INPUT_BOX 101
#define IDC_LIST_BOX 102

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
static  ULONG_PTR g_gdiplusToken = 0;
#pragma comment(lib, "gdiplus.lib")
RefreshCallback g_refresh_callback = NULL;
EnterCallback g_enter_callback = NULL;
CtrlEnterCallback g_ctrl_enter_callback = NULL;
TabCallback g_tab_callback = NULL;
RenderCallback g_render_callback = NULL;
char* g_current_feature = NULL;
int g_timeout = 1;
const char** g_initial_items = NULL;
int g_initial_count = 0;
int g_SelectedIndex = 0; 
int g_TotalMatches = 0; 
int g_last_match_count = 0;
const char** g_last_items = NULL;
int g_last_item_count = 0;
char g_last_filter[256] = {0};
int g_last_matches[4000];
char g_filter_history[20][256];
int g_match_history[20][4000]; 
int g_match_count_history[20]; 
int g_history_index = 0;       
int g_history_count = 0;     
int g_current_window_height = 0;
int g_current_window_width = 0;
#define MAX_ITEMS 1000
char g_populated_cache[MAX_ITEMS][512];
int g_populated_count = 0;
int g_view_offset = 0;
const char* g_title = NULL;
int img_region_step = 0;
long img_x1 = 0, img_y1 = 0;
HFONT g_hNumFont = NULL;
HFONT g_hMainFont = NULL;
HashMap* feature_history = NULL;
HashMap* feature_data = NULL;

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
    g_current_window_height = height;
    g_current_window_width = width;
    g_current_feature = (char*)window_title + 4;
    struct feature_data* data = GetFeatureData(g_current_feature);
    if (data) {
        data->name = _strdup(g_current_feature);
        data->items = window_list;
        data->item_count = count;
        data->refresh_callback = refresh_callback;
        data->enter_callback = enter_callback;
        data->ctrl_enter_callback = ctrl_enter_callback;
        data->tab_callback = tab_callback;
        data->render_callback = render_callback;
        data->filter_match = NULL;
        
        printf("[REGISTRY] Registered feature: %s (%d items)\n", data->name, data->item_count);
    }
    
    SetMode(MODE_INSERT);

    WindowLayout layout;
    CalculateWindowMetrics(count, &layout, height, width, item_height);
    g_current_window_height = layout.h;
    g_current_window_width = layout.w;
    
    HFONT hCustomFont = CreateCustomNerdFont(24, UsedFont);

    hPopupWnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_COMPOSITED,
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

    CreateChildControls(hPopupWnd, hCustomFont, window_list, count, &layout);
    
    SetLayeredWindowAttributes(hPopupWnd, 0, 245, LWA_ALPHA);

    ForceWindowFocus(hPopupWnd, hEditControl);


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
    g_current_window_height = height;
    g_current_window_width = width;
    g_current_feature = (char*)window_title + 4;
    
    SetMode(MODE_INSERT);

    WindowLayout layout;
    CalculateWindowMetrics(1, &layout, height, width, item_height);
    g_current_window_height = layout.h;
    g_current_window_width = layout.w;
    
    HFONT hCustomFont = CreateCustomNerdFont(24, UsedFont);

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

char* DisplayMessage(const char* message, int centerX, int centerY, int width, int height, int timeout) {
    HINSTANCE hInstance = GetModuleHandle(NULL);
    
    FadeWindowData* pWndData = (FadeWindowData*)malloc(sizeof(FadeWindowData));
    if (!pWndData) return NULL;

    pWndData->message = _strdup(message ? message : "");
    pWndData->hFont = CreateCustomNerdFont(20, UsedFont);
    pWndData->alpha = 245;
    g_timeout = timeout * 20;

    int topLeftX = centerX - (width / 2);
    int topLeftY = centerY - (height / 2);

    HWND hPopupWnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE, 
        "DisplayMessageModalClass", NULL, WS_POPUP, 
        topLeftX, topLeftY, width, height, NULL, NULL, hInstance, pWndData
    );

    if (hPopupWnd == NULL) {
        printf("[DEBUG-UI] ERROR: CreateWindowExA failed. GetLastError: %lu\n", GetLastError());
        DeleteObject(pWndData->hFont);
        free(pWndData);
        return NULL;
    }

    SetLayeredWindowAttributes(hPopupWnd, 0, pWndData->alpha, LWA_ALPHA);
    
    ShowWindow(hPopupWnd, SW_SHOWNOACTIVATE);
    UpdateWindow(hPopupWnd);

    SetTimer(hPopupWnd, 999, 50, NULL);
    
    return NULL; 
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
        
        if (CheckAppHotkeyBindings(pressed_vk, modifiers)) {
            return 1;
        }

        if (CheckMiscHotkeyBindings(pressed_vk, modifiers)) {
            return 1;
        }

        if (CheckQuickLinkHotkeyBindings(pressed_vk, modifiers)) {
            return 1;
        }

        if (HandleFocusSwitcher(pressed_vk, modifiers, fgTitle)) {
            return 1;
        }
        if (HandleMusicPlayer(pressed_vk, modifiers, fgTitle)) {
            return 1;
        }

        if (HandleVideoPlayer(pressed_vk, modifiers, fgTitle)) {
            return 1;
        }

        if (HandleBulkKillProcess(pressed_vk, modifiers, fgTitle)) {
            return 1;
        }
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
    initialize_app_cache();
    feature_history = HashMap_Create(32);
    feature_data = HashMap_Create(32);
    
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
    // NEW: Register your dedicated standalone notification class
    WNDCLASSA wcFade = {0};
    wcFade.lpfnWndProc = DisplayMessageProc;
    wcFade.hInstance = GetModuleHandle(NULL);
    wcFade.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcFade.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1); 
    wcFade.lpszClassName = "DisplayMessageModalClass";

    if (RegisterClassA(&wcFade)) {
        printf("[SYSTEM-BOOT] DisplayMessageClass registered successfully with OS.\n");
    } else {
        printf("[SYSTEM-BOOT] WARNING: Failed to register FadeNotificationClass! Error: %lu\n", GetLastError());
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
