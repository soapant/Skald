#ifndef CORE_CHECKBOX_H
#define CORE_CHECKBOX_H

#include <stdbool.h>

#include "core/types.h"
#include "core/types/color.h"
#include "core/types/window.h"

#define PLACE_CHECKBOX 0
#define PLACE_BUTTON 1

typedef struct {
  PlRect rect;
  const char *name;
  int kind;
  bool hovered;
  bool checked;
  bool clicked;
} PlaceState;

typedef struct {
  const char *name;
  char *text;
  int fontsize;
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

void Checkbox_FrameBegin(void);
bool Checkbox_IsChecked(const char *name, int kind);
bool Checkbox_IsHovered(const char *name, int kind);
bool Checkbox_IsClicked(const char *name, int kind);
void Checkbox_ResetAll(void);
bool Checkbox_Place(const CheckboxSpec *spec);
void Checkbox_Draw(const CheckboxSpec *spec);

#endif
