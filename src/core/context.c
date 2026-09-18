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
        .hActiveEditControl = {0},
        .selection_complete = false,
        .output_buffer = {0},
        .current_feature = NULL,
        .title = NULL,
        .last_index = 0,
    },
    .prompt_callbacks = {0},
    .feature_history = NULL,
    .item_index_map = NULL,
    .history_indices = NULL,
    .SelectionData = {0},
    .SelectionDataCount = 0,
#if defined(_WIN32)
    .main_thread_id = 0,
    .hMainFont = NULL,
    .hKeyboardHook = NULL,
    .g_timeout = 1,
    .g_waiting_for_key = false,
    .g_key_pressed = 0,
    .g_key_callback = NULL,
#endif
};

FlintContext* FlintContext_Get(void) {
    return &g_ctx;
}
