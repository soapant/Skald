#include "core/flapi.h"
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
        fl_message(.message="Wifi Turned OFF");
    } else {
        fl_run_terminal_command("netsh interface set interface Wi-Fi admin=enabled");
        fl_message(.message="Wifi Turned ON");
    }
}

void TriggerBlueToothToggle(void) {
    bool currently_enabled = IsWifiEnabled();
    if (currently_enabled) {
        fl_run_terminal_command("netsh interface set interface \"Bluetooth Network Connection\" admin=enabled");
        fl_message(.message="BlueTooth Turned OFF");
    } else {
        fl_run_terminal_command("netsh interface set interface \"Bluetooth Network Connection\" admin=disabled");
        fl_message(.message="BlueTooth Turned ON");
    }
}

FLINT_FEATURE_INIT {
    // temporary maybe
    // fl_register_hotkey("Alt+Shift+W", TriggerWifiToggle);
    // fl_register_hotkey("Alt+Shift+B", TriggerBlueToothToggle);
}
