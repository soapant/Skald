#include <stdbool.h>
#ifndef WM_PLATFORM_H
#define WM_PLATFORM_H

#include "core/window_manager.h"
#include "core/types.h"

bool Platform_WM_InitDisplay(WindowManager *wm);
int Platform_WM_Run(WindowManager *wm);
void Platform_WM_Quit(WindowManager *wm);
void Platform_WM_ProcessEvents(WindowManager *wm);
void Platform_WM_OnPopEmpty(WindowManager *wm);
void Platform_WM_GetMonitorGeometry(int *x, int *y, int *w, int *h);

void Platform_ProcessPendingSubwindow(void);

void Platform_PromptFlushDisplay(WindowManager *wm);
bool Platform_PromptPopupShowWindow(Popup *self, int x, int y, int w, int h);
void Platform_PromptPopupHideWindow(Popup *self);
void Platform_PromptPopupRender(Popup *self);
void Platform_PromptPopupDestroyWindow(Popup *self);
bool Platform_PromptPopupOnKey(Popup *self, const PopupKeyEvent *key);
#endif
