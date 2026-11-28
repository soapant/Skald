#include <stdbool.h>
#include <stdlib.h>
#include "platform/widget.h"
#include "core/types/color.h"

PlatformWidget *Platform_Widget_Create(FnPlatformWidgetCreateSpec *argspec) {
  (void)argspec->f_x;
  (void)argspec->f_y;
  (void)argspec->f_w;
  (void)argspec->f_h;
  (void)argspec->f_fps;
  (void)argspec->f_no_bg;
  (void)argspec->f_on_click;
  (void)argspec->f_name;
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
