#ifndef MISC_HOTKEYS_H
#define MISC_HOTKEYS_H

void HandleKillMyProgram(void);
void HandleInitAppLauncherCache(void);
void HandleSettingsTrigger(void);
void RunMiscApplication(const char* path);
bool CheckMiscHotkeyBindings(int pressed_vk, int modifiers);
void misc_hotkeys_init(void);

#endif
