#ifndef WM_PLATFORM_H
#define WM_PLATFORM_H

#include "core/window_manager.h"
#include "core/types.h"

bool Platform_WatcherStart(WindowManager* wm);
void Platform_WatcherStop(WindowManager* wm);

bool Platform_WM_InitDisplay(WindowManager* wm);
int Platform_WM_Run(WindowManager* wm);
void Platform_WM_Quit(WindowManager* wm);
void Platform_WM_ProcessEvents(WindowManager* wm);
void Platform_WM_OnPopEmpty(WindowManager* wm);
void Platform_WM_GetMonitorGeometry(int *x, int *y, int *w, int *h);
char* Platform_PromptUser(WindowManager* wm, const PromptSpec* spec);

bool Platform_EditPopupCreateWindow(Popup* self, int x, int y, int w, int h, const char* title);
void Platform_EditPopupShowWindow(Popup* self);
void Platform_EditPopupHideWindow(Popup* self);
void Platform_EditPopupRender(Popup* self);
void Platform_EditPopupDestroyWindow(Popup* self);

bool Platform_PromptPopupShowWindow(Popup* self, int x, int y, int w, int h);
void Platform_PromptPopupHideWindow(Popup* self);
void Platform_PromptPopupRender(Popup* self);
void Platform_PromptPopupDestroyWindow(Popup* self);

#endif
