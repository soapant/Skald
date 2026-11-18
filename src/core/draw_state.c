#include "core/draw_state.h"

#include <string.h>

static __thread DrawCanvas draw_state;

DrawCanvas *draw_state_get(void) { return &draw_state; }

void draw_state_reset(void) { memset(draw_state_get(), 0, sizeof(DrawCanvas)); }

void draw_state_begin_measure(void) {
  DrawCanvas *ds = draw_state_get();
  ds->measuring = true;
  ds->meas_x0 = 1 << 30;
  ds->meas_y0 = 1 << 30;
  ds->meas_x1 = -(1 << 30);
  ds->meas_y1 = -(1 << 30);
}

void draw_state_track_measure(int x, int y, int w, int h) {
  DrawCanvas *ds = draw_state_get();
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

void draw_state_end_measure(int *out_w, int *out_h) {
  DrawCanvas *ds = draw_state_get();
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

void draw_state_offset(int *x, int *y) {
  const DrawCanvas *d = draw_state_get();
  *x += d->off_x;
  *y += d->off_y;
}
