#include <stdbool.h>
#ifndef WM_PLATFORM_H
#define WM_PLATFORM_H

#include "core/window_manager.h"
#include "core/types.h"

bool Platform_InitDisplay(WindowManager *wm);
void Platform_OnPopEmpty(void);
void Platform_GetMonitorGeometry(int *x, int *y, int *w, int *h);

void Platform_PromptFlushDisplay(WindowManager *wm);
bool Platform_PromptPopupShowWindow(Popup *self, int x, int y, int w, int h);
void Platform_PromptPopupHideWindow(Popup *self);
void Platform_PromptPopupRender(Popup *self);
void Platform_PromptPopupDestroyWindow(Popup *self);
bool Platform_PromptPopupOnKey(Popup *self, const PopupKeyEvent *key);
#endif
