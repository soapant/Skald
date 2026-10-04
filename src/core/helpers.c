#include "core/context.h"
#include "core/modal.h"
#include "core/timer.h"
#include "config.h"
#include "config.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_ui.h"
#include "platform/ui.h"
#include "platform/platform.h"
#include "platform/wm_platform.h"
#include "core/window_manager.h"

int MessagePromptGap = MSG_PROMPT_GAP_DEFAULT;
int MessageScreenGap = MSG_SCREEN_GAP_DEFAULT;

unsigned long VimModeTextColor(void) {
    PROFILE_START(timer);
    if (g_current_mode == MODE_NORMAL) { PROFILE_END_VAR(timer, "helpers:VimModeTextColor"); return rgb_to_hex(COLOR_NORMAL_MODE_TEXT); }
    if (g_current_mode == MODE_INSERT) { PROFILE_END_VAR(timer, "helpers:VimModeTextColor"); return rgb_to_hex(COLOR_INSERT_MODE_TEXT); }
    PROFILE_END_VAR(timer, "helpers:VimModeTextColor");
    return rgb_to_hex(COLOR_VISUAL_MODE_TEXT);
}

void MessageSlotToRect(MessageSlot slot, int prompt_gap, int* out_x, int* out_y, int* out_w, int* out_h, int* out_h_slot, int* out_v_slot, int* out_gap, int* out_outward) {
    int h_slot = 1;
    int v_slot = 1;
    int gap = MessageScreenGap;
    int outward = 0;
    int x = 0, y = 0, w = 0, h = 0;
    int mox = 0, moy = 0, mow = 0, moh = 0;
    WindowManager_GetMainMonitorGeometry(&mox, &moy, &mow, &moh);
    if (mow <= 0) mow = 1920;
    if (moh <= 0) moh = 1080;
    if (slot >= TOP_LEFT_RELATIVE && slot <= BOTTOM_RIGHT_RELATIVE) {
        if (prompt_gap > 0) gap = prompt_gap;
        outward = 1;
        WindowManager_GetPromptBounds(&x, &y, &w, &h);
        if (w <= 0 || h <= 0) { x = mox; y = moy; w = mow; h = moh; }
        slot = (MessageSlot)(slot - TOP_LEFT_RELATIVE + TOP_LEFT);
    } else {
        x = mox;
        y = moy;
        w = mow;
        h = moh;
    }
    if (slot >= TOP_LEFT && slot <= BOTTOM_RIGHT) {
        int idx = slot - TOP_LEFT;
        h_slot = idx % 3;
        v_slot = idx / 3;
    }
    if (out_x) *out_x = x;
    if (out_y) *out_y = y;
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
    if (out_h_slot) *out_h_slot = h_slot;
    if (out_v_slot) *out_v_slot = v_slot;
    if (out_gap) *out_gap = gap;
    if (out_outward) *out_outward = outward;
}

void DisplayMessageSlot(const char* message, MessageSlot slot, int max_w, int max_h, int prompt_gap, int timeout, ColorRef color) {
    if (!message || !message[0]) return;
    int x = 0, y = 0, w = 0, h = 0, h_slot = 1, v_slot = 1, gap = 0, outward = 0;
    MessageSlotToRect(slot, prompt_gap, &x, &y, &w, &h, &h_slot, &v_slot, &gap, &outward);
    Platform_DisplayMessageAnchored(message, x, y, w, h, h_slot, v_slot, gap, outward, max_w, max_h, timeout, color);
}

EditorMode g_last_mode = MODE_INSERT;
