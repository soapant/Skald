#ifndef CORE_HOVER_WIDGET_H
#define CORE_HOVER_WIDGET_H

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
} PlHoverWidgetShowSpec;

void HoverWidgetShow(PlHoverWidgetShowSpec spec);
void HoverWidgetHide(const char *key);
void HoverWidget_PurgeOwner(const char *path_a, const char *path_b);

#endif
