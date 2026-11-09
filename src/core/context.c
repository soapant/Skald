#include "core/context.h"

#include <stddef.h>
#include <string.h>

static FlintContext g_ctx = {
    .window =
        {
            .hPopupWnd = {0},
            .selection_complete = false,
            .output_buffer = {0},
            .current_feature = NULL,
            .title = NULL,
            .prompt_callbacks = {0},
        },
};

FlintContext *FlintContext_Get(void) { return &g_ctx; }

void FlintContext_Clear(void) {
  FlintContext *ctx = FlintContext_Get();
  memset(&ctx->window, 0, sizeof(ctx->window));
}
