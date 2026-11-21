#ifndef EDITBOX_H
#define EDITBOX_H

#define PROMPT_PRIMARY_EDITBOX_NAME "__prompt__"

#include <stdbool.h>

#include "core/types.h"
#include "core/types/window.h"
#include "platform/platform.h"

typedef int (*EditboxMeasureFn)(const char *text, int len, void *user);

#define EDITBOX_TIGHT_INSET 6
#define EDITBOX_TIGHT_RIGHT_PAD 4

int editbox_auto_width(const char *text, int font_size);

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

  const char *align;
  int placeholder_dy;

  const char *placeholder;
  ColorRef text_color;
  ColorRef bg_color;
  FontHandle font;
  int font_size;
  const Metrics *metrics;
  int caret_h;
  int caret_w;
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
  int font_size;
  int caret_h;
  int caret_w;
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
  int font_size;
} EditboxPlace;

typedef struct {
  ColorRef color_text;
  ColorRef color_bg;
  const char *placeholder;
  int caret_h;
  int caret_w;
  int caret_x;
  int caret_y;
  int caret_pos;
} EditboxUiSpec;

typedef struct {
  const char *buffer;
  int x, y, w, h;
  int font_size;
} EditboxPlaceSpec;

typedef struct {
  int text_w, cursor_x, ascent, descent;
  FontHandle font;
} EditboxMetrics;

void editbox_resolve_place(const EditboxPlaceSpec *spec, PromptState *state,
                           EditboxPlace *out);

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
  int font_size;
  FontHandle font;
  ColorRef text_color;
  ColorRef bg_color;
  const char *placeholder;
  int caret_h;
  int caret_w;
  int caret_x;
  int caret_y;
  int caret_pos;
} EditboxInitFromSpec;

void editbox_init_from_spec(EditboxInit *out, const EditboxInitFromSpec *args);

void editbox_build_spec(EditboxSpec *out, const EditboxInit *in);
void editbox_spec_from_place(EditboxSpec *out, const EditboxPlace *place,
                             PromptState *state, const EditboxUiSpec *ui,
                             const EditboxMetrics *metrics);
int editbox_caret_y(int box_top, int ascent, int descent, int font_size);
void editbox_spec_fill_from_prompt(EditboxSpec *spec, PromptState *state,
                                   int text_len, const EditboxMetrics *metrics);
void editbox_resolve_selection(PromptState *state, int text_len,
                               bool prefer_state, int *out_start, int *out_end,
                               bool *out_has);
int editbox_scroll_offset(const EditboxSpec *spec);
void editbox_layout(EditboxSpec *spec, EditboxLayout *out);
void editbox_draw(const EditboxSpec *spec, const EditboxLayout *layout,
                  int parts);
void editbox_render(EditboxSpec *spec, EditboxLayout *out);

void editbox_registry_begin(PromptState *state);
void editbox_registry_begin_frame(PromptState *state);
int editbox_registry_place(PromptState *state, const char *name, char *buffer,
                           size_t buffer_size, int x, int y, int w, int h);
PromptEditbox *editbox_registry_find(PromptState *state, const char *name);
void editbox_registry_tick(PromptState *state);
void editbox_registry_sync(PromptState *state);
void editbox_registry_set_active(PromptState *state, const char *name);
void editbox_registry_cycle(PromptState *state, int dir);
const char *editbox_registry_active_name(const PromptState *state);

#endif
