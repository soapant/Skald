#include <string.h>
#include <stdio.h>

#include "helpers/helpers_monitor.h"

int Monitor_Count(void) {
  MonitorEntry mon[16];
  return Monitor_Fill(mon, 16);
}

bool Monitor_GeometryAt(int index, int *x, int *y, int *w, int *h) {
  MonitorEntry mon[16];
  int n = Monitor_Fill(mon, 16);
  if (n <= 0 || index < 0 || index >= n || !mon[index].w)
    return false;
  if (x)
    *x = mon[index].x;
  if (y)
    *y = mon[index].y;
  if (w)
    *w = mon[index].w;
  if (h)
    *h = mon[index].h;
  return true;
}

bool Monitor_Info(int index, MonitorInfo *out) {
  MonitorEntry mon[16];
  int n = Monitor_Fill(mon, 16);
  if (n <= 0 || index < 0 || index >= n || !mon[index].w || !out)
    return false;
  memset(out, 0, sizeof(*out));
  out->index = index;
  out->x = mon[index].x;
  out->y = mon[index].y;
  out->w = mon[index].w;
  out->h = mon[index].h;
  out->is_primary = mon[index].is_primary;
  snprintf(out->name, sizeof(out->name), "%s", mon[index].name);
  return true;
}
