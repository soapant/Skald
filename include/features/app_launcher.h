#ifndef APP_LAUNCHER_H
#define APP_LAUNCHER_H

void initialize_app_cache(void);
void launch_app_by_name(const char* display_name);
void launch_exe_with_args(const char* exe_path, const char* args);
void TriggerAppLauncher(void);

#endif
