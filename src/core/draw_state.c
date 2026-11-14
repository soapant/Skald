#include "core/draw_state.h"

#include <string.h>

static __thread PlDrawState draw_state;

PlDrawState *PlDrawState_Get(void) { return &draw_state; }

void PlDrawState_Reset(void) {
  memset(PlDrawState_Get(), 0, sizeof(PlDrawState));
}

void PlDrawState_BeginMeasure(void) {
  PlDrawState *ds = PlDrawState_Get();
  ds->measuring = true;
  ds->meas_x0 = 1 << 30;
  ds->meas_y0 = 1 << 30;
  ds->meas_x1 = -(1 << 30);
  ds->meas_y1 = -(1 << 30);
}

void PlDrawState_TrackMeasure(int x, int y, int w, int h) {
  PlDrawState *ds = PlDrawState_Get();
  if (!ds->measuring) {
    return;
  }
  if (x < ds->meas_x0)
    ds->meas_x0 = x;
  if (y < ds->meas_y0)
    ds->meas_y0 = y;
  if (x + w > ds->meas_x1)
    ds->meas_x1 = x + w;
  if (y + h > ds->meas_y1)
    ds->meas_y1 = y + h;
}

void PlDrawState_EndMeasure(int *out_w, int *out_h) {
  PlDrawState *ds = PlDrawState_Get();
  int rw = 0, rh = 0;
  if (ds->meas_x1 > ds->meas_x0)
    rw = ds->meas_x1 - ds->meas_x0;
  if (ds->meas_y1 > ds->meas_y0)
    rh = ds->meas_y1 - ds->meas_y0;
  if (out_w)
    *out_w = rw;
  if (out_h)
    *out_h = rh;
  ds->meas_origin_x = (rw > 0 || rh > 0) ? ds->meas_x0 : 0;
  ds->meas_origin_y = (rw > 0 || rh > 0) ? ds->meas_y0 : 0;
  ds->measuring = false;
  ds->meas_x0 = 0;
  ds->meas_y0 = 0;
  ds->meas_x1 = 0;
  ds->meas_y1 = 0;
}

void PlDrawState_Offset(int *x, int *y) {
  const PlDrawState *d = PlDrawState_Get();
  *x += d->off_x;
  *y += d->off_y;
}
