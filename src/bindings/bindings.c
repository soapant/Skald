#include "core/context.h"
#include "core/modal.h"
#include "bindings/bindings.h"
#include "bindings/vim_bindings.h"
#include "core/vim.h"
#include "core/window_manager.h"
#include "platform/ui.h"
#include "config.h"
#include "deps/log.h"
#include <ctype.h>

static const MotionBinding* g_bindings;
static int g_binding_count;
static const SequenceBinding* g_sequences;
static int g_sequence_count;

static bool vim_scope_allows(VimScope scope) {
    WindowManager* wm = WindowManager_Get();
    Popup* top = wm ? WindowManager_GetTopPopup(wm) : NULL;
    bool in_prompt = top && top->type == POPUP_PROMPT;
    bool in_edit = top && top->type == POPUP_EDIT;
    if (!in_prompt && !in_edit && IsPopupActive(WH_NULL)) in_prompt = true;
    switch (scope) {
        case VIM_SCOPE_ALL: return true;
        case VIM_SCOPE_MODAL: return in_prompt;
        case VIM_SCOPE_POPUP: return in_edit;
    }
    return true;
}

static bool sequence_in_scope(const SequenceBinding* b, EditorMode mode) {
    if (b->mode != mode) return false;
    return vim_scope_allows(b->scope);
}

static bool binding_in_scope(const MotionBinding* b, EditorMode mode) {
    if (b->mode != mode) return false;
    return vim_scope_allows(b->scope);
}

bool CheckVimMotion(int pressed_vk, int modifiers, VimMotion* out) {
    if (!EnableVimMode) return false;
    if (!out) return false;
    if (!g_sequences) {
        g_sequence_count = VimSequenceBindings_Get(&g_sequences);
        g_binding_count = VimBindings_Get(&g_bindings);
    }

    bool ctrl_held = (modifiers & HOTKEY_MOD_CTRL) != 0;
    bool shift_held = (modifiers & HOTKEY_MOD_SHIFT) != 0;

    char seq_char = tolower((char)pressed_vk);

    bool in_insert = IsInsertMode();
    bool in_normal = IsNormalMode();

    out->handler = NULL;
    out->sequence = NULL;
    out->typed_count = 0;

    if (in_insert && SequenceIsWaiting()) {
        if (SequenceTimedOut()) {
            log_info("CheckVimMotion insert timeout fallthrough key='%c'", seq_char);
            SequenceReset();
            return false;
        }

        SequenceAddKey(seq_char, false);
        SequenceStartTimer();

        for (int i = 0; i < g_sequence_count; i++) {
            const SequenceBinding* b = &g_sequences[i];
            if (!sequence_in_scope(b, MODE_INSERT)) continue;
            if (SequenceMatches(b->sequence)) {
                log_info("CheckVimMotion insert match seq='%s'", b->sequence);
                out->handler = b->handler;
                out->sequence = b->sequence;
                out->typed_count = SequenceTypedCount();
                SequenceReset();
                return true;
            }
        }

        for (int i = 0; i < g_sequence_count; i++) {
            const SequenceBinding* b = &g_sequences[i];
            if (!sequence_in_scope(b, MODE_INSERT)) continue;
            if (SequenceStartsWith(b->sequence)) return true;
        }

        SequenceReset();
        log_info("CheckVimMotion insert nomatch key='%c'", seq_char);
        return false;
    }

    if (in_insert) {
        if (shift_held) return false;

        bool is_prefix = false;
        for (int i = 0; i < g_sequence_count; i++) {
            const SequenceBinding* b = &g_sequences[i];
            if (!sequence_in_scope(b, MODE_INSERT)) continue;
            if (tolower(b->sequence[0]) == seq_char) { is_prefix = true; break; }
        }

        if (is_prefix) {
            SequenceAddKey(seq_char, true);
            SequenceStartTimer();
            log_info("CheckVimMotion insert seqstart key='%c'", seq_char);
            out->typed_count = SequenceTypedCount();
            return false;
        }

        SequenceReset();
        return false;
    }

    if (in_normal || IsVisualMode()) {
        if (SequenceTimedOut()) SequenceReset();

        if (pressed_vk == VK_RETURN || pressed_vk == 160) {
            SequenceReset();
            return false;
        }

        if (IsVisualMode()) {
            if (SequenceIsWaiting() && !SequenceTimedOut()) {
                SequenceAddKey(seq_char, false);
                for (int i = 0; i < g_sequence_count; i++) {
                    const SequenceBinding* b = &g_sequences[i];
                    if (!sequence_in_scope(b, MODE_VISUAL)) continue;
                    if (SequenceMatches(b->sequence)) {
                        out->handler = b->handler;
                        out->sequence = b->sequence;
                        out->typed_count = SequenceTypedCount();
                        SequenceReset();
                        return true;
                    }
                }
                for (int i = 0; i < g_sequence_count; i++) {
                    const SequenceBinding* b = &g_sequences[i];
if (!sequence_in_scope(b, MODE_VISUAL)) continue;
                    if (SequenceStartsWith(b->sequence)) return true;
                }
                SequenceReset();
            } else {
                SequenceReset();
            }

            if (!shift_held && !ctrl_held) {
                bool starts_seq = false;
                for (int i = 0; i < g_sequence_count; i++) {
                    const SequenceBinding* b = &g_sequences[i];
                    if (!sequence_in_scope(b, MODE_VISUAL)) continue;
                    if (tolower((unsigned char)b->sequence[0]) == seq_char) { starts_seq = true; break; }
                }
                if (starts_seq) {
                    SequenceAddKey(seq_char, false);
                    SequenceStartTimer();
                    return false;
                }
            }

            for (int i = 0; i < g_binding_count; i++) {
                const MotionBinding* b = &g_bindings[i];
                if (!binding_in_scope(b, MODE_VISUAL)) continue;
                if (b->vk_code == pressed_vk && b->modifiers == modifiers) {
                    out->handler = b->handler;
                    return true;
                }
            }
            return false;
        }

        if (!shift_held && !ctrl_held) {
            SequenceAddKey(seq_char, true);
            SequenceStartTimer();

            for (int i = 0; i < g_sequence_count; i++) {
                const SequenceBinding* b = &g_sequences[i];
                if (!sequence_in_scope(b, MODE_NORMAL)) continue;
                if (SequenceMatches(b->sequence)) {
                    out->handler = b->handler;
                    out->sequence = b->sequence;
                    out->typed_count = SequenceTypedCount();
                    SequenceReset();
                    return true;
                }
                if (SequenceStartsWith(b->sequence)) {
                    if (SequenceLength() == 1) {
                        for (int j = 0; j < g_binding_count; j++) {
                            const MotionBinding* mb = &g_bindings[j];
                            if (!binding_in_scope(mb, MODE_NORMAL)) continue;
                            if (mb->vk_code == pressed_vk && mb->modifiers == modifiers) {
                                out->handler = mb->handler;
                                break;
                            }
                        }
                    }
                    return true;
                }
            }
            SequenceReset();
        }

        for (int i = 0; i < g_binding_count; i++) {
            const MotionBinding* b = &g_bindings[i];
            if (!binding_in_scope(b, MODE_NORMAL)) continue;
            if (b->vk_code == pressed_vk && b->modifiers == modifiers) {
                out->handler = b->handler;
                return true;
            }
        }

        return false;
    }

    return false;
}

void ExecuteMotion(const VimMotion* motion) {
    if (!motion || !motion->handler) return;
    FlintContext* ctx = FlintContext_Get();
    if (motion->typed_count > 0 && motion->sequence && motion->sequence[0] != '\0' && IsInsertMode()) {
        DeleteLastTypedChar(ctx);
        if (ctx->prompt_callbacks.refresh) {
            ListBoxInfo* lb = Vim_GetListBox(ctx);
            ctx->prompt_callbacks.refresh(ctx->window.filter, lb);
        }
    }
    motion->handler(ctx);
}
