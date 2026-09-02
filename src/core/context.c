#include "core/context.h"
#include "core/modal.h"
#include <string.h>

static FlintContext g_ctx = {
    .vim_mode = MODE_INSERT,
    .vim_mode_str = "INSERT",
    .window = {
        .hPopupWnd = {0},
        .hListBox = {0},
        .hEditControl = {0},
        .hDisplayMessage = {0},
        .hSettings = {0},
        .g_ActiveEditControl = {0},
        .selection_complete = false,
        .output_buffer = {0},
        .current_feature = NULL,
        .title = NULL,
        .last_index = 0,
    },
    .prompt_callbacks = {0},
    .feature_history = NULL,
    .g_item_index_map = NULL,
    .history_indices = NULL,
    .g_timer = {0},
    .SelectionData = {0},
    .SelectionDataCount = 0,
#if defined(_WIN32)
    .g_main_thread_id = 0,
    .g_timeout = 1,
    .g_waiting_for_key = false,
    .g_key_pressed = 0,
    .g_key_callback = NULL,
    .draw_background = true,
#endif
};

FlintContext* FlintContext_Get(void) {
    return &g_ctx;
}
