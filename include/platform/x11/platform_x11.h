#ifndef PLATFORM_X11_H
#define PLATFORM_X11_H

#include "core/types.h"

bool Platform_LaunchTerminalAtPath(const char* working_dir, const char* extra_args);
bool Platform_ExpandPath(const char* template_path, char* out, size_t out_size);
bool Platform_ShellExecuteWithVerb(const char* path, const char* verb, bool async);
bool Platform_LaunchApp(const char* path);
bool Platform_LaunchAppAsAdmin(const char* path);
void Platform_HandleShowCurrentTime(void);
bool Platform_SendX11Command(const char* window_title, const char* atom_name, long command);
bool Platform_SetX11Property(const char* window_title, const char* atom_name, long value);
bool Platform_SetX11StringProperty(const char* window_title, const char* atom_name, const char* value);
const char* Platform_GetSessionDisplay(void);
const char* Platform_GetSessionXAuthority(void);
const char* Platform_GetRuntimeDir(void);
const char* Platform_GetRealUser(void);
const char* Platform_GetRealHome(void);
bool Platform_IsRunningAsRoot(void);
void Platform_StartUniversalTimer(void);
void Platform_ResetUniversalTimer(void);
void Platform_ModifyUniversalTimer(double seconds_to_add);
double Platform_GetUniversalTimeSeconds(void);
void Platform_SaveRegionToPNG(int x1, int y1, int x2, int y2, const char* fullPath);
void Platform_WaitForKeypressAsync(void (*callback)(int));
int Platform_InitializeTrayIcon(void);
void Platform_RunHotKeyMessageLoop(void);
void Platform_InitUI(void);
void Platform_ShutdownUI(void);

#endif
