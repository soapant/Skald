#include "core/flapi.h"
#include "config.h"
#include "core/platform.h"
#include <string.h>

bool IsWifiEnabled(void) {
    char* out = fl_run_terminal_command("netsh interface show interface");
    if (!out) return false;
    bool found = strstr(out, "Wi-Fi") || strstr(out, "Wireless");
    bool enabled = strstr(out, "Enabled") || strstr(out, "Connected");
    return found && enabled;
}

bool IsBluetoothEnabled(void) {
    char* out = fl_run_terminal_command("netsh interface show interface");
    if (!out) return false;
    bool found = strstr(out, "Bluetooth") && strstr(out, "Network Connection");
    bool enabled = strstr(out, "Enabled") || strstr(out, "Connected");
    return found && enabled;
}

void TriggerWifiToggle(void) {
    bool currently_enabled = IsWifiEnabled();
    if (currently_enabled) {
        fl_run_terminal_command("netsh interface set interface Wi-Fi admin=disabled");
        SubjectiveMessageDisplay("Wifi Turned OFF",140,TimeOutForMessages,COLOR_TEXT);
    } else {
        fl_run_terminal_command("netsh interface set interface Wi-Fi admin=enabled");
        SubjectiveMessageDisplay("Wifi Turned ON",140,TimeOutForMessages,COLOR_TEXT);
    }
}

void TriggerBlueToothToggle(void) {
    bool currently_enabled = IsWifiEnabled();
    if (currently_enabled) {
        fl_run_terminal_command("netsh interface set interface \"Bluetooth Network Connection\" admin=enabled");
        SubjectiveMessageDisplay("BlueTooth Turned OFF",140,TimeOutForMessages,COLOR_TEXT);
    } else {
        fl_run_terminal_command("netsh interface set interface \"Bluetooth Network Connection\" admin=disabled");
        SubjectiveMessageDisplay("BlueTooth Turned ON",140,TimeOutForMessages,COLOR_TEXT);
    }
}

FLINT_FEATURE_INIT {
    // temporary maybe
    // fl_register_hotkey("Alt+Shift+W", TriggerWifiToggle);
    // fl_register_hotkey("Alt+Shift+B", TriggerBlueToothToggle);
}
