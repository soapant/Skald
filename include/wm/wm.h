#ifndef WINDOW_PLATFORM_H
#define WINDOW_PLATFORM_H

#include <stdbool.h>
#include <ctype.h>
#include <string.h>

char *wm_trim_focus_title(const char *title);

#include "core/types.h"
#include "core/types/window.h"

#include "helpers/helpers_text.h"

bool Wm_AppendWindowName(PlWindowList *list, const char *name,
                         bool toupper_first);

char *Wm_GetActiveWindowTitle(void);
char *Wm_GetActiveWindowAppId(void);
bool Wm_FocusByTitle(const char *title);
char *Wm_Paste(char *content);
PlWindowList Wm_GetWindowTitles(void);
PlWindowList Wm_GetWindowNames(void);
bool Wm_CloseActiveWindow(void);
bool Wm_ToggleFullscreenActive(void);
bool Wm_MoveActiveWindowToMonitor(bool right);
bool Wm_MoveActiveWindowLeft(void);
bool Wm_MoveActiveWindowUp(void);
bool Wm_MoveActiveWindowDown(void);
bool Wm_MoveActiveWindowRight(void);
bool Wm_FocusWindowLeft(void);
bool Wm_FocusWindowUp(void);
bool Wm_FocusWindowDown(void);
bool Wm_FocusWindowRight(void);
bool Wm_MinimizeActiveWindow(void);
bool Wm_InjectKey(int vk, int down);
bool Wm_GetMousePos(int *x, int *y);

#endif
