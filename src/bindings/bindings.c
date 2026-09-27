#include "core/context.h"
#include "core/modal.h"
#include "bindings/bindings.h"
#include "bindings/vim_bindings.h"
#include "core/modal.h"
#include "core/vim.h"
#include "config.h"
#include "deps/log.h"
#include <ctype.h>

bool CheckVimMotion(int pressed_vk, int modifiers, MotionAction* out_action, const char** out_sequence, int* out_typed_count) {
    if (!EnableVimMode) return false;
    bool ctrl_held = (modifiers & HOTKEY_MOD_CTRL) != 0;
    bool shift_held = (modifiers & HOTKEY_MOD_SHIFT) != 0;
    
    char key_char = (char)pressed_vk;
    char seq_char = tolower(key_char);
    
    bool in_insert = IsInsertMode();
    bool in_normal = IsNormalMode();
    bool in_visual = IsVisualMode();
    
    if (in_insert && SequenceIsWaiting()) {
        if (SequenceTimedOut()) {
            log_info("CheckVimMotion insert timeout fallthrough key='%c'", seq_char);
            SequenceReset();
            return false;
        }
        
        SequenceAddKey(seq_char, false);
        SequenceStartTimer();
        for (int i = 0; i < VIM_SEQUENCE_BINDING_COUNT; i++) {
            const SequenceBinding* b = &G_VIM_SEQUENCE_BINDINGS[i];
            if (b->mode == MODE_INSERT && SequenceMatches(b->sequence)) {
                log_info("CheckVimMotion insert match seq='%s'", b->sequence);
                *out_action = b->action_id;
                *out_sequence = b->sequence;
                *out_typed_count = SequenceTypedCount();
                SequenceReset();
                return true;
            }
        }
        
        for (int i = 0; i < VIM_SEQUENCE_BINDING_COUNT; i++) {
            const SequenceBinding* b = &G_VIM_SEQUENCE_BINDINGS[i];
            if (b->mode == MODE_INSERT && SequenceStartsWith(b->sequence)) {
                *out_action = MOTION_NONE;
                *out_sequence = NULL;
                *out_typed_count = 0;
                return true;
            }
        }
        
        SequenceReset();
        log_info("CheckVimMotion insert nomatch key='%c'", seq_char);
        return false;
    }
    
    if (in_insert) {
        if (shift_held) {
            return false;
        }
        
        bool is_prefix = false;
        for (int i = 0; i < VIM_SEQUENCE_BINDING_COUNT; i++) {
            const SequenceBinding* b = &G_VIM_SEQUENCE_BINDINGS[i];
            if (b->mode == MODE_INSERT && tolower(b->sequence[0]) == seq_char) {
                is_prefix = true;
                break;
            }
        }
        
        if (is_prefix) {
            SequenceAddKey(seq_char, true);
            SequenceStartTimer();
            log_info("CheckVimMotion insert seqstart key='%c'", seq_char);
            *out_sequence = NULL;
            *out_typed_count = SequenceTypedCount();
            return false;
        }
        
        SequenceReset();
        return false;
    }
    
    if (in_normal) {
        if (SequenceTimedOut()) {
            SequenceReset();
        }
        
        if (pressed_vk == VK_RETURN || pressed_vk == 160) {
            SequenceReset();
            *out_sequence = NULL;
            *out_typed_count = 0;
            return false;
        }
        if (!shift_held && !ctrl_held) {
            SequenceAddKey(seq_char, true);
            SequenceStartTimer();
            
            for (int i = 0; i < VIM_SEQUENCE_BINDING_COUNT; i++) {
                const SequenceBinding* b = &G_VIM_SEQUENCE_BINDINGS[i];
                if (b->mode == MODE_NORMAL) {
                    if (SequenceMatches(b->sequence)) {
                        *out_action = b->action_id;
                        *out_sequence = b->sequence;
                        *out_typed_count = SequenceTypedCount();
                        SequenceReset();
                        return true;
                    }
                    if (SequenceStartsWith(b->sequence)) {
                        *out_action = MOTION_NONE;
                        *out_sequence = NULL;
                        *out_typed_count = 0;
                        return true;
                    }
                }
            }
            SequenceReset();
        } else {
        }
        
        for (int i = 0; i < VIM_BINDING_COUNT; i++) {
            const MotionBinding* b = &G_VIM_BINDINGS[i];
            if (b->vk_code == pressed_vk && b->modifiers == modifiers && b->mode == MODE_NORMAL) {
                *out_action = b->action_id;
                *out_sequence = NULL;
                *out_typed_count = 0;
                return true;
            }
        }
        
        return false;
    }
    
    if (in_visual) {
        return false;
    }
    
    return false;
}

void ExecuteMotion(MotionAction action, const char* sequence, int typed_count) {
    FlintContext* ctx = FlintContext_Get();
    if (typed_count > 0 && sequence != NULL && strlen(sequence) > 0 && IsInsertMode()) {
        DeleteLastTypedChar(ctx);
        if (ctx->prompt_callbacks.refresh) {
            ListBoxInfo* lb = Vim_GetListBox(ctx);
            ctx->prompt_callbacks.refresh(ctx->window.filter, lb);
        }
    }
    for (int i = 0; i < VIM_SEQUENCE_BINDING_COUNT; i++) {
        const SequenceBinding* b = &G_VIM_SEQUENCE_BINDINGS[i];
        if (b->action_id == action && b->handler != NULL) {
            b->handler(ctx);
            return;
        }
    }
    
    for (int i = 0; i < VIM_BINDING_COUNT; i++) {
        const MotionBinding* b = &G_VIM_BINDINGS[i];
        if (b->action_id == action && b->handler != NULL) {
            b->handler(ctx);
            return;
        }
    }
    
    if (action == MOTION_ENTER_NORMAL) {
        HandleEnterNormal(ctx);
    } else if (action == MOTION_ENTER_INSERT_OUT) {
        HandleEnterInsertOut(ctx);
    } else if (action == MOTION_ENTER_VISUAL) {
        HandleEnterVisual(ctx);
    }
}
