#ifndef CORE_TYPES_FLAPI_TYPES_H
#define CORE_TYPES_FLAPI_TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include "core/types.h"

typedef PlTimer FlTimer;

typedef enum {

  FL_NET_OFFLINE,

  FL_NET_ETHERNET,

  FL_NET_WIFI,
} FlNetStatus;

typedef struct {
  const char *buffer;
  int x;
  int y;
  int w;
  int h;
  const char *placeholder;
  ColorRef color_bg;
  ColorRef color_text;
  FontHandle font;
  int fontsize;
  int caret_height;
  int caret_width;
  int caret_x;
  int caret_y;
  int caret_pos;
} FlEditSpec;

typedef struct {
  int idx;
  bool selected;
  int x;
  int y;
  int w;
  int h;
  const char *text;
} FlListItemCtx;

#endif
