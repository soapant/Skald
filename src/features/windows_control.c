#include "common.h"
#include "core/flapi.h"
#include "features/windows_control.h"
#include "helpers.h"
//
// Not Implemented on Linux - uses netsh (Windows only)

bool IsWifiEnabled(void) {
    FILE* fp = popen("netsh interface show interface", "r");
    if (!fp) return false;
    
    char line[256];
    bool found = false;
    bool enabled = false;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "Wi-Fi") || strstr(line, "Wireless")) {
            found = true;
            if (strstr(line, "Enabled") || strstr(line, "Connected")) {
                enabled = true;
            }
        }
    }
    
    pclose(fp);
    return found && enabled;
}

bool IsBluetoothEnabled(void) {
    FILE* fp = popen("netsh interface show interface", "r");
    if (!fp) return false;
    
    char line[256];
    bool found = false;
    bool enabled = false;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "Bluetooth") && strstr(line, "Network Connection")) {
            found = true;
            if (strstr(line, "Enabled") || strstr(line, "Connected")) {
                enabled = true;
            }
        }
    }
    
    pclose(fp);
    return found && enabled;
}

void TriggerWifiToggle(void) {
    bool currently_enabled = IsWifiEnabled();
    if (currently_enabled) {
        system("netsh interface set interface Wi-Fi admin=disabled");
        SubjectiveMessageDisplay("Wifi Turned OFF",140,TimeOutForMessages,COLOR_TEXT);
    } else {
        system("netsh interface set interface Wi-Fi admin=enabled");
        SubjectiveMessageDisplay("Wifi Turned ON",140,TimeOutForMessages,COLOR_TEXT);
    }

}

void TriggerBlueToothToggle(void) {
    bool currently_enabled = IsWifiEnabled();
    if (currently_enabled) {
        system("netsh interface set interface \"Bluetooth Network Connection\" admin=enabled");
        SubjectiveMessageDisplay("BlueTooth Turned OFF",140,TimeOutForMessages,COLOR_TEXT);
    } else {
        system("netsh interface set interface \"Bluetooth Network Connection\" admin=disabled");
        SubjectiveMessageDisplay("BlueTooth Turned ON",140,TimeOutForMessages,COLOR_TEXT);
    }

}

void windows_control_init(void) {
    fl_register_hotkey("Alt+Shift+W", TriggerWifiToggle, NULL, NULL);
    fl_register_hotkey("Alt+Shift+B", TriggerBlueToothToggle, NULL, NULL);
}
