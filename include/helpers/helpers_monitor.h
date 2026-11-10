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

int MonitorFill(MonitorEntry *out, int max_entries);

int Monitor_Count(void);
bool Monitor_GeometryAt(int index, int *x, int *y, int *w, int *h);
bool Monitor_Info(int index, MonitorInfo *out);

#endif
