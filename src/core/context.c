#include "core/context.h"

#include <stdbool.h>
#include <stddef.h>
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
