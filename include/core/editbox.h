#ifndef CORE_EDITBOX_H
#define CORE_EDITBOX_H

#include <stdbool.h>

#include "core/types.h"
#include "core/types/window.h"

typedef int (*FlEditboxMeasureFn)(const char *text, int len, void *user);

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

  int sel_start;
  int sel_end;
  bool has_selection;
  EditboxSelMode sel_mode;

  bool insert_mode;

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
  int caret_w_normal;
  int caret_w_insert;

  const char *align;
  int placeholder_dy;
  int rainbow_limit_x;

  const char *placeholder;
  ColorRef text_color;
  FontHandle font;

  FlEditboxMeasureFn measure;
  void *measure_user;
} EditboxSpec;

typedef struct {
  int scroll;
  int draw_x;
  int cursor_draw_x;
  int block_x;
  int block_w;
  bool draw_block;
  int sel_x0;
  int sel_x1;
  bool draw_selection;
  int insert_x;
  bool draw_insert_caret;
} EditboxLayout;

typedef struct {
  PromptState *state;
  const char *text;
  int text_len;
  int cursor;
  bool insert_mode;
  int x;
  int y;
  int w;
  int h;
  int cursor_x;
  int text_w;
  int ascent;
  int descent;
  unsigned long text_color;
  FontHandle font;
} EditboxInit;

void Editbox_BuildSpec(EditboxSpec *out, const EditboxInit *in);
int Editbox_CaretY(int box_top, int ascent, int descent);
void Editbox_ResolveSelection(PromptState *state, int text_len,
                              bool prefer_state, int *out_start, int *out_end,
                              bool *out_has);
int Editbox_ScrollOffset(const EditboxSpec *spec);
void Editbox_Layout(EditboxSpec *spec, EditboxLayout *out);
void Editbox_Draw(const EditboxSpec *spec, const EditboxLayout *layout,
                  int parts);
void Editbox_Render(EditboxSpec *spec, EditboxLayout *out);

#endif
