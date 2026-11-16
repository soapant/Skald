#include <stdbool.h>
#include <stdlib.h>
#include "platform/widget.h"
#include "core/types/color.h"

PlatformWidget *Platform_Widget_Create(int x, int y, int w, int h, int fps,
                                       bool no_bg, void (*on_click)(void),
                                       const char *name) {
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  (void)fps;
  (void)no_bg;
  (void)on_click;
  (void)name;
  return NULL;
}

void Platform_Widget_Destroy(PlatformWidget *widget) { (void)widget; }

void Platform_Widget_Show(PlatformWidget *widget) { (void)widget; }

void Platform_Widget_Hide(PlatformWidget *widget) { (void)widget; }

void Platform_Widget_Move(PlatformWidget *widget, int x, int y) {
  (void)widget;
  (void)x;
  (void)y;
}

void Platform_Widget_Resize(PlatformWidget *widget, int w, int h) {
  (void)widget;
  (void)w;
  (void)h;
}

void Platform_Widget_SetBgColor(PlatformWidget *widget, ColorRef bg_color) {
  (void)widget;
  (void)bg_color;
}

void Platform_Widget_SetNoBg(PlatformWidget *widget, bool no_bg) {
  (void)widget;
  (void)no_bg;
}

bool Platform_Widget_DispatchClick(long window_id) {
  (void)window_id;
  return false;
}

void Platform_Widget_DrainClicks(void) {}

void Platform_Widget_FlushDestruction(void) {}
