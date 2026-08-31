#include "core/flapi.h"
#include "core/platform.h"
#include "features/process_killer.h"
#include "features/app_launcher.h"

void HandleKillMyProgram(void) {
    terminate_process_by_name("flint.exe");
    terminate_process_by_name("main.exe");
}

void HandleInitAppLauncherCache(void) {
    initialize_app_cache();
}

void HandleSettingsTrigger(void) {
    Platform_SettingsPopUp();
}

void misc_hotkeys_init(void) {
    fl_register_hotkey("Alt+Shift+K", HandleKillMyProgram, NULL, NULL);
    fl_register_hotkey("Alt+Shift+E", HandleInitAppLauncherCache, NULL, NULL);
    fl_register_hotkey("Alt+Shift+T", Platform_HandleShowCurrentTime, NULL, NULL);
    fl_register_hotkey("Alt+D", HandleSettingsTrigger, NULL, NULL);
}
