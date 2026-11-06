#ifndef CONTEXT_H
#define CONTEXT_H

#include <stdbool.h>

#include "core/types.h"
#include "core/types/window.h"
#include "core/types/prompt.h"

typedef struct {
  WindowHandle hPopupWnd;
  bool selection_complete;
  bool editbox_placed;
  char output_buffer[1024];
  char *current_feature;
  const char *title;
  int last_index;
  PromptCallbacks prompt_callbacks;
} WindowContext;

typedef struct {
  WindowContext window;
} FlintContext;

FlintContext *FlintContext_Get(void);

#endif
