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


#define SWITCHER      0x20
#define TODO          0x54
#define CALCULATOR    0x43
#define EXE           0x45
#define KILL_PROCESS  0x55
#define GOOGLE_SEARCH  0x47  
#define TERMINAL     0x57
#define YAZI     0x51
#define TIMER     0x4F
#define EXPLORER     0x52
#define TERMINAL_COMMAND     0xDB
#define VOLUME     0x4D
#define SCREENSHOT          0x59
#define POS                 0x5A
#define COLOR               0x58
#define PIXEL_EQUALS        0x42
#define WAIT_FOR_PIXEL      0x4E
#define REGION              0x41
#define NVIM     0x49
#define MONITOR_FOCUS_MAIN     0x4C
#define MONITOR_FOCUS_SIDE     0x48
#define REMINDER     0x53

static const char* SWITCHER_blacklist[] = {"Fortnite", "CrosshairX"};
#define SWITCHER_BLACKLIST_COUNT (sizeof(SWITCHER_blacklist) / sizeof(SWITCHER_blacklist[0]))

static const char* REMINDER_blacklist[] = {"Fortnite", "CrosshairX"};
#define REMINDER_BLACKLIST_COUNT (sizeof(REMINDER_blacklist) / sizeof(REMINDER_blacklist[0]))

static const char* TODO_blacklist[] = {"Fortnite", "CrosshairX"};
#define TODO_BLACKLIST_COUNT (sizeof(TODO_blacklist) / sizeof(TODO_blacklist[0]))

static const char* POS_blacklist[] = {"Fortnite", "CrosshairX"};
#define POS_BLACKLIST_COUNT (sizeof(POS_blacklist) / sizeof(POS_blacklist[0]))

static const char* COLOR_blacklist[] = {"Fortnite", "CrosshairX"};
#define COLOR_BLACKLIST_COUNT (sizeof(COLOR_blacklist) / sizeof(COLOR_blacklist[0]))

static const char* PIXEL_EQUALS_blacklist[] = {"Fortnite", "CrosshairX"};
#define PIXEL_EQUALS_BLACKLIST_COUNT (sizeof(PIXEL_EQUALS_blacklist) / sizeof(PIXEL_EQUALS_blacklist[0]))

static const char* WAIT_FOR_PIXEL_blacklist[] = {"Fortnite", "CrosshairX"};
#define WAIT_FOR_PIXEL_BLACKLIST_COUNT (sizeof(WAIT_FOR_PIXEL_blacklist) / sizeof(WAIT_FOR_PIXEL_blacklist[0]))

static const char* REGION_blacklist[] = {"Fortnite", "CrosshairX"};
#define REGION_BLACKLIST_COUNT (sizeof(REGION_blacklist) / sizeof(REGION_blacklist[0]))

static const char* CALCULATOR_blacklist[] = {"Fortnite", "CrosshairX"};
#define CALCULATOR_BLACKLIST_COUNT (sizeof(CALCULATOR_blacklist) / sizeof(CALCULATOR_blacklist[0]))

static const char* EXE_blacklist[] = {"Fortnite", "CrosshairX"};
#define EXE_BLACKLIST_COUNT (sizeof(EXE_blacklist) / sizeof(EXE_blacklist[0]))

static const char* KILL_PROCESS_blacklist[] = {"Fortnite", "CrosshairX"};
#define KILL_PROCESS_BLACKLIST_COUNT (sizeof(KILL_PROCESS_blacklist) / sizeof(KILL_PROCESS_blacklist[0]))

static const char* GOOGLE_SEARCH_blacklist[] = {"Fortnite", "CrosshairX"};
#define GOOGLE_SEARCH_BLACKLIST_COUNT (sizeof(GOOGLE_SEARCH_blacklist) / sizeof(GOOGLE_SEARCH_blacklist[0]))

static const char* NVIM_blacklist[] = {"Fortnite", "CrosshairX"};
#define NVIM_BLACKLIST_COUNT (sizeof(NVIM_blacklist) / sizeof(NVIM_blacklist[0]))

static const char* TIMER_blacklist[] = {"Fortnite", "CrosshairX"};
#define TIMER_BLACKLIST_COUNT (sizeof(TIMER_blacklist) / sizeof(TIMER_blacklist[0]))

static const char* TERMINAL_blacklist[] = {"Fortnite", "CrosshairX"};
#define TERMINAL_BLACKLIST_COUNT (sizeof(TERMINAL_blacklist) / sizeof(TERMINAL_blacklist[0]))

static const char* SCREENSHOT_blacklist[] = {"Fortnite", "CrosshairX"};
#define SCREENSHOT_BLACKLIST_COUNT (sizeof(SCREENSHOT_blacklist) / sizeof(SCREENSHOT_blacklist[0]))

static const char* YAZI_blacklist[] = {"Fortnite", "CrosshairX"};
#define YAZI_BLACKLIST_COUNT (sizeof(YAZI_blacklist) / sizeof(YAZI_blacklist[0]))

static const char* VIDEO_blacklist[] = {"Fortnite", "CrosshairX"};
#define VIDEO_BLACKLIST_COUNT (sizeof(VIDEO_blacklist) / sizeof(VIDEO_blacklist[0]))

static const char* EXPLORER_blacklist[] = {"Fortnite", "CrosshairX"};
#define EXPLORER_BLACKLIST_COUNT (sizeof(EXPLORER_blacklist) / sizeof(EXPLORER_blacklist[0]))

static const char* TERMINAL_COMMAND_blacklist[] = {"Fortnite", "CrosshairX"};
#define TERMINAL_COMMAND_BLACKLIST_COUNT (sizeof(TERMINAL_COMMAND_blacklist) / sizeof(TERMINAL_COMMAND_blacklist[0]))

static const char* VOLUME_blacklist[] = {"Fortnite", "CrosshairX"};
#define VOLUME_BLACKLIST_COUNT (sizeof(VOLUME_blacklist) / sizeof(VOLUME_blacklist[0]))

static const char* MONITOR_FOCUS_MAIN_blacklist[] = {"Nvim", "Neovide","Fortnite", "CrosshairX"};
#define MONITOR_FOCUS_MAIN_BLACKLIST_COUNT (sizeof(MONITOR_FOCUS_MAIN_blacklist) / sizeof(MONITOR_FOCUS_MAIN_blacklist[0]))

// Fixed the copy-paste bug here so it references SIDE instead of MAIN
static const char* MONITOR_FOCUS_SIDE_blacklist[] = {"Nvim", "Neovide","Fortnite", "CrosshairX"};
#define MONITOR_FOCUS_SIDE_BLACKLIST_COUNT (sizeof(MONITOR_FOCUS_SIDE_blacklist) / sizeof(MONITOR_FOCUS_SIDE_blacklist[0]))

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
  printf("THIS IS NOT IMPLEMETNETD");
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
#define BINDING(key, mod, handler, enabled) \
    { key, mod, key##_blacklist, key##_BLACKLIST_COUNT, handler, #key, enabled }

#define BINDING_CUSTOM(key, mod, blist, blist_count, handler, name, enabled) \
    { key, mod, blist, blist_count, handler, name, enabled }

static const HotkeyBinding G_HOTKEY_BINDINGS[] = {
    BINDING(SWITCHER,           HOTKEY_MOD_ALT,                      SwitcherHandler,         true),
    BINDING(GOOGLE_SEARCH,      HOTKEY_MOD_ALT,                      TriggerGoogleSearch,     true),
    BINDING(TODO,               HOTKEY_MOD_ALT,                      TriggerTodo,             true),
    BINDING(CALCULATOR,         HOTKEY_MOD_ALT,                      TriggerCalculator,       true),
    BINDING(EXE,                HOTKEY_MOD_ALT,                      TriggerAppLauncher,      true),
    BINDING(KILL_PROCESS,       HOTKEY_MOD_ALT,                      TriggerProcessKiller,    true),
    BINDING(YAZI,               HOTKEY_MOD_ALT,                      TriggerYaziZoxide,       true),
    BINDING(TERMINAL,           HOTKEY_MOD_ALT,                      TriggerZoxide,           true),
    BINDING(TIMER,              HOTKEY_MOD_ALT,                      TriggerTimer,            TimerAvailable),
    BINDING(EXPLORER,           HOTKEY_MOD_ALT,                      TriggerExplorer,         true),
    BINDING(TERMINAL_COMMAND,   HOTKEY_MOD_ALT,                      TriggerTerminalCommand,  true),
    BINDING(VOLUME,             HOTKEY_MOD_ALT,                      TriggerVolume,           true),
    BINDING(MONITOR_FOCUS_MAIN, HOTKEY_MOD_ALT,                      TriggerMonitorFocusMain, true),
    BINDING(MONITOR_FOCUS_SIDE, HOTKEY_MOD_ALT,                      TriggerMonitorFocusSide, true),
    BINDING(REMINDER,           HOTKEY_MOD_ALT,                      TriggerReminder,         true),
    BINDING(POS,                HOTKEY_MOD_ALT,                      PosCopyHandler,          true),
    BINDING(POS,                (HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT), PosAddHandler,           true),
    BINDING(POS,                (HOTKEY_MOD_ALT | HOTKEY_MOD_CTRL),  PosClearHandler,         true),
    BINDING(COLOR,              HOTKEY_MOD_ALT,                      ColorPlainHandler,       true),
    BINDING(COLOR,              (HOTKEY_MOD_ALT | HOTKEY_MOD_CTRL),  ColorParenHandler,       true),
    BINDING(PIXEL_EQUALS,       HOTKEY_MOD_ALT,                      PixelEqualsAdbHandler,   true),
    BINDING(PIXEL_EQUALS,       (HOTKEY_MOD_ALT | HOTKEY_MOD_CTRL),  PixelEqualsFnHandler,    true),
    BINDING(WAIT_FOR_PIXEL,     HOTKEY_MOD_ALT,                      WaitForPixelAdbHandler,  true),
    BINDING(WAIT_FOR_PIXEL,     (HOTKEY_MOD_ALT | HOTKEY_MOD_CTRL),  WaitForPixelTapHandler,  true),
    BINDING(WAIT_FOR_PIXEL,     (HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT), WaitForPixelFnHandler,   true),
    BINDING(REGION,             HOTKEY_MOD_ALT,                      RegionCoordsHandler,     true),
    BINDING(REGION,             (HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT), RegionImageHandler,      true),
    BINDING(SCREENSHOT,         HOTKEY_MOD_ALT,                      ScreenshotSaveHandler,   true),
    BINDING(SCREENSHOT,         (HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT), ScreenshotOpenHandler,   true),
    BINDING(NVIM,               HOTKEY_MOD_ALT,                      NeovideHandler,          true),
};

#define HOTKEY_BINDING_COUNT (sizeof(G_HOTKEY_BINDINGS) / sizeof(G_HOTKEY_BINDINGS[0]))

#endif 
