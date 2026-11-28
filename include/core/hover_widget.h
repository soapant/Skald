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
} HoverWidgetShow;

void hover_widget_show(HoverWidgetShow spec);
void hover_widget_hide(const char *key);
void hover_widget_purge_owner(const char *path_a, const char *path_b);

typedef struct {
  void (*render)(void *ud);
  void *render_data;
  int w;
  int h;
} HoverPopupCreate;

typedef struct {
  void (*render)(void *ud);
  void *render_data;
  int w;
  int h;
  unsigned long frame;
  Timer created_at;
  int pad;
} HoverPopupCtx;

HoverPopupCtx *hover_popup_ctx_new(const HoverPopupCreate *spec);
void hover_popup_frame(void *ud);
void hover_popup_ctx_free(void *ud);

typedef struct {
  const char *placeholder;
  const char *initial_text;
} TextPromptRenderData;

typedef struct {
  TextPromptRenderData *render_data;
  const char *placeholder;
  const char *initial_text;
  int x;
  int y;
  int w;
  int h;
} TextPrompt;

PromptSpec text_prompt_spec(TextPrompt *argspec);

#endif


