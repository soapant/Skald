#ifndef CHECKBOX_H
#define CHECKBOX_H

#include <stdbool.h>

#include "core/types.h"
#include "core/types/color.h"
#include "core/types/window.h"

#define PLACE_CHECKBOX 0
#define PLACE_BUTTON 1

typedef struct {
  DrawRect rect;
  const char *name;
  int kind;
  bool hovered;
  bool checked;
  bool clicked;
} PlaceState;

typedef struct {
  const char *name;
  char *text;
  int font_size;
  int x;
  int y;
  int w;
  int h;
  bool checked;
  bool button;
  bool checkbox;
  bool hovered;
  bool was_down;
  bool clicked;
  bool focused;
  int thickness;
  ColorRef text_color;
  ColorRef hovered_color;
  ColorRef clicked_color;
  ColorRef bg_color;
  bool border;
  bool no_bg;
  void (*on_click)(void);
  void (*on_hover)(void);
  void (*render)(PlaceState state);
} CheckboxSpec;

void checkbox_frame_begin(void);
bool checkbox_is_checked(const char *name, int kind);
bool checkbox_is_hovered(const char *name, int kind);
bool checkbox_is_clicked(const char *name, int kind);
void checkbox_reset_all(void);
bool checkbox_place(const CheckboxSpec *spec);
void checkbox_draw(const CheckboxSpec *spec);

#endif
