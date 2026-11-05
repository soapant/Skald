#include "core/context.h"

#include <stddef.h>

static FlintContext g_ctx = {
    .window =
        {
            .hPopupWnd = {0},
            .hDisplayMessage = {0},
            .selection_complete = false,
            .output_buffer = {0},
            .current_feature = NULL,
            .title = NULL,
            .last_index = 0,
        },
    .prompt_callbacks = {0},
    .item_index_map = NULL,
};

FlintContext *FlintContext_Get(void) { return &g_ctx; }
