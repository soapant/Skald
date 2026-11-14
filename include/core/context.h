#ifndef CONTEXT_H
#define CONTEXT_H

#include <stdbool.h>

#include "core/types.h"
#include "core/types/window.h"
#include "core/types/prompt.h"

typedef struct {
  bool in_enter;
  bool pending;
  bool created;
  PromptSpec pending_subwindow;
} SubwindowInfo;

typedef struct {
  WindowHandle hPopupWnd;
  bool selection_complete;
  bool editbox_placed;
  char output_buffer[1024];
  char *current_feature;
  const char *title;
  PromptCallbacks prompt_callbacks;
  SubwindowInfo subwindow;
  long long last_popup_pop_ms;
} WindowContext;

typedef struct {
  WindowContext window;
} SkaldContext;

SkaldContext *SkaldContext_Get(void);
void skald_context_clear(void);

#endif
