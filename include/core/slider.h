#ifndef SLIDER_H
#define SLIDER_H

#include <stdbool.h>

#include "core/types.h"
#include "core/types/color.h"
#include "core/types/window.h"

#define SLIDER_HORIZONTAL 0
#define SLIDER_VERTICAL 1

typedef struct {
  DrawRect rect;
  const char *name;
  int orientation;
  bool hovered;
  bool dragging;
  bool clicked;
  float value;
  float fraction;
} SliderState;

typedef struct {
  const char *name;
  int x;
  int y;
  int w;
  int h;
  int orientation;
  float min_value;
  float max_value;
  float step;
  float value;
  int thickness;
  ColorRef fill_color;
  ColorRef thumb_color;
  ColorRef bg_color;
  ColorRef hovered_color;
  ColorRef clicked_color;
  bool border;
  bool no_bg;
  bool hovered;
  bool dragging;
  bool clicked;
  bool was_down;
  void (*on_input)(float value);
  void (*on_change)(float value);
  void (*on_hover)(void);
  void (*render)(SliderState state);
} SliderSpec;

void slider_frame_begin(void);
bool slider_is_hovered(const char *name);
bool slider_is_clicked(const char *name);
bool slider_is_dragging(const char *name);
float slider_value(const char *name);
float slider_fraction(const char *name);
int slider_percent(const char *name);
bool slider_set_value(const char *name, float value);
void slider_reset_all(void);
void slider_draw(const SliderSpec *spec);
bool slider_place(const SliderSpec *spec);

#endif
