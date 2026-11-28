#ifndef HELPERS_MONITOR_H
#define HELPERS_MONITOR_H

#include <stdbool.h>
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
typedef struct {
  int f_index;
  int *f_x;
  int *f_y;
  int *f_w;
  int *f_h;
} MonitorGeometryAt;

bool monitor_geometry_at(MonitorGeometryAt *argspec);
bool monitor_info(int index, MonitorInfo *out);

#endif

