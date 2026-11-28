#ifndef HOVER_WIDGET_H
#define HOVER_WIDGET_H

#include "core/types.h"

typedef struct {
  const char *key;
  void (*render)(void *ud);
  void *ud;
  void *owner_src;
  int x, y, w, h;
  int fps;
  bool no_bg;
  unsigned long bg_hex;
  void (*release)(void *ud);
  void *release_ud;
} HoverWidgetShowSpec;

void hover_widget_show(HoverWidgetShowSpec spec);
void hover_widget_hide(const char *key);
void hover_widget_purge_owner(const char *path_a, const char *path_b);

typedef struct {
  void (*render)(void *ud);
  void *render_data;
  int w;
  int h;
} HoverPopupCreateSpec;

typedef struct {
  void (*render)(void *ud);
  void *render_data;
  int w;
  int h;
  unsigned long frame;
  Timer created_at;
  int pad;
} HoverPopupCtx;

HoverPopupCtx *hover_popup_ctx_new(const HoverPopupCreateSpec *spec);
void hover_popup_frame(void *ud);
void hover_popup_ctx_free(void *ud);

typedef struct {
  const char *placeholder;
  const char *initial_text;
} TextPromptRenderData;

typedef struct {
  TextPromptRenderData *f_render_data;
  const char *f_placeholder;
  const char *f_initial_text;
  int f_x;
  int f_y;
  int f_w;
  int f_h;
} TextPromptSpec;

PromptSpec text_prompt_spec(TextPromptSpec *argspec);

#endif

