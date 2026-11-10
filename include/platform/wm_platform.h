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
typedef struct {
  int index;
  char name[128];
  int x, y, w, h;
  bool is_primary;
} WMMonitorInfo;

int Platform_WM_GetMonitorCount(void);
bool Platform_WM_GetMonitorGeometryAt(int index, int *x, int *y, int *w, int *h);
bool Platform_WM_GetMonitorInfo(int index, WMMonitorInfo *out);
char *Platform_PromptUser(WindowManager *wm, const PromptSpec *spec);
void Platform_ProcessPendingSubwindow(void);

bool Platform_PromptPopupShowWindow(Popup *self, int x, int y, int w, int h);
void Platform_PromptPopupHideWindow(Popup *self);
void Platform_PromptPopupRender(Popup *self);
void Platform_PromptPopupDestroyWindow(Popup *self);
bool Platform_PromptPopupOnKey(Popup *self, const PopupKeyEvent *key);
#endif
