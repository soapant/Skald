#include <stdbool.h>

#ifndef HELPERS_MONITOR_H
#define HELPERS_MONITOR_H

#include "core/types.h"

typedef struct {
  int x, y, w, h;
  bool is_primary;
  char name[128];
} MonitorEntry;

typedef struct {
  int index;
  int x, y, w, h;
  bool is_primary;
  char name[128];
} MonitorInfo;

int monitor_fill(MonitorEntry *out, int max_entries);
int monitor_count(void);
bool monitor_geometry_at(int index, int *x, int *y, int *w, int *h);
bool monitor_info(int index, MonitorInfo *out);

#endif
