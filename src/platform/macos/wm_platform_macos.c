#include <stdbool.h>
#include "platform/wm_platform.h"
#include "core/types/window.h"

bool Platform_InitDisplay(WindowManager *wm) {
  (void)wm;
  return false;
}

void Platform_OnPopEmpty(void) {}

void Platform_GetMonitorGeometry(int *x, int *y, int *w, int *h) {
  if (x)
    *x = 0;
  if (y)
    *y = 0;
  if (w)
    *w = 0;
  if (h)
    *h = 0;
}

void Platform_PromptFlushDisplay(WindowManager *wm) { (void)wm; }

bool Platform_PromptPopupShowWindow(Popup *self, int x, int y, int w, int h) {
  (void)self;
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  return false;
}

void Platform_PromptPopupHideWindow(Popup *self) { (void)self; }

void Platform_PromptPopupRender(Popup *self) { (void)self; }

void Platform_PromptPopupDestroyWindow(Popup *self) { (void)self; }

bool Platform_PromptPopupOnKey(const Popup *self, const PopupKeyEvent *key) {
  (void)self;
  (void)key;
  return false;
}
