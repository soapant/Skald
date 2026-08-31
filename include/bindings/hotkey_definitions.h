#ifndef HOTKEY_DEFINITIONS_H
#define HOTKEY_DEFINITIONS_H

#include "helpers.h"

#define SWITCHER_BLACKLIST_COUNT (sizeof(SWITCHER_blacklist) / sizeof(SWITCHER_blacklist[0]))

#define FORGETFULL_BLACKLIST_COUNT (sizeof(FORGETFULL_blacklist) / sizeof(FORGETFULL_blacklist[0]))

#define REMINDER_BLACKLIST_COUNT (sizeof(REMINDER_blacklist) / sizeof(REMINDER_blacklist[0]))

#define TODO_BLACKLIST_COUNT (sizeof(TODO_blacklist) / sizeof(TODO_blacklist[0]))

#define POS_BLACKLIST_COUNT (sizeof(POS_blacklist) / sizeof(POS_blacklist[0]))

#define COLOR_BLACKLIST_COUNT (sizeof(COLOR_blacklist) / sizeof(COLOR_blacklist[0]))

#define PIXEL_EQUALS_BLACKLIST_COUNT (sizeof(PIXEL_EQUALS_blacklist) / sizeof(PIXEL_EQUALS_blacklist[0]))

#define WAIT_FOR_PIXEL_BLACKLIST_COUNT (sizeof(WAIT_FOR_PIXEL_blacklist) / sizeof(WAIT_FOR_PIXEL_blacklist[0]))

#define REGION_BLACKLIST_COUNT (sizeof(REGION_blacklist) / sizeof(REGION_blacklist[0]))

#define CALCULATOR_BLACKLIST_COUNT (sizeof(CALCULATOR_blacklist) / sizeof(CALCULATOR_blacklist[0]))

#define EXE_BLACKLIST_COUNT (sizeof(EXE_blacklist) / sizeof(EXE_blacklist[0]))

#define KILL_PROCESS_BLACKLIST_COUNT (sizeof(KILL_PROCESS_blacklist) / sizeof(KILL_PROCESS_blacklist[0]))

#define GOOGLE_SEARCH_BLACKLIST_COUNT (sizeof(GOOGLE_SEARCH_blacklist) / sizeof(GOOGLE_SEARCH_blacklist[0]))

#define NVIM_BLACKLIST_COUNT (sizeof(NVIM_blacklist) / sizeof(NVIM_blacklist[0]))

#define TIMER_BLACKLIST_COUNT (sizeof(TIMER_blacklist) / sizeof(TIMER_blacklist[0]))

#define TERMINAL_BLACKLIST_COUNT (sizeof(TERMINAL_blacklist) / sizeof(TERMINAL_blacklist[0]))

#define SCREENSHOT_BLACKLIST_COUNT (sizeof(SCREENSHOT_blacklist) / sizeof(SCREENSHOT_blacklist[0]))

#define YAZI_BLACKLIST_COUNT (sizeof(YAZI_blacklist) / sizeof(YAZI_blacklist[0]))

#define VIDEO_BLACKLIST_COUNT (sizeof(VIDEO_blacklist) / sizeof(VIDEO_blacklist[0]))

#define EXPLORER_BLACKLIST_COUNT (sizeof(EXPLORER_blacklist) / sizeof(EXPLORER_blacklist[0]))

#define TERMINAL_COMMAND_BLACKLIST_COUNT (sizeof(TERMINAL_COMMAND_blacklist) / sizeof(TERMINAL_COMMAND_blacklist[0]))

#define VOLUME_BLACKLIST_COUNT (sizeof(VOLUME_blacklist) / sizeof(VOLUME_blacklist[0]))

#define MUTE_BLACKLIST_COUNT (sizeof(MUTE_blacklist) / sizeof(MUTE_blacklist[0]))

#define REPLICA_BLACKLIST_COUNT (sizeof(REPLICA_blacklist) / sizeof(REPLICA_blacklist[0]))

#define WIFI_TOGGLE_BLACKLIST_COUNT (sizeof(WIFI_TOGGLE_blacklist) / sizeof(WIFI_TOGGLE_blacklist[0]))

#define BLUETOOTH_TOGGLE_BLACKLIST_COUNT (sizeof(BLUETOOTH_TOGGLE_blacklist) / sizeof(BLUETOOTH_TOGGLE_blacklist[0]))

#define MONITOR_FOCUS_MAIN_BLACKLIST_COUNT (sizeof(MONITOR_FOCUS_MAIN_blacklist) / sizeof(MONITOR_FOCUS_MAIN_blacklist[0]))

#define MONITOR_FOCUS_SIDE_BLACKLIST_COUNT (sizeof(MONITOR_FOCUS_SIDE_blacklist) / sizeof(MONITOR_FOCUS_SIDE_blacklist[0]))

#define SETTINGS_BLACKLIST_COUNT (sizeof(SETTINGS_blacklist) / sizeof(SETTINGS_blacklist[0]))

#define MONITOR_CONTROL_BLACKLIST_COUNT (sizeof(MONITOR_CONTROL_blacklist) / sizeof(MONITOR_CONTROL_blacklist[0]))

#define BINDING(key, mod, handler, enabled) \
    { key, mod, key##_blacklist, key##_BLACKLIST_COUNT, handler,enabled }

#define BINDING_CUSTOM(key, mod, blist, blist_count, handler, enabled) \
    { key, mod, blist, blist_count, handler, enabled }
#define BINDING_SIMPLE(key,mod,handler) \
    { key, mod, NULL, 0, handler, AlwaysEnabled }

extern HotkeyBinding G_HOTKEY_BINDINGS[36];

#define HOTKEY_BINDING_COUNT (sizeof(G_HOTKEY_BINDINGS) / sizeof(G_HOTKEY_BINDINGS[0]))

#endif
