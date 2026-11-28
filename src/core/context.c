#include "core/context.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static const PromptCallbacks zero_callbacks = {0};

static SkaldContext ctx = {
    .window =
        {
            .hPopupWnd = {0},
            .selection_complete = false,
            .output_buffer = {0},
            .current_feature = NULL,
            .title = NULL,
            .prompt_callbacks = &zero_callbacks,
            .last_popup_pop_ms = 0,
        },
};

SkaldContext *SkaldContext_Get(void) { return &ctx; }

void window_set_current_feature(const char *name) {
  if (ctx.window.current_feature_owned)
    free(ctx.window.current_feature);
  ctx.window.current_feature = NULL;
  ctx.window.current_feature_owned = false;
  if (!name)
    return;
  size_t n = strlen(name);
  char *copy = (char *)malloc(n + 1);
  if (!copy)
    return;
  memcpy(copy, name, n + 1);
  ctx.window.current_feature = copy;
  ctx.window.current_feature_owned = true;
}
