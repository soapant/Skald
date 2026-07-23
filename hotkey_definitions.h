#ifndef HOTKEY_DEFINITIONS_H
#define HOTKEY_DEFINITIONS_H

#include "helpers.h"
#include "config.h"
#include "calc.h"
#include "video_player.h"
#include "music_player.h"
#include "timer.h"
#include "explorer.h"
#include "screenshot.h"
#include "terminal_command.h"
#include "zoxide.h"
#include "nvim_zoxide.h"
#include "yazi_zoxide.h"
#include "window_switcher.h"
#include "pos_handler.h"
#include "focus_switcher.h"
#include "color_handler.h"
#include "minecraft.h"
#include "pixel_equals_handler.h"
#include "wait_for_pixel_handler.h"
#include "region_handler.h"
#include "google_search.h"
#include "process_killer.h"
#include "todo.h"
#include "volume.h"
#include "monitor_focus.h"
#include "reminder.h"


extern BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
extern HWND hPopupWnd;
extern HWND hGlobalTrayWnd;

#define SWITCHER_KEY      0x20
#define TODO_KEY          0x54
#define CALCULATOR_KEY    0x43
#define EXE_KEY           0x45
#define KILL_PROCESS_KEY  0x55
#define GOOGLE_SEARCH_KEY  0x47  
#define TERMINAL_KEY     0x57
#define YAZI_KEY     0x51
#define TIMER_KEY     0x4F
#define EXPLORER_KEY     0x52
#define TERMINAL_COMMAND_KEY     0xDB
#define VOLUME_KEY     0x4D
#define SCREENSHOT_KEY          0x59
#define POS_KEY                 0x5A
#define COLOR_KEY               0x58
#define PIXEL_EQUALS_KEY        0x42
#define WAIT_FOR_PIXEL_KEY      0x4E
#define REGION_KEY              0x41
#define NVIM_KEY     0x49
#define MONITOR_FOCUS_MAIN_KEY     0x4C
#define MONITOR_FOCUS_SIDE_KEY     0x48
#define REMINDER_KEY     0x53

static const char* switcher_blacklist[] = {"Fortnite", "CrosshairX"};
#define SWITCHER_BLACKLIST_COUNT (sizeof(switcher_blacklist) / sizeof(switcher_blacklist[0]))

static const char* reminder_blacklist[] = {"Fortnite", "CrosshairX"};
#define REMINDER_BLACKLIST_COUNT (sizeof(reminder_blacklist) / sizeof(reminder_blacklist[0]))

static const char* todo_blacklist[] = {"Fortnite", "CrosshairX"};
#define TODO_BLACKLIST_COUNT (sizeof(todo_blacklist) / sizeof(todo_blacklist[0]))

static const char* pos_blacklist[] = {"Fortnite", "CrosshairX"};
#define POS_BLACKLIST_COUNT (sizeof(pos_blacklist) / sizeof(pos_blacklist[0]))

static const char* color_blacklist[] = {"Fortnite", "CrosshairX"};
#define COLOR_BLACKLIST_COUNT (sizeof(color_blacklist) / sizeof(color_blacklist[0]))

static const char* pixel_equals_blacklist[] = {"Fortnite", "CrosshairX"};
#define PIXEL_EQUALS_BLACKLIST_COUNT (sizeof(pixel_equals_blacklist) / sizeof(pixel_equals_blacklist[0]))

static const char* wait_for_pixel_blacklist[] = {"Fortnite", "CrosshairX"};
#define WAIT_FOR_PIXEL_BLACKLIST_COUNT (sizeof(wait_for_pixel_blacklist) / sizeof(wait_for_pixel_blacklist[0]))

static const char* region_blacklist[] = {"Fortnite", "CrosshairX"};
#define REGION_BLACKLIST_COUNT (sizeof(region_blacklist) / sizeof(region_blacklist[0]))

static const char* calculator_blacklist[] = {"Fortnite", "CrosshairX"};
#define CALCULATOR_BLACKLIST_COUNT (sizeof(calculator_blacklist) / sizeof(calculator_blacklist[0]))

static const char* exe_blacklist[] = {"Fortnite", "CrosshairX"};
#define EXE_BLACKLIST_COUNT (sizeof(exe_blacklist) / sizeof(exe_blacklist[0]))

static const char* kill_process_blacklist[] = {"Fortnite", "CrosshairX"};
#define KILL_PROCESS_BLACKLIST_COUNT (sizeof(kill_process_blacklist) / sizeof(kill_process_blacklist[0]))

static const char* google_search_blacklist[] = {"Fortnite", "CrosshairX"};
#define GOOGLE_SEARCH_BLACKLIST_COUNT (sizeof(google_search_blacklist) / sizeof(google_search_blacklist[0]))

static const char* nvim_blacklist[] = {"Fortnite", "CrosshairX"};
#define NVIM_BLACKLIST_COUNT (sizeof(nvim_blacklist) / sizeof(nvim_blacklist[0]))

static const char* timer_blacklist[] = {"Fortnite", "CrosshairX"};
#define TIMER_BLACKLIST_COUNT (sizeof(timer_blacklist) / sizeof(timer_blacklist[0]))

static const char* terminal_blacklist[] = {"Fortnite", "CrosshairX"};
#define TERMINAL_BLACKLIST_COUNT (sizeof(terminal_blacklist) / sizeof(terminal_blacklist[0]))

static const char* screenshot_blacklist[] = {"Fortnite", "CrosshairX"};
#define SCREENSHOT_BLACKLIST_COUNT (sizeof(screenshot_blacklist) / sizeof(screenshot_blacklist[0]))

static const char* yazi_blacklist[] = {"Fortnite", "CrosshairX"};
#define YAZI_BLACKLIST_COUNT (sizeof(yazi_blacklist) / sizeof(yazi_blacklist[0]))

static const char* video_blacklist[] = {"Fortnite", "CrosshairX"};
#define VIDEO_BLACKLIST_COUNT (sizeof(video_blacklist) / sizeof(video_blacklist[0]))

static const char* explorer_blacklist[] = {"Fortnite", "CrosshairX"};
#define EXPLORER_BLACKLIST_COUNT (sizeof(explorer_blacklist) / sizeof(explorer_blacklist[0]))

static const char* terminal_command_blacklist[] = {"Fortnite", "CrosshairX"};
#define TERMINAL_COMMAND_BLACKLIST_COUNT (sizeof(terminal_command_blacklist) / sizeof(terminal_command_blacklist[0]))

static const char* volume_blacklist[] = {"Fortnite", "CrosshairX"};
#define VOLUME_BLACKLIST_COUNT (sizeof(volume_blacklist) / sizeof(volume_blacklist[0]))

static const char* monitor_focus_main_blacklist[] = {"Nvim", "Neovide","Fortnite", "CrosshairX"};
#define MONITOR_FOCUS_MAIN_BLACKLIST_COUNT (sizeof(monitor_focus_main_blacklist) / sizeof(monitor_focus_main_blacklist[0]))

static const char* monitor_focus_side_blacklist[] = {"Nvim", "Neovide","Fortnite", "CrosshairX"};

#define MONITOR_FOCUS_SIDE_BLACKLIST_COUNT (sizeof(monitor_focus_main_blacklist) / sizeof(monitor_focus_main_blacklist[0]))

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
static inline void NvimHandler(void) { TriggerNvimZoxide(1, hPopupWnd); }

static inline void NeovideHandler(void) { TriggerNvimZoxide(0, hPopupWnd); }


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


static inline void ScreenshotSaveHandler(void) {
    POINT cursorPos;
    if (!GetCursorPos(&cursorPos)) return;
    
    extern int img_region_step;
    extern long img_x1, img_y1, img_x2, img_y2;
    printf("%i",img_region_step);
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
static const HotkeyBinding G_HOTKEY_BINDINGS[] = {
    
    { SWITCHER_KEY,         HOTKEY_MOD_ALT,  switcher_blacklist,        SWITCHER_BLACKLIST_COUNT,        SwitcherHandler,          "Switcher" },
    { GOOGLE_SEARCH_KEY,    HOTKEY_MOD_ALT,  google_search_blacklist,   GOOGLE_SEARCH_BLACKLIST_COUNT,   TriggerGoogleSearch,            "Google" },
    { TODO_KEY,             HOTKEY_MOD_ALT,  todo_blacklist,            TODO_BLACKLIST_COUNT,            TriggerTodo,              "Todo" },
    { CALCULATOR_KEY,       HOTKEY_MOD_ALT,  calculator_blacklist,      CALCULATOR_BLACKLIST_COUNT,      TriggerCalculator,        "Calculator" },
    { EXE_KEY,              HOTKEY_MOD_ALT,  exe_blacklist,             EXE_BLACKLIST_COUNT,             TriggerAppLauncher,       "AppLauncher" },
    { KILL_PROCESS_KEY,     HOTKEY_MOD_ALT,  kill_process_blacklist,    KILL_PROCESS_BLACKLIST_COUNT,    TriggerProcessKiller,     "ProcessKiller" },
    
    { YAZI_KEY,             HOTKEY_MOD_ALT,  yazi_blacklist,            YAZI_BLACKLIST_COUNT,            TriggerYaziZoxide,              "Yazi" },
    { TERMINAL_KEY,         HOTKEY_MOD_ALT,  terminal_blacklist,        TERMINAL_BLACKLIST_COUNT,        TriggerZoxide,          "Terminal" },
    { TIMER_KEY,            HOTKEY_MOD_ALT,  timer_blacklist,           TIMER_BLACKLIST_COUNT,           TriggerTimer,             "Timer" },
    { EXPLORER_KEY,         HOTKEY_MOD_ALT,  explorer_blacklist,        EXPLORER_BLACKLIST_COUNT,        TriggerExplorer,          "Explorer" },
    { TERMINAL_COMMAND_KEY, HOTKEY_MOD_ALT,  terminal_command_blacklist, TERMINAL_COMMAND_BLACKLIST_COUNT, TriggerTerminalCommand, "TerminalCommand" },
    { VOLUME_KEY,           HOTKEY_MOD_ALT,  volume_blacklist,          VOLUME_BLACKLIST_COUNT,          TriggerVolume,            "Volume" },
    
    { MONITOR_FOCUS_MAIN_KEY,           HOTKEY_MOD_ALT,  monitor_focus_main_blacklist,          MONITOR_FOCUS_MAIN_BLACKLIST_COUNT,          TriggerMonitorFocusMain,            "MonitorFocusMain" },
    
    { MONITOR_FOCUS_SIDE_KEY,           HOTKEY_MOD_ALT,  monitor_focus_side_blacklist,          MONITOR_FOCUS_SIDE_BLACKLIST_COUNT,          TriggerMonitorFocusSide,            "MonitorFocusSide" },
    
    { VOLUME_KEY,           HOTKEY_MOD_ALT,  volume_blacklist,          VOLUME_BLACKLIST_COUNT,          TriggerVolume,            "Volume" },
    
    { REMINDER_KEY,           HOTKEY_MOD_ALT,  reminder_blacklist,          REMINDER_BLACKLIST_COUNT,          TriggerReminder,            "Reminder" },
    
    { POS_KEY,              HOTKEY_MOD_ALT,                         pos_blacklist, POS_BLACKLIST_COUNT, PosCopyHandler,   "PosCopy" },
    { POS_KEY,              (HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT),    pos_blacklist, POS_BLACKLIST_COUNT, PosAddHandler,    "PosAdd" },
    { POS_KEY,              (HOTKEY_MOD_ALT | HOTKEY_MOD_CTRL),     pos_blacklist, POS_BLACKLIST_COUNT, PosClearHandler,  "PosClear" },
    
    { COLOR_KEY,            HOTKEY_MOD_ALT,                         color_blacklist, COLOR_BLACKLIST_COUNT, ColorPlainHandler,   "ColorPlain" },
    { COLOR_KEY,            (HOTKEY_MOD_ALT | HOTKEY_MOD_CTRL),     color_blacklist, COLOR_BLACKLIST_COUNT, ColorParenHandler,   "ColorParen" },
    
    { PIXEL_EQUALS_KEY,     HOTKEY_MOD_ALT,                         pixel_equals_blacklist, PIXEL_EQUALS_BLACKLIST_COUNT, PixelEqualsAdbHandler, "PixelEqualsAdb" },
    { PIXEL_EQUALS_KEY,     (HOTKEY_MOD_ALT | HOTKEY_MOD_CTRL),     pixel_equals_blacklist, PIXEL_EQUALS_BLACKLIST_COUNT, PixelEqualsFnHandler,  "PixelEqualsFn" },
    
    { WAIT_FOR_PIXEL_KEY,   HOTKEY_MOD_ALT,                         wait_for_pixel_blacklist, WAIT_FOR_PIXEL_BLACKLIST_COUNT, WaitForPixelAdbHandler, "WaitForPixelAdb" },
    { WAIT_FOR_PIXEL_KEY,   (HOTKEY_MOD_ALT | HOTKEY_MOD_CTRL),     wait_for_pixel_blacklist, WAIT_FOR_PIXEL_BLACKLIST_COUNT, WaitForPixelTapHandler, "WaitForPixelTap" },
    { WAIT_FOR_PIXEL_KEY,   (HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT),    wait_for_pixel_blacklist, WAIT_FOR_PIXEL_BLACKLIST_COUNT, WaitForPixelFnHandler,  "WaitForPixelFn" },
    
    { REGION_KEY,           HOTKEY_MOD_ALT,                         region_blacklist, REGION_BLACKLIST_COUNT, RegionCoordsHandler, "RegionCoords" },
    { REGION_KEY,           (HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT),    region_blacklist, REGION_BLACKLIST_COUNT, RegionImageHandler,  "RegionImage" },
    
    { SCREENSHOT_KEY,       HOTKEY_MOD_ALT,                         screenshot_blacklist, SCREENSHOT_BLACKLIST_COUNT, ScreenshotSaveHandler, "ScreenshotSave" },
    { SCREENSHOT_KEY,       (HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT),    screenshot_blacklist, SCREENSHOT_BLACKLIST_COUNT, ScreenshotOpenHandler, "ScreenshotOpen" },
    
    { NVIM_KEY,             HOTKEY_MOD_ALT,  nvim_blacklist,            NVIM_BLACKLIST_COUNT,            NeovideHandler,           "Neovide" },
    { NVIM_KEY,             (HOTKEY_MOD_ALT | HOTKEY_MOD_CTRL), nvim_blacklist, NVIM_BLACKLIST_COUNT, NvimHandler, "Nvim" },
};

#define HOTKEY_BINDING_COUNT (sizeof(G_HOTKEY_BINDINGS) / sizeof(G_HOTKEY_BINDINGS[0]))

#endif 
