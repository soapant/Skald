#ifndef WIDGET_H
#define WIDGET_H

#include <stdbool.h>
#include "helpers/helpers_render_thread.h"
#include "core/types/color.h"
#include "config.h"

typedef struct PlatformWidgetBase {
  bool dead;
  int fps;
  bool dirty;
  long long last_draw_ms;
  RenderFrameFn fn;
  void *ud;
  int x, y, w, h;
  bool mapped;
  void (*on_click)(void);
  ColorRef bg_color;
  bool has_bg;
  bool no_bg;
  char name[64];
} PlatformWidgetBase;

typedef struct PlatformWidget PlatformWidget;

PlatformWidget *Platform_Widget_Create(int x, int y, int w, int h, int fps,
                                       bool no_bg, void (*on_click)(void),
                                       const char *name);
void Platform_Widget_Destroy(PlatformWidget *widget);
void Platform_Widget_Show(PlatformWidget *widget);
void Platform_Widget_Hide(PlatformWidget *widget);
void Platform_Widget_Move(PlatformWidget *widget, int x, int y);
void Platform_Widget_Resize(PlatformWidget *widget, int w, int h);
void Platform_Widget_SetBgColor(PlatformWidget *widget, ColorRef bg_color);
void Platform_Widget_SetNoBg(PlatformWidget *widget, bool no_bg);
bool Platform_Widget_DispatchClick(long window_id);
void Platform_Widget_DrainClicks(void);
void Platform_Widget_FlushDestruction(void);

#endif
