#ifndef WINDOW_PLATFORM_H
#define WINDOW_PLATFORM_H

#include <stdbool.h>
#include "core/types.h"


char* Wm_GetActiveWindowTitle();
char* Wm_GetActiveWindowAppId();
bool Wm_FocusByTitle(const char *title);
char* Wm_Paste(char* content);
PlWindowList Wm_GetWindowTitles(void);
PlWindowList Wm_GetWindowNames(void);
bool Wm_CloseActiveWindow(void);
bool Wm_ToggleFullscreenActive(void);
bool Wm_MoveActiveWindowToMonitor(bool right);
bool Wm_MoveActiveWindowLeft(void);
bool Wm_MoveActiveWindowRight(void);
bool Wm_FocusWindowLeft(void);
bool Wm_FocusWindowRight(void);
bool Wm_MinimizeActiveWindow(void);
bool Wm_ScreenshotCopyArea(void);
bool Wm_ScreenshotCopy(void);
bool Wm_ScreenshotSaveArea(char* save_path);
bool Wm_ScreenshotSave(char* save_path);
char* Wm_OcrArea(void);

#endif
