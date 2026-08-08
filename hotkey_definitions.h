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


static const char* SWITCHER_blacklist[] = {"Fortnite"};
#define SWITCHER_BLACKLIST_COUNT (sizeof(SWITCHER_blacklist) / sizeof(SWITCHER_blacklist[0]))

static const char* FORGETFULL_blacklist[] = {"Fortnite"};
#define FORGETFULL_BLACKLIST_COUNT (sizeof(FORGETFULL_blacklist) / sizeof(FORGETFULL_blacklist[0]))

static const char* REMINDER_blacklist[] = {"CrosshairX"};
#define REMINDER_BLACKLIST_COUNT (sizeof(REMINDER_blacklist) / sizeof(REMINDER_blacklist[0]))

static const char* TODO_blacklist[] = {"Fortnite"};
#define TODO_BLACKLIST_COUNT (sizeof(TODO_blacklist) / sizeof(TODO_blacklist[0]))

static const char* POS_blacklist[] = {"CrosshairX"};
#define POS_BLACKLIST_COUNT (sizeof(POS_blacklist) / sizeof(POS_blacklist[0]))

static const char* COLOR_blacklist[] = {"CrosshairX"};
#define COLOR_BLACKLIST_COUNT (sizeof(COLOR_blacklist) / sizeof(COLOR_blacklist[0]))

static const char* PIXEL_EQUALS_blacklist[] = {"CrosshairX"};
#define PIXEL_EQUALS_BLACKLIST_COUNT (sizeof(PIXEL_EQUALS_blacklist) / sizeof(PIXEL_EQUALS_blacklist[0]))

static const char* WAIT_FOR_PIXEL_blacklist[] = {"CrosshairX"};
#define WAIT_FOR_PIXEL_BLACKLIST_COUNT (sizeof(WAIT_FOR_PIXEL_blacklist) / sizeof(WAIT_FOR_PIXEL_blacklist[0]))

static const char* REGION_blacklist[] = {"CrosshairX"};
#define REGION_BLACKLIST_COUNT (sizeof(REGION_blacklist) / sizeof(REGION_blacklist[0]))

static const char* CALCULATOR_blacklist[] = {"CrosshairX"};
#define CALCULATOR_BLACKLIST_COUNT (sizeof(CALCULATOR_blacklist) / sizeof(CALCULATOR_blacklist[0]))

static const char* EXE_blacklist[] = {"CrosshairX"};
#define EXE_BLACKLIST_COUNT (sizeof(EXE_blacklist) / sizeof(EXE_blacklist[0]))

static const char* KILL_PROCESS_blacklist[] = {"CrosshairX"};
#define KILL_PROCESS_BLACKLIST_COUNT (sizeof(KILL_PROCESS_blacklist) / sizeof(KILL_PROCESS_blacklist[0]))

static const char* GOOGLE_SEARCH_blacklist[] = {"Fortnite","CrosshairX"};
#define GOOGLE_SEARCH_BLACKLIST_COUNT (sizeof(GOOGLE_SEARCH_blacklist) / sizeof(GOOGLE_SEARCH_blacklist[0]))

static const char* NVIM_blacklist[] = {"CrosshairX"};
#define NVIM_BLACKLIST_COUNT (sizeof(NVIM_blacklist) / sizeof(NVIM_blacklist[0]))

static const char* TIMER_blacklist[] = {"CrosshairX"};
#define TIMER_BLACKLIST_COUNT (sizeof(TIMER_blacklist) / sizeof(TIMER_blacklist[0]))

static const char* TERMINAL_blacklist[] = {"CrosshairX"};
#define TERMINAL_BLACKLIST_COUNT (sizeof(TERMINAL_blacklist) / sizeof(TERMINAL_blacklist[0]))

static const char* SCREENSHOT_blacklist[] = {"CrosshairX"};
#define SCREENSHOT_BLACKLIST_COUNT (sizeof(SCREENSHOT_blacklist) / sizeof(SCREENSHOT_blacklist[0]))

static const char* YAZI_blacklist[] = {"CrosshairX"};
#define YAZI_BLACKLIST_COUNT (sizeof(YAZI_blacklist) / sizeof(YAZI_blacklist[0]))

static const char* VIDEO_blacklist[] = {"CrosshairX"};
#define VIDEO_BLACKLIST_COUNT (sizeof(VIDEO_blacklist) / sizeof(VIDEO_blacklist[0]))

static const char* EXPLORER_blacklist[] = {"CrosshairX"};
#define EXPLORER_BLACKLIST_COUNT (sizeof(EXPLORER_blacklist) / sizeof(EXPLORER_blacklist[0]))

static const char* TERMINAL_COMMAND_blacklist[] = {"CrosshairX"};
#define TERMINAL_COMMAND_BLACKLIST_COUNT (sizeof(TERMINAL_COMMAND_blacklist) / sizeof(TERMINAL_COMMAND_blacklist[0]))

static const char* VOLUME_blacklist[] = {"CrosshairX"};
#define VOLUME_BLACKLIST_COUNT (sizeof(VOLUME_blacklist) / sizeof(VOLUME_blacklist[0]))

static const char* MUTE_blacklist[] = {""};
#define MUTE_BLACKLIST_COUNT (sizeof(MUTE_blacklist) / sizeof(MUTE_blacklist[0]))

static const char* REPLICA_blacklist[] = {""};
#define REPLICA_BLACKLIST_COUNT (sizeof(REPLICA_blacklist) / sizeof(REPLICA_blacklist[0]))

static const char* WIFI_TOGGLE_blacklist[] = {""};
#define WIFI_TOGGLE_BLACKLIST_COUNT (sizeof(WIFI_TOGGLE_blacklist) / sizeof(WIFI_TOGGLE_blacklist[0]))

static const char* BLUETOOTH_TOGGLE_blacklist[] = {""};
#define BLUETOOTH_TOGGLE_BLACKLIST_COUNT (sizeof(BLUETOOTH_TOGGLE_blacklist) / sizeof(BLUETOOTH_TOGGLE_blacklist[0]))

static const char* MONITOR_FOCUS_MAIN_blacklist[] = {"Fortnite","Nvim", "Neovide"};
#define MONITOR_FOCUS_MAIN_BLACKLIST_COUNT (sizeof(MONITOR_FOCUS_MAIN_blacklist) / sizeof(MONITOR_FOCUS_MAIN_blacklist[0]))

static const char* MONITOR_FOCUS_SIDE_blacklist[] = {"Fortnite","Nvim", "Neovide"};
#define MONITOR_FOCUS_SIDE_BLACKLIST_COUNT (sizeof(MONITOR_FOCUS_SIDE_blacklist) / sizeof(MONITOR_FOCUS_SIDE_blacklist[0]))

static const char* SETTINGS_blacklist[] = {"CrosshairX"};
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

bool IsPixelHotkeysEnabled(void) { return PixelHotkeys; }
bool IsTimerAvailable(void) { return TimerAvailable; }
bool IsReplicaAvailable(void) { return ReplicaAvailable; }

void HandleSwapHotkeyMode(void) {
    PixelHotkeys = !PixelHotkeys;
    char buffer[20];
    snprintf(buffer,sizeof(buffer),"Set The Pixel Hotkeys Mode To %s",PixelHotkeys  ? "True" : "False");
    MSG_BOTTOM(buffer,140,TimeOutForMessages);
}


#define BINDING(key, mod, handler, enabled) \
    { key, mod, key##_blacklist, key##_BLACKLIST_COUNT, handler, #key, enabled }

#define BINDING_CUSTOM(key, mod, blist, blist_count, handler, name, enabled) \
    { key, mod, blist, blist_count, handler, name, enabled }
#define BINDING_SIMPLE(key,mod,handler) \
    { key, mod, NULL, 0, handler, "", AlwaysEnabled }

static HotkeyBinding G_HOTKEY_BINDINGS[] = {
    BINDING(SWITCHER,           HOTKEY_MOD_ALT,       TriggerWindowSwitcher,   AlwaysEnabled),
    BINDING(GOOGLE_SEARCH,      HOTKEY_MOD_ALT,       TriggerGoogleSearch,     AlwaysEnabled),
    BINDING(TODO,               HOTKEY_MOD_ALT,       TriggerTodo,             AlwaysEnabled),
    BINDING(CALCULATOR,         HOTKEY_MOD_ALT,       TriggerCalculator,       AlwaysEnabled),
    BINDING(EXE,                HOTKEY_MOD_ALT,       TriggerAppLauncher,      AlwaysEnabled),
    BINDING(KILL_PROCESS,       HOTKEY_MOD_ALT,       TriggerProcessKiller,    AlwaysEnabled),
    BINDING(YAZI,               HOTKEY_MOD_ALT,       TriggerYaziZoxide,       AlwaysEnabled),
    BINDING(TERMINAL,           HOTKEY_MOD_ALT,       TriggerZoxide,           AlwaysEnabled),
    BINDING(TIMER,              HOTKEY_MOD_ALT,       TriggerTimer,            IsTimerAvailable),
    BINDING(EXPLORER,           HOTKEY_MOD_ALT,       TriggerExplorer,         AlwaysEnabled),
    BINDING(TERMINAL_COMMAND,   HOTKEY_MOD_ALT,       TriggerTerminalCommand,  AlwaysEnabled),
    BINDING(VOLUME,             HOTKEY_MOD_ALT,       TriggerVolume,           AlwaysEnabled),
    BINDING(MONITOR_FOCUS_MAIN, HOTKEY_MOD_ALT,       TriggerMonitorFocusMain, AlwaysEnabled),
    BINDING(MONITOR_FOCUS_SIDE, HOTKEY_MOD_ALT,       TriggerMonitorFocusSide, AlwaysEnabled),
    BINDING(REMINDER,           HOTKEY_MOD_ALT,       TriggerReminder,         AlwaysEnabled),
    BINDING(MUTE,               HOTKEY_MOD_ALT_SHIFT, TriggerMuteState,        AlwaysEnabled),
    BINDING(FORGETFULL,         HOTKEY_MOD_ALT,       TriggerForgetFull,       AlwaysEnabled),
    BINDING(REPLICA,            HOTKEY_MOD_ALT_CTRL,  TriggerReplica,          IsReplicaAvailable),
    BINDING(POS,                HOTKEY_MOD_ALT,       TriggerPosCopy,          IsPixelHotkeysEnabled),
    BINDING(POS,                HOTKEY_MOD_ALT_SHIFT, TriggerPosAdd,           IsPixelHotkeysEnabled),
    BINDING(POS,                HOTKEY_MOD_ALT_CTRL,  TriggerPosClear,         IsPixelHotkeysEnabled),
    BINDING(COLOR,              HOTKEY_MOD_ALT,       TriggerColorPlain,       IsPixelHotkeysEnabled),
    BINDING(COLOR,              HOTKEY_MOD_ALT_CTRL,  TriggerColorParen,       IsPixelHotkeysEnabled),
    BINDING(PIXEL_EQUALS,       HOTKEY_MOD_ALT,       TriggerPixelEqualsAdb,   IsPixelHotkeysEnabled),
    BINDING(PIXEL_EQUALS,       HOTKEY_MOD_ALT_CTRL,  TriggerPixelEqualsFn,    IsPixelHotkeysEnabled),
    BINDING(REGION,             HOTKEY_MOD_ALT,       TriggerRegionCoords,     IsPixelHotkeysEnabled),
    BINDING(REGION,             HOTKEY_MOD_ALT_SHIFT, TriggerRegionImage,      IsPixelHotkeysEnabled),
    BINDING(SCREENSHOT,         HOTKEY_MOD_ALT,       TriggerSaveScreenshot,   IsPixelHotkeysEnabled),
    BINDING(SCREENSHOT,         HOTKEY_MOD_ALT_SHIFT, TriggerOpenScreenshot,   IsPixelHotkeysEnabled),
    BINDING(NVIM,               HOTKEY_MOD_ALT,       TriggerNvimZoxide,       AlwaysEnabled),
    BINDING(WIFI_TOGGLE,        HOTKEY_MOD_ALT_SHIFT, TriggerWifiToggle,       AlwaysEnabled),
    BINDING(BLUETOOTH_TOGGLE,   HOTKEY_MOD_ALT_CTRL,  TriggerBlueToothToggle,  AlwaysEnabled),
    BINDING(SETTINGS,           HOTKEY_MOD_ALT,  HandleSettingsTrigger,         AlwaysEnabled),

    BINDING_SIMPLE('M', HOTKEY_MOD_ALT_CTRL,HandleKillMyProgram),
    BINDING_SIMPLE('E', HOTKEY_MOD_ALT_CTRL,HandleInitAppLauncherCache),
    BINDING_SIMPLE(TOGGLE_PIXEL_HOTKEYS_KEY,HOTKEY_MOD_SHIFT_CTRL,HandleSwapHotkeyMode),
};

#define HOTKEY_BINDING_COUNT (sizeof(G_HOTKEY_BINDINGS) / sizeof(G_HOTKEY_BINDINGS[0]))

#endif 
