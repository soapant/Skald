#ifndef HOVER_WIDGET_H
#define HOVER_WIDGET_H

#include "core/types.h"

typedef struct {
  const char *key;
  void (*render)(void *ud);
  void *ud;
  void *owner_src;
  int x, y, w, h;
  int fps;
  bool no_bg;
  unsigned long bg_hex;
  void (*release)(void *ud);
  void *release_ud;
} HoverWidgetShowSpec;

void hover_widget_show(HoverWidgetShowSpec spec);
void hover_widget_hide(const char *key);
void hover_widget_purge_owner(const char *path_a, const char *path_b);

#endif
