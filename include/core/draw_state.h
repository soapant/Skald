#ifndef DRAW_STATE_H
#define DRAW_STATE_H

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
  bool listbox_rows;
  bool suppress_separators;
  bool rendering;
  bool repaint_pending;
  int clip_y0;
  int clip_y1;
} DrawCanvas;

DrawCanvas *draw_state_get(void);
void draw_state_offset(int *x, int *y);
bool draw_state_begin_render(void);
bool draw_state_end_render(void);

void draw_state_set_clip(int y0, int y1);
void draw_state_clear_clip(void);
void draw_state_begin_measure(void);
void draw_state_track_measure(int x, int y, int w, int h);
void draw_state_end_measure(int *out_w, int *out_h);

#endif
