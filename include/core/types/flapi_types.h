#ifndef CORE_TYPES_FLAPI_TYPES_H
#define CORE_TYPES_FLAPI_TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include "core/types.h"

typedef struct { uint64_t ticks; } FlTimer;

typedef enum {
    FL_NET_OFFLINE,
    FL_NET_ETHERNET,
    FL_NET_WIFI,
} FlNetStatus;

typedef struct {
    const char *buffer;
    int x; int y; int w; int h;
    unsigned long color;
    FontHandle font;
} FlEditSpec;


typedef struct {
    int idx;
    bool selected;
    int x; int y; int w; int h;
    const char *text;
} FlListItemCtx;

#endif
