#ifndef WM_PLATFORM_H
#define WM_PLATFORM_H

#include <stdbool.h>

#include "core/window_manager.h"
#include "core/types.h"

bool Platform_InitDisplay(WindowManager *wm);
void Platform_OnPopEmpty(void);
void Platform_GetMonitorGeometry(int *x, int *y, int *w, int *h);

void Platform_PromptFlushDisplay(WindowManager *wm);
typedef struct {
  Popup *f_self;
  int f_x;
  int f_y;
  int f_w;
  int f_h;
} PlatformPromptPopupShowWindow;

bool Platform_PromptPopupShowWindow(PlatformPromptPopupShowWindow *argspec);
typedef struct {
  Popup *f_self;
  int f_x;
  int f_y;
  int f_w;
  int f_h;
} PlatformPromptPopupResize;

bool Platform_PromptPopupResize(PlatformPromptPopupResize *argspec);
void Platform_PromptPopupHideWindow(Popup *self);
void Platform_PromptPopupRender(Popup *self);
void Platform_PromptPopupDestroyWindow(Popup *self);
bool Platform_PromptPopupOnKey(const Popup *self, const PopupKeyEvent *key);
#endif

