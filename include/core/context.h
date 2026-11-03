#ifndef CONTEXT_H
#define CONTEXT_H

#include <stdbool.h>

#include "core/types.h"
#include "core/types/window.h"
#include "core/types/prompt.h"

typedef struct {
  char *key;
  void *value;
} HashMap;

typedef struct {
  WindowHandle hGlobalTrayWnd;
  WindowHandle hPopupWnd;
  WindowHandle hDisplayMessage;
  bool selection_complete;
  bool editbox_placed;
  char output_buffer[256];
  char filter[1024];
  int filter_cursor;
  char *current_feature;
  const char *title;
  int last_index;
  PromptCallbacks callbacks;
} WindowContext;

typedef struct {
  WindowContext window;
  PromptCallbacks prompt_callbacks;
  HashMap *feature_history;
  HashMap *item_index_map;
  HashMap *history_indices;
} FlintContext;

FlintContext *FlintContext_Get(void);

#endif
