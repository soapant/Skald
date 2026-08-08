#include "common.h"
#include "bindings/hotkey_definitions.h"
#include "helpers.h"
#include "features/window_switcher.h"
#include "features/google_search.h"
#include "features/todo.h"
#include "features/calc.h"
#include "features/app_launcher.h"
#include "features/process_killer.h"
#include "features/yazi_zoxide.h"
#include "features/zoxide.h"
#include "features/timer.h"
#include "features/explorer.h"
#include "features/terminal_command.h"
#include "features/volume.h"
#include "features/monitor_focus.h"
#include "features/reminder.h"
#include "features/mute_microphone.h"
#include "features/forgetfull.h"
#include "features/replica.h"
#include "features/pixel_utils.h"
#include "features/screenshot.h"
#include "features/nvim_zoxide.h"
#include "features/windows_control.h"
#include "features/adb.h"

static const char* SWITCHER_blacklist[] = {"Fortnite"};
static const char* FORGETFULL_blacklist[] = {"Fortnite"};
static const char* REMINDER_blacklist[] = {""};
static const char* TODO_blacklist[] = {"Fortnite"};
static const char* POS_blacklist[] = {""};
static const char* COLOR_blacklist[] = {""};
static const char* PIXEL_EQUALS_blacklist[] = {""};
static const char* WAIT_FOR_PIXEL_blacklist[] = {""};
static const char* REGION_blacklist[] = {""};
static const char* CALCULATOR_blacklist[] = {""};
static const char* EXE_blacklist[] = {""};
static const char* KILL_PROCESS_blacklist[] = {"Fortnite"};
static const char* GOOGLE_SEARCH_blacklist[] = {"Fortnite",""};
static const char* NVIM_blacklist[] = {""};
static const char* TIMER_blacklist[] = {""};
static const char* TERMINAL_blacklist[] = {""};
static const char* SCREENSHOT_blacklist[] = {""};
static const char* YAZI_blacklist[] = {""};
static const char* VIDEO_blacklist[] = {""};
static const char* EXPLORER_blacklist[] = {""};
static const char* TERMINAL_COMMAND_blacklist[] = {""};
static const char* VOLUME_blacklist[] = {""};
static const char* MUTE_blacklist[] = {""};
static const char* REPLICA_blacklist[] = {""};
static const char* WIFI_TOGGLE_blacklist[] = {""};
static const char* BLUETOOTH_TOGGLE_blacklist[] = {""};
static const char* MONITOR_FOCUS_MAIN_blacklist[] = {"Fortnite","Nvim", "Neovide"};
static const char* MONITOR_FOCUS_SIDE_blacklist[] = {"Fortnite","Nvim", "Neovide"};
static const char* SETTINGS_blacklist[] = {""};

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

HotkeyBinding G_HOTKEY_BINDINGS[] = {
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

    BINDING_SIMPLE('M', HOTKEY_MOD_ALT_SHIFT,  mute_messenger_call),
    BINDING_SIMPLE('S', HOTKEY_MOD_ALT_SHIFT,  toggle_messenger_speaker),
    BINDING_SIMPLE('M', HOTKEY_MOD_ALT_CTRL,   HandleKillMyProgram),
    BINDING_SIMPLE('E', HOTKEY_MOD_ALT_CTRL,   HandleInitAppLauncherCache),
    BINDING_SIMPLE(TOGGLE_PIXEL_HOTKEYS_KEY,HOTKEY_MOD_SHIFT_CTRL,HandleSwapHotkeyMode),
};
