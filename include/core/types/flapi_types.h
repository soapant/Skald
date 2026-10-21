#ifndef CORE_TYPES_FLAPI_TYPES_H
#define CORE_TYPES_FLAPI_TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include "core/types.h"

// Data-only types shared between the flapi surface (core/flapi.h) and the
// platform-side flapi implementations (include/platform/flapi_platform.h).
// Keeping them here lets the platform header avoid depending on core/flapi.h.

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
    ListBoxInfo *lb;
    const char **items;
    int count;
    int x; int y; int w; int h;
    int item_height;
} FlListSpec;

typedef struct {
    int idx;
    bool selected;
    int x; int y; int w; int h;
    const char *text;
} FlListItemCtx;

#endif
