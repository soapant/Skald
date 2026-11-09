#ifndef CORE_TYPES_SKAPI_TYPES_H
#define CORE_TYPES_SKAPI_TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include "core/types.h"

typedef PlTimer SkTimer;

typedef enum {

  SK_NET_OFFLINE,

  SK_NET_ETHERNET,

  SK_NET_WIFI,
} SkNetStatus;

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
} SkEditSpec;

typedef struct {
  int idx;
  bool selected;
  int x;
  int y;
  int w;
  int h;
  const char *text;
} SkListItemCtx;

#endif
