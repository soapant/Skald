#ifndef EDITBOX_H
#define EDITBOX_H

#include <stdbool.h>

#include "core/types.h"
#include "core/types/window.h"

typedef int (*EditboxMeasureFn)(const char *text, int len, void *user);

typedef enum {
  EDITBOX_SEL_EXCLUSIVE = 0,
  EDITBOX_SEL_INCLUSIVE = 1,
  EDITBOX_SEL_INCLUSIVE_CARET = 2
} EditboxSelMode;

#define EDITBOX_PART_TINT 1
#define EDITBOX_PART_TEXT 2
#define EDITBOX_PART_CARET 4
#define EDITBOX_PART_ALL                                                       \
  (EDITBOX_PART_TINT | EDITBOX_PART_TEXT | EDITBOX_PART_CARET)

typedef struct {
  const char *text;
  int text_len;
  int cursor;
  bool password_mode;

  int sel_start;
  int sel_end;
  bool has_selection;
  EditboxSelMode sel_mode;

  int x;
  int y;
  int w;
  int inset;
  int visible_w;
  int scroll;

  int cursor_x;
  int text_w;

  int text_y;
  int caret_y;
  int caret_h;

  const char *align;
  int placeholder_dy;
  int rainbow_limit_x;

  const char *placeholder;
  ColorRef text_color;
  ColorRef bg_color;
  FontHandle font;
  int fontsize;
  int caret_height;
  int caret_width;
  int caret_x;
  int caret_pos;

  EditboxMeasureFn measure;
  void *measure_user;
} EditboxSpec;

typedef struct {
  int scroll;
  int draw_x;
  int cursor_draw_x;
  int sel_x0;
  int sel_x1;
  bool draw_selection;
  int caret_x;
  int caret_w;
} EditboxLayout;

typedef struct {
  PromptState *state;
  const char *text;
  int text_len;
  int cursor;
  int x;
  int y;
  int w;
  int h;
  int cursor_x;
  int text_w;
  int ascent;
  int descent;
  unsigned long text_color;
  unsigned long bg_color;
  const char *placeholder;
  FontHandle font;
  int fontsize;
  int caret_height;
  int caret_width;
  int caret_x;
  int caret_y;
  int caret_pos;
} EditboxInit;

typedef struct {
  PromptState *state;
  const char *buf;
  int text_len;
  int cursor;
  int x;
  int y;
  int w;
  int h;
  int y_raw;
  int fontsize;
} EditboxPlace;

typedef struct {
  ColorRef color_text;
  ColorRef color_bg;
  const char *placeholder;
  int caret_height;
  int caret_width;
  int caret_x;
  int caret_y;
  int caret_pos;
} EditboxUiSpec;

void editbox_resolve_place(const char *buffer, int x, int y, int w, int h,
                          int fontsize, PromptState *state, EditboxPlace *out);

typedef struct {
  PromptState *state;
  const char *buf;
  int text_len;
  int cursor;
  int x;
  int y;
  int w;
  int h;
  int cursor_x;
  int text_w;
  int ascent;
  int descent;
  int fontsize;
  FontHandle font;
  ColorRef text_color;
  ColorRef bg_color;
  const char *placeholder;
  int caret_height;
  int caret_width;
  int caret_x;
  int caret_y;
  int caret_pos;
} EditboxInitFromSpec;

void editbox_init_from_spec(EditboxInit *out, const EditboxInitFromSpec *args);

void editbox_build_spec(EditboxSpec *out, const EditboxInit *in);
void editbox_spec_from_place(EditboxSpec *out, const EditboxPlace *place,
                             PromptState *state, const EditboxUiSpec *ui,
                             int text_w, int cursor_x, int ascent, int descent,
                             FontHandle font);
int editbox_caret_y(int box_top, int ascent, int descent, int fontsize);
void editbox_spec_fill_from_prompt(EditboxSpec *spec, PromptState *state,
                                int text_len, int cursor_x, int text_w,
                                int ascent, int descent, FontHandle font);
void editbox_resolve_selection(PromptState *state, int text_len,
                              bool prefer_state, int *out_start, int *out_end,
                              bool *out_has);
int editbox_scroll_offset(const EditboxSpec *spec);
void editbox_layout(EditboxSpec *spec, EditboxLayout *out);
void editbox_draw(const EditboxSpec *spec, const EditboxLayout *layout,
                  int parts);
void editbox_render(EditboxSpec *spec, EditboxLayout *out);

#endif
