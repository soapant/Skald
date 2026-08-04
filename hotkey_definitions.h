#ifndef HOTKEY_DEFINITIONS_H
#define HOTKEY_DEFINITIONS_H

#include "helpers.h"
#include "calc.h"
#include "config.h"
#include "types.h"
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
#include "focus_switcher.h"
#include "minecraft.h"
#include "google_search.h"
#include "process_killer.h"
#include "todo.h"
#include "volume.h"
#include "pixel_utils.h"
#include "monitor_focus.h"
#include "reminder.h"
#include "mute_microphone.h"
#include "windows_control.h"
#include "replica.h"
#include "forgetfull.h"


static const char* SWITCHER_blacklist[] = {"Fortnite", "CrosshairX"};
#define SWITCHER_BLACKLIST_COUNT (sizeof(SWITCHER_blacklist) / sizeof(SWITCHER_blacklist[0]))

static const char* FORGETFULL_blacklist[] = {"Fortnite", "CrosshairX"};
#define FORGETFULL_BLACKLIST_COUNT (sizeof(FORGETFULL_blacklist) / sizeof(FORGETFULL_blacklist[0]))

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

static const char* MUTE_blacklist[] = {""};
#define MUTE_BLACKLIST_COUNT (sizeof(MUTE_blacklist) / sizeof(MUTE_blacklist[0]))

static const char* REPLICA_blacklist[] = {""};
#define REPLICA_BLACKLIST_COUNT (sizeof(REPLICA_blacklist) / sizeof(REPLICA_blacklist[0]))

static const char* WIFI_TOGGLE_blacklist[] = {""};
#define WIFI_TOGGLE_BLACKLIST_COUNT (sizeof(WIFI_TOGGLE_blacklist) / sizeof(WIFI_TOGGLE_blacklist[0]))

static const char* BLUETOOTH_TOGGLE_blacklist[] = {""};
#define BLUETOOTH_TOGGLE_BLACKLIST_COUNT (sizeof(BLUETOOTH_TOGGLE_blacklist) / sizeof(BLUETOOTH_TOGGLE_blacklist[0]))

static const char* MONITOR_FOCUS_MAIN_blacklist[] = {"Nvim", "Neovide","Fortnite", "CrosshairX"};
#define MONITOR_FOCUS_MAIN_BLACKLIST_COUNT (sizeof(MONITOR_FOCUS_MAIN_blacklist) / sizeof(MONITOR_FOCUS_MAIN_blacklist[0]))

static const char* MONITOR_FOCUS_SIDE_blacklist[] = {"Nvim", "Neovide","Fortnite", "CrosshairX"};
#define MONITOR_FOCUS_SIDE_BLACKLIST_COUNT (sizeof(MONITOR_FOCUS_SIDE_blacklist) / sizeof(MONITOR_FOCUS_SIDE_blacklist[0]))

static const char* SETTINGS_blacklist[] = {"Fortnite", "CrosshairX"};
#define SETTINGS_BLACKLIST_COUNT (sizeof(SETTINGS_blacklist) / sizeof(SETTINGS_blacklist[0]))

void HandleKillMyProgram(void) {
    terminate_process_by_name("myprogram.exe");
}
void HandleInitAppLauncherCache(void) {
    initialize_app_cache();
}
void HandleSettingsTrigger(void) {
  SettingsPopUp();
}


#define BINDING(key, mod, handler, enabled) \
    { key, mod, key##_blacklist, key##_BLACKLIST_COUNT, handler, #key, enabled }

#define BINDING_CUSTOM(key, mod, blist, blist_count, handler, name, enabled) \
    { key, mod, blist, blist_count, handler, name, enabled }
#define BINDING_SIMPLE(key,mod,handler) \
    { key, mod, NULL, 0, handler, "", true }

static const HotkeyBinding G_HOTKEY_BINDINGS[] = {
    BINDING(SWITCHER,           HOTKEY_MOD_ALT,       TriggerWindowSwitcher,   true),
    BINDING(GOOGLE_SEARCH,      HOTKEY_MOD_ALT,       TriggerGoogleSearch,     true),
    BINDING(TODO,               HOTKEY_MOD_ALT,       TriggerTodo,             true),
    BINDING(CALCULATOR,         HOTKEY_MOD_ALT,       TriggerCalculator,       true),
    BINDING(EXE,                HOTKEY_MOD_ALT,       TriggerAppLauncher,      true),
    BINDING(KILL_PROCESS,       HOTKEY_MOD_ALT,       TriggerProcessKiller,    true),
    BINDING(YAZI,               HOTKEY_MOD_ALT,       TriggerYaziZoxide,       true),
    BINDING(TERMINAL,           HOTKEY_MOD_ALT,       TriggerZoxide,           true),
    BINDING(TIMER,              HOTKEY_MOD_ALT,       TriggerTimer,            TimerAvailable),
    BINDING(EXPLORER,           HOTKEY_MOD_ALT,       TriggerExplorer,         true),
    BINDING(TERMINAL_COMMAND,   HOTKEY_MOD_ALT,       TriggerTerminalCommand,  true),
    BINDING(VOLUME,             HOTKEY_MOD_ALT,       TriggerVolume,           true),
    BINDING(MONITOR_FOCUS_MAIN, HOTKEY_MOD_ALT,       TriggerMonitorFocusMain, true),
    BINDING(MONITOR_FOCUS_SIDE, HOTKEY_MOD_ALT,       TriggerMonitorFocusSide, true),
    BINDING(REMINDER,           HOTKEY_MOD_ALT,       TriggerReminder,         true),
    BINDING(MUTE,               HOTKEY_MOD_ALT_SHIFT, TriggerMuteState,        true),
    BINDING(FORGETFULL,         HOTKEY_MOD_ALT,       TriggerForgetFull,       true),
    BINDING(REPLICA,            HOTKEY_MOD_ALT_CTRL,  TriggerReplica,          ReplicaAvailable),
    BINDING(POS,                HOTKEY_MOD_ALT,       TriggerPosCopy,          PixelHotkeys),
    BINDING(POS,                HOTKEY_MOD_ALT_SHIFT, TriggerPosAdd,           PixelHotkeys),
    BINDING(POS,                HOTKEY_MOD_ALT_CTRL,  TriggerPosClear,         PixelHotkeys),
    BINDING(COLOR,              HOTKEY_MOD_ALT,       TriggerColorPlain,       PixelHotkeys),
    BINDING(COLOR,              HOTKEY_MOD_ALT_CTRL,  TriggerColorParen,       PixelHotkeys),
    BINDING(PIXEL_EQUALS,       HOTKEY_MOD_ALT,       TriggerPixelEqualsAdb,   PixelHotkeys),
    BINDING(PIXEL_EQUALS,       HOTKEY_MOD_ALT_CTRL,  TriggerPixelEqualsFn,    PixelHotkeys),
    BINDING(REGION,             HOTKEY_MOD_ALT,       TriggerRegionCoords,     PixelHotkeys),
    BINDING(REGION,             HOTKEY_MOD_ALT_SHIFT, TriggerRegionImage,      PixelHotkeys),
    BINDING(SCREENSHOT,         HOTKEY_MOD_ALT,       TriggerSaveScreenshot,   PixelHotkeys),
    BINDING(SCREENSHOT,         HOTKEY_MOD_ALT_SHIFT, TriggerOpenScreenshot,   PixelHotkeys),
    BINDING(NVIM,               HOTKEY_MOD_ALT,       TriggerNvimZoxide,       true),
    BINDING(WIFI_TOGGLE,        HOTKEY_MOD_ALT_SHIFT, TriggerWifiToggle,       true),
    BINDING(BLUETOOTH_TOGGLE,   HOTKEY_MOD_ALT_CTRL,  TriggerBlueToothToggle,  true),

    BINDING(SETTINGS,           HOTKEY_MOD_ALT,  HandleSettingsTrigger,         true),
    BINDING_SIMPLE('M', HOTKEY_MOD_ALT_CTRL,HandleKillMyProgram),
    BINDING_SIMPLE('E', HOTKEY_MOD_ALT_CTRL,HandleInitAppLauncherCache),
};

#define HOTKEY_BINDING_COUNT (sizeof(G_HOTKEY_BINDINGS) / sizeof(G_HOTKEY_BINDINGS[0]))

#endif 
