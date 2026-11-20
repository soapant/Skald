#ifndef SCROLL_AREA_H
#define SCROLL_AREA_H

#include <stdbool.h>

#include "helpers/helpers_owner.h"

typedef struct {
  const char *name;
  int x, y, w, h;
  int content_w, content_h;
  int *off_x;
  int *off_y;
  int wheel_px;
  const char *owner;
} ScrollAreaSpec;

bool scroll_area_begin(const ScrollAreaSpec *spec);
void scroll_area_end(void);
bool scroll_area_dispatch_wheel(int root_x, int root_y, int dx, int dy);
void scroll_area_purge_owner(const char *path_a, const char *path_b);

#endif
