// hotkey_definitions.h
#ifndef HOTKEY_DEFINITIONS_H
#define HOTKEY_DEFINITIONS_H

#include "helpers.h"
#include "config.h"

// ============================================================
// FORWARD DECLARATIONS
// ============================================================
extern BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
extern HWND hPopupWnd;
extern HWND hGlobalTrayWnd;

// Forward declarations of all trigger functions
void TriggerGoogleSearch(void);
void TriggerTodo(HWND hPopupWnd);
void TriggerCalculator(HWND hPopupWnd);
void TriggerAppLauncher(HWND hPopupWnd);
void TriggerProcessKiller(HWND hPopupWnd);
void TriggerTimer(HWND hPopupWnd);
void TriggerVolume(HWND hPopupWnd);
void TriggerTerminalCommand(HWND hPopupWnd);
void TriggerExplorer(HWND hPopupWnd);
void TriggerZoxide(HWND hPopupWnd);
void TriggerNvimZoxide(int version, HWND hPopupWnd);
void TriggerYaziZoxide(HWND hPopupWnd);
char* TriggerWindowSwitcher(HWND hPopupWnd, WindowList myWindows);

// ============================================================
// HANDLER WRAPPERS
// ============================================================

// --- Switcher ---
static inline void SwitcherHandler(void) {
    WindowList myWindows;
    myWindows.count = 0;
    myWindows.capacity = 10;
    myWindows.items = (WindowData*)malloc(myWindows.capacity * sizeof(WindowData));
    if (myWindows.items != NULL) {
        EnumWindows(EnumWindowsProc, (LPARAM)&myWindows);
        TriggerWindowSwitcher(hPopupWnd, myWindows);
        free(myWindows.items);
    }
}

// --- Google ---
static inline void GoogleHandler(void) { TriggerGoogleSearch(); }

// --- Todo ---
static inline void TodoHandler(void) { TriggerTodo(hPopupWnd); }

// --- Calculator ---
static inline void CalculatorHandler(void) { TriggerCalculator(hPopupWnd); }

// --- App Launcher ---
static inline void AppLauncherHandler(void) { TriggerAppLauncher(hPopupWnd); }

// --- Process Killer ---
static inline void ProcessKillerHandler(void) { TriggerProcessKiller(hPopupWnd); }

// --- Timer ---
static inline void TimerHandler(void) { TriggerTimer(hPopupWnd); }

// --- Terminal Command ---
static inline void TerminalCommandHandler(void) { TriggerTerminalCommand(hPopupWnd); }

// --- Explorer ---
static inline void ExplorerHandler(void) { TriggerExplorer(hPopupWnd); }

// --- Terminal ---
static inline void TerminalHandler(void) { TriggerZoxide(hPopupWnd); }

// --- Nvim (Alt+Ctrl+N) ---
static inline void NvimHandler(void) { TriggerNvimZoxide(1, hPopupWnd); }

// --- Neovide (Alt+N) ---
static inline void NeovideHandler(void) { TriggerNvimZoxide(0, hPopupWnd); }

// --- Yazi ---
static inline void YaziHandler(void) { TriggerYaziZoxide(hPopupWnd); }

// --- Volume ---
static inline void VolumeHandler(void) { TriggerVolume(hPopupWnd); }

// --- POS Handlers ---
static inline void PosCopyHandler(void) {
    POINT mousePos;
    if (GetCursorPos(&mousePos)) {
        char coords_buf[64];
        snprintf(coords_buf, sizeof(coords_buf), "%ld, %ld", mousePos.x, mousePos.y);
        CopyToClipboard(coords_buf);
    }
}

static inline void PosAddHandler(void) {
    POINT mousePos;
    if (GetCursorPos(&mousePos)) {
        char coords_buf[64];
        snprintf(coords_buf, sizeof(coords_buf), "(%ld, %ld)", mousePos.x, mousePos.y);
        char temp_lines[100][512];
        int temp_count = ReadMdFile(temp_lines, TEMP_FILE_PATH);
        if (temp_count < 100) {
            snprintf(temp_lines[temp_count], sizeof(temp_lines[temp_count]), "- [ ] %s", coords_buf);
            WriteMdFile(temp_lines, temp_count + 1, TEMP_FILE_PATH);
        }
    }
}

static inline void PosClearHandler(void) {
    ClearMdFile(TEMP_FILE_PATH);
}

// --- Color Handlers ---
static inline void ColorPlainHandler(void) {
    long x, y; BYTE r, g, b;
    if (GetCursorPixel(&x, &y, &r, &g, &b)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%d, %d, %d", r, g, b);
        CopyToClipboard(buf);
    }
}

static inline void ColorParenHandler(void) {
    long x, y; BYTE r, g, b;
    if (GetCursorPixel(&x, &y, &r, &g, &b)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "(%d, %d, %d)", r, g, b);
        CopyToClipboard(buf);
    }
}

// --- Pixel Equals Handlers ---
static inline void PixelEqualsAdbHandler(void) {
    long x, y; BYTE r, g, b;
    if (GetCursorPixel(&x, &y, &r, &g, &b)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "adb.pixel_equals(%ld,%ld,(%d,%d,%d))", x, y, r, g, b);
        CopyToClipboard(buf);
    }
}

static inline void PixelEqualsFnHandler(void) {
    long x, y; BYTE r, g, b;
    if (GetCursorPixel(&x, &y, &r, &g, &b)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "fn.pixel_equals(%ld,%ld,(%d,%d,%d))", x, y, r, g, b);
        CopyToClipboard(buf);
    }
}

// --- Wait For Pixel Handlers ---
static inline void WaitForPixelAdbHandler(void) {
    long x, y; BYTE r, g, b;
    if (GetCursorPixel(&x, &y, &r, &g, &b)) {
        char buf[256];
        snprintf(buf, sizeof(buf), "adb.wait_for_pixel(%ld,%ld,(%d,%d,%d),tolerance=0)", x, y, r, g, b);
        CopyToClipboard(buf);
    }
}

static inline void WaitForPixelTapHandler(void) {
    long x, y; BYTE r, g, b;
    if (GetCursorPixel(&x, &y, &r, &g, &b)) {
        char buf[256];
        snprintf(buf, sizeof(buf), "adb.wait_for_pixel(%ld,%ld,(%d,%d,%d),tolerance=0)\nadb.tap(%ld,%ld)", 
                 x, y, r, g, b, x, y);
        CopyToClipboard(buf);
    }
}

static inline void WaitForPixelFnHandler(void) {
    long x, y; BYTE r, g, b;
    if (GetCursorPixel(&x, &y, &r, &g, &b)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "wait_for_pixel(%ld,%ld,%d,%d,%d)", x, y, r, g, b);
        CopyToClipboard(buf);
    }
}

// --- Region Handlers ---
static inline void RegionCoordsHandler(void) {
    POINT cursorPos;
    if (!GetCursorPos(&cursorPos)) return;
    
    static long saved_x1 = 0, saved_y1 = 0;
    static int region_step = 0;
    
    if (region_step == 0) {
        saved_x1 = cursorPos.x;
        saved_y1 = cursorPos.y;
        region_step = 1;
        printf("[REGION] Step 1: %ld,%ld\n", saved_x1, saved_y1);
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "%ld,%ld,%ld,%ld", saved_x1, saved_y1, cursorPos.x, cursorPos.y);
        CopyToClipboard(buf);
        region_step = 0;
        printf("[REGION] Complete: %s\n", buf);
    }
}

static inline void RegionImageHandler(void) {
    POINT cursorPos;
    if (!GetCursorPos(&cursorPos)) return;
    
    extern int img_region_step;
    extern long img_x1, img_y1, img_x2, img_y2;
    
    if (img_region_step == 0) {
        img_x1 = cursorPos.x;
        img_y1 = cursorPos.y;
        img_region_step = 1;
        printf("[IMG REGION] Top-Left: %ld,%ld\n", img_x1, img_y1);
    } else {
        img_x2 = cursorPos.x;
        img_y2 = cursorPos.y;
        img_region_step = 0;
        printf("[IMG REGION] Bottom-Right: %ld,%ld\n", img_x2, img_y2);
        if (hGlobalTrayWnd != NULL) {
            SendNotifyMessage(hGlobalTrayWnd, WM_TRIGGER_IMG_CAPTURE, 0, 0);
        }
    }
}

// --- Screenshot Handlers ---
static inline void ScreenshotSaveHandler(void) {
    POINT cursorPos;
    if (!GetCursorPos(&cursorPos)) return;
    
    extern int img_region_step;
    extern long img_x1, img_y1, img_x2, img_y2;
    
    if (img_region_step == 0) {
        img_x1 = cursorPos.x;
        img_y1 = cursorPos.y;
        img_region_step = 1;
        printf("[SCREENSHOT] Top-Left: %ld,%ld\n", img_x1, img_y1);
    } else {
        img_x2 = cursorPos.x;
        img_y2 = cursorPos.y;
        img_region_step = 0;
        TriggerScreenshot(hPopupWnd, false);
    }
}

static inline void ScreenshotOpenHandler(void) {
    POINT cursorPos;
    if (!GetCursorPos(&cursorPos)) return;
    
    extern int img_region_step;
    extern long img_x1, img_y1, img_x2, img_y2;
    
    if (img_region_step == 0) {
        img_x1 = cursorPos.x;
        img_y1 = cursorPos.y;
        img_region_step = 1;
        printf("[SCREENSHOT] Top-Left: %ld,%ld\n", img_x1, img_y1);
    } else {
        img_x2 = cursorPos.x;
        img_y2 = cursorPos.y;
        img_region_step = 0;
        TriggerScreenshot(hPopupWnd, true);
    }
}

// ============================================================
// THE MASTER HOTKEY BINDINGS TABLE
// ============================================================
static const HotkeyBinding G_HOTKEY_BINDINGS[] = {
    // Standard Alt hotkeys
    { SWITCHER_KEY,         HOTKEY_MOD_ALT,  switcher_blacklist,        SWITCHER_BLACKLIST_COUNT,        SwitcherHandler,          "Switcher" },
    { GOOGLE_SEARCH_KEY,    HOTKEY_MOD_ALT,  google_search_blacklist,   GOOGLE_SEARCH_BLACKLIST_COUNT,   GoogleHandler,            "Google" },
    { TODO_KEY,             HOTKEY_MOD_ALT,  todo_blacklist,            TODO_BLACKLIST_COUNT,            TodoHandler,              "Todo" },
    { CALCULATOR_KEY,       HOTKEY_MOD_ALT,  calculator_blacklist,      CALCULATOR_BLACKLIST_COUNT,      CalculatorHandler,        "Calculator" },
    { EXE_KEY,              HOTKEY_MOD_ALT,  exe_blacklist,             EXE_BLACKLIST_COUNT,             AppLauncherHandler,       "AppLauncher" },
    { KILL_PROCESS_KEY,     HOTKEY_MOD_ALT,  kill_process_blacklist,    KILL_PROCESS_BLACKLIST_COUNT,    ProcessKillerHandler,     "ProcessKiller" },
    // Neovide: Alt+N
    { NVIM_KEY,             HOTKEY_MOD_ALT,  nvim_blacklist,            NVIM_BLACKLIST_COUNT,            NeovideHandler,           "Neovide" },
    // Nvim: Alt+Ctrl+N
    { NVIM_KEY,             (HOTKEY_MOD_ALT | HOTKEY_MOD_CTRL), nvim_blacklist, NVIM_BLACKLIST_COUNT, NvimHandler, "Nvim" },
    { YAZI_KEY,             HOTKEY_MOD_ALT,  yazi_blacklist,            YAZI_BLACKLIST_COUNT,            YaziHandler,              "Yazi" },
    { TERMINAL_KEY,         HOTKEY_MOD_ALT,  terminal_blacklist,        TERMINAL_BLACKLIST_COUNT,        TerminalHandler,          "Terminal" },
    { TIMER_KEY,            HOTKEY_MOD_ALT,  timer_blacklist,           TIMER_BLACKLIST_COUNT,           TimerHandler,             "Timer" },
    { EXPLORER_KEY,         HOTKEY_MOD_ALT,  explorer_blacklist,        EXPLORER_BLACKLIST_COUNT,        ExplorerHandler,          "Explorer" },
    { TERMINAL_COMMAND_KEY, HOTKEY_MOD_ALT,  terminal_command_blacklist, TERMINAL_COMMAND_BLACKLIST_COUNT, TerminalCommandHandler, "TerminalCommand" },
    { VOLUME_KEY,           HOTKEY_MOD_ALT,  volume_blacklist,          VOLUME_BLACKLIST_COUNT,          VolumeHandler,            "Volume" },
    
    // POS - different modifier combos
    { POS_KEY,              HOTKEY_MOD_ALT,                         pos_blacklist, POS_BLACKLIST_COUNT, PosCopyHandler,   "PosCopy" },
    { POS_KEY,              (HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT),    pos_blacklist, POS_BLACKLIST_COUNT, PosAddHandler,    "PosAdd" },
    { POS_KEY,              (HOTKEY_MOD_ALT | HOTKEY_MOD_CTRL),     pos_blacklist, POS_BLACKLIST_COUNT, PosClearHandler,  "PosClear" },
    
    // COLOR
    { COLOR_KEY,            HOTKEY_MOD_ALT,                         color_blacklist, COLOR_BLACKLIST_COUNT, ColorPlainHandler,   "ColorPlain" },
    { COLOR_KEY,            (HOTKEY_MOD_ALT | HOTKEY_MOD_CTRL),     color_blacklist, COLOR_BLACKLIST_COUNT, ColorParenHandler,   "ColorParen" },
    
    // PIXEL_EQUALS
    { PIXEL_EQUALS_KEY,     HOTKEY_MOD_ALT,                         pixel_equals_blacklist, PIXEL_EQUALS_BLACKLIST_COUNT, PixelEqualsAdbHandler, "PixelEqualsAdb" },
    { PIXEL_EQUALS_KEY,     (HOTKEY_MOD_ALT | HOTKEY_MOD_CTRL),     pixel_equals_blacklist, PIXEL_EQUALS_BLACKLIST_COUNT, PixelEqualsFnHandler,  "PixelEqualsFn" },
    
    // WAIT_FOR_PIXEL
    { WAIT_FOR_PIXEL_KEY,   HOTKEY_MOD_ALT,                         wait_for_pixel_blacklist, WAIT_FOR_PIXEL_BLACKLIST_COUNT, WaitForPixelAdbHandler, "WaitForPixelAdb" },
    { WAIT_FOR_PIXEL_KEY,   (HOTKEY_MOD_ALT | HOTKEY_MOD_CTRL),     wait_for_pixel_blacklist, WAIT_FOR_PIXEL_BLACKLIST_COUNT, WaitForPixelTapHandler, "WaitForPixelTap" },
    { WAIT_FOR_PIXEL_KEY,   (HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT),    wait_for_pixel_blacklist, WAIT_FOR_PIXEL_BLACKLIST_COUNT, WaitForPixelFnHandler,  "WaitForPixelFn" },
    
    // REGION
    { REGION_KEY,           HOTKEY_MOD_ALT,                         region_blacklist, REGION_BLACKLIST_COUNT, RegionCoordsHandler, "RegionCoords" },
    { REGION_KEY,           (HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT),    region_blacklist, REGION_BLACKLIST_COUNT, RegionImageHandler,  "RegionImage" },
    
    // SCREENSHOT
    { SCREENSHOT_KEY,       HOTKEY_MOD_ALT,                         screenshot_blacklist, SCREENSHOT_BLACKLIST_COUNT, ScreenshotSaveHandler, "ScreenshotSave" },
    { SCREENSHOT_KEY,       (HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT),    screenshot_blacklist, SCREENSHOT_BLACKLIST_COUNT, ScreenshotOpenHandler, "ScreenshotOpen" },
};

#define HOTKEY_BINDING_COUNT (sizeof(G_HOTKEY_BINDINGS) / sizeof(G_HOTKEY_BINDINGS[0]))

#endif // HOTKEY_DEFINITIONS_H
