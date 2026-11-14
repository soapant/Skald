#ifndef CORE_DRAW_STATE_H
#define CORE_DRAW_STATE_H

#include <stdbool.h>

typedef struct {
  int off_x;
  int off_y;
  unsigned long bg_override;
  bool transparent;
  bool measuring;
  int meas_x0;
  int meas_y0;
  int meas_x1;
  int meas_y1;
  int meas_origin_x;
  int meas_origin_y;
  bool win32_widget_surface;
  bool win32_listbox_surface;
} PlDrawState;

PlDrawState *pl_draw_state_get(void);
void pl_draw_state_offset(int *x, int *y);

void pl_draw_state_begin_measure(void);
void pl_draw_state_reset(void);
void pl_draw_state_track_measure(int x, int y, int w, int h);
void pl_draw_state_end_measure(int *out_w, int *out_h);

#endif
