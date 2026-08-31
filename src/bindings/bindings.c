#include "common.h"
#include "bindings/bindings.h"
#include "bindings/vim_bindings.h"
#include "core/common_ui.h"
#include "core/modal.h"
#include "core/vim.h"
#include "core/window_manager.h"
#include "helpers.h"
#include <ctype.h>

bool CheckVimMotion(int pressed_vk, int modifiers, MotionAction* out_action, const char** out_sequence, int* out_typed_count) {
    if (!EnableVimMode) return false;
    WindowManager* wm = WindowManager_Get();
    if (!wm) return false;
    
    Popup* top = WindowManager_GetTopPopup(wm);
    if (!top) return false;
    
    bool in_popup = IsWindowValid(g_hEditPopup);
    
#if defined(_WIN32)
    bool ctrl_held = (GetKeyState(VK_CONTROL) & 0x8000);
    bool shift_held = (GetKeyState(VK_SHIFT) & 0x8000);
#else
    bool ctrl_held = (modifiers & HOTKEY_MOD_CTRL) != 0;
    bool shift_held = (modifiers & HOTKEY_MOD_SHIFT) != 0;
#endif
    
    if (pressed_vk == VK_ESCAPE) {
        if (!IsNormalMode()) {
            SequenceReset();
            *out_action = MOTION_ENTER_NORMAL;
            *out_sequence = NULL;
            *out_typed_count = 0;
            return true;
        }
        return false;
    }
    
    char key_char = (char)pressed_vk;
    char seq_char = tolower(key_char);
    
    bool in_insert = IsInsertMode();
    bool in_normal = IsNormalMode();
    bool in_visual = IsVisualMode();
    
    if (in_insert && SequenceIsWaiting()) {
        if (SequenceTimedOut()) {
            SequenceReset();
            return false;
        }
        
        SequenceAddKey(seq_char, false);
        SequenceStartTimer();
        for (int i = 0; i < VIM_SEQUENCE_BINDING_COUNT; i++) {
            const SequenceBinding* b = &G_VIM_SEQUENCE_BINDINGS[i];
            if (b->mode == MODE_INSERT && SequenceMatches(b->sequence) &&
                ScopeMatches(b->scope, in_popup)) {
                *out_action = b->action_id;
                *out_sequence = b->sequence;
                *out_typed_count = SequenceTypedCount();
                SequenceReset();
                return true;
            }
        }
        
        for (int i = 0; i < VIM_SEQUENCE_BINDING_COUNT; i++) {
            const SequenceBinding* b = &G_VIM_SEQUENCE_BINDINGS[i];
            if (b->mode == MODE_INSERT && SequenceStartsWith(b->sequence) &&
                ScopeMatches(b->scope, in_popup)) {
                return true;
            }
        }
        
        SequenceReset();
        return false;
    }
    
    if (in_insert) {
        if (shift_held) {
            return false;
        }
        
        bool is_prefix = false;
        for (int i = 0; i < VIM_SEQUENCE_BINDING_COUNT; i++) {
            const SequenceBinding* b = &G_VIM_SEQUENCE_BINDINGS[i];
            if (b->mode == MODE_INSERT && tolower(b->sequence[0]) == seq_char &&
                ScopeMatches(b->scope, in_popup)) {
                is_prefix = true;
                break;
            }
        }
        
        if (is_prefix) {
            SequenceAddKey(seq_char, true);
            SequenceStartTimer();
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
                if (b->mode == MODE_NORMAL && ScopeMatches(b->scope, in_popup)) {
                    if (SequenceMatches(b->sequence)) {
                        *out_action = b->action_id;
                        *out_sequence = b->sequence;
                        *out_typed_count = SequenceTypedCount();
                        SequenceReset();
                        return true;
                    }
                    if (SequenceStartsWith(b->sequence)) {
                        return true;
                    }
                }
            }
            SequenceReset();
        } else {
        }
        
        for (int i = 0; i < VIM_BINDING_COUNT; i++) {
            const MotionBinding* b = &G_VIM_BINDINGS[i];
            if (b->vk_code == pressed_vk && b->modifiers == modifiers && b->mode == MODE_NORMAL &&
                ScopeMatches(b->scope, in_popup)) {
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
    VimContext ctx = Vim_FromGlobals();
    if (typed_count > 0 && sequence != NULL && strlen(sequence) > 0 && IsInsertMode()) {
        DeleteLastTypedChar(&ctx);
        if (ctx.state && g_refresh_callback) g_refresh_callback(ctx.state->filter, ctx.hListBox, ctx.lb);
    }
    for (int i = 0; i < VIM_SEQUENCE_BINDING_COUNT; i++) {
        const SequenceBinding* b = &G_VIM_SEQUENCE_BINDINGS[i];
        if (b->action_id == action && b->handler != NULL) {
            b->handler(&ctx);
            return;
        }
    }
    
    for (int i = 0; i < VIM_BINDING_COUNT; i++) {
        const MotionBinding* b = &G_VIM_BINDINGS[i];
        if (b->action_id == action && b->handler != NULL) {
            b->handler(&ctx);
            return;
        }
    }
    
    if (action == MOTION_ENTER_NORMAL) {
        HandleEnterNormal(&ctx);
    } else if (action == MOTION_ENTER_INSERT_OUT) {
        HandleEnterInsertOut(&ctx);
    } else if (action == MOTION_ENTER_VISUAL) {
        HandleEnterVisual(&ctx);
    }
}

