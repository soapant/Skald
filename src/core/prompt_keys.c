#include "core/prompt_keys.h"
#include "config.h"
#include "core/context.h"
#include "deps/log.h"
#include "core/history.h"
#include "core/modal.h"
#include "core/listbox_hover.h"
#include "core/prompt_controller.h"
#include "core/timer.h"
#include "bindings/vim_helpers.h"
#include "bindings/vim_state.h"
#include "platform/input.h"
#include "platform/platform.h"
#include "platform/ui.h"
#include "helpers/helpers_ui.h"
#include "helpers/helpers_alias.h"

void Prompt_Refresh(PromptState* state) {
    PROFILE_START(_pt);
    if (!state) { PROFILE_END_VAR(_pt, "Prompt_Refresh"); return; }
    FlintContext* ctx = FlintContext_Get();
    if (ctx && ctx->prompt_callbacks.refresh) {
        ctx->prompt_callbacks.refresh(state->filter, state->has_listbox ? &state->listbox : NULL);
    }
    PROFILE_END_VAR(_pt, "Prompt_Refresh");
}

bool Prompt_DeleteSelection(PromptState* state) {
    PROFILE_START(_pt);
    if (!state) { PROFILE_END_VAR(_pt, "Prompt_DeleteSelection"); return false; }
    int ss = 0, se = 0;
    if (!VimState_GetSelection(&ss, &se)) {
        PROFILE_END_VAR(_pt, "Prompt_DeleteSelection");
        return false;
    }
    if (se < ss) { int t = ss; ss = se; se = t; }
    int len = (int)strlen(state->filter);
    if (ss < 0) ss = 0;
    if (se > len) se = len;
    if (se > ss) {
        memmove(state->filter + ss, state->filter + se, len - se + 1);
        state->cursor = ss;
        VimState_EndSelection();
        PROFILE_END_VAR(_pt, "Prompt_DeleteSelection");
        return true;
    }
    VimState_EndSelection();
    PROFILE_END_VAR(_pt, "Prompt_DeleteSelection");
    return false;
}

void Prompt_InsertText(PromptState* state, const char* text, int len) {
    PROFILE_START(_pt);
    if (!state || !text || len <= 0) { PROFILE_END_VAR(_pt, "Prompt_InsertText"); return; }
    Prompt_DeleteSelection(state);
    size_t fcur = (size_t)state->cursor;
    EditBufferInsertText(state->filter, sizeof(state->filter), &fcur, text, (size_t)len);
    state->cursor = (int)fcur;
    History_ResetCursor(FlintContext_Get()->window.current_feature);
    Prompt_Refresh(state);
    PROFILE_END_VAR(_pt, "Prompt_InsertText");
}

void Prompt_TriggerEnter(PromptState* state, bool ctrl, bool shift, bool alt) {
    PROFILE_START(_pt);
    const char* sel = NULL;
    char filter_copy[sizeof(state->filter)];
    if (state->has_listbox && UI_IsListBoxSelected(&state->listbox)) {
        sel = ListBoxInfo_Row(&state->listbox, state->listbox.selected_index);
    } else {
        strncpy(filter_copy, state->filter, sizeof(filter_copy) - 1);
        filter_copy[sizeof(filter_copy) - 1] = '\0';
        sel = filter_copy;
    }
    const char* feat = FlintContext_Get()->window.current_feature;
    if (feat && sel && sel[0]) History_Add(feat, sel);
    FlintContext* ctx = FlintContext_Get();
    log_info(
        "ENTER_DISPATCH: ctx=%p current_feature=%s "
        "enter=%p ctrl_enter=%p window_enter=%p",
        (void*)ctx,
        ctx->window.current_feature
            ? ctx->window.current_feature : "(null)",
        (void*)ctx->prompt_callbacks.enter,
        (void*)ctx->prompt_callbacks.ctrl_enter,
        (void*)ctx->window.callbacks.enter
    );
    bool should_close = true;
    if (ctrl && shift) {
        should_close = false;
        if (ctx->prompt_callbacks.ctrl_shift_enter) should_close = ctx->prompt_callbacks.ctrl_shift_enter(sel, &state->listbox);
        else if (ctx->prompt_callbacks.enter) should_close = ctx->prompt_callbacks.enter(sel, &state->listbox);
    } else if (ctrl && alt) {
        should_close = false;
        if (ctx->prompt_callbacks.ctrl_alt_enter) should_close = ctx->prompt_callbacks.ctrl_alt_enter(sel, &state->listbox);
        else if (ctx->prompt_callbacks.enter) should_close = ctx->prompt_callbacks.enter(sel, &state->listbox);
    } else if (shift && alt) {
        should_close = false;
        if (ctx->prompt_callbacks.shift_alt_enter) should_close = ctx->prompt_callbacks.shift_alt_enter(sel, &state->listbox);
        else if (ctx->prompt_callbacks.enter) should_close = ctx->prompt_callbacks.enter(sel, &state->listbox);
    } else if (shift) {
        should_close = false;
        if (ctx->prompt_callbacks.shift_enter) should_close = ctx->prompt_callbacks.shift_enter(sel, &state->listbox);
        else if (ctx->prompt_callbacks.enter) should_close = ctx->prompt_callbacks.enter(sel, &state->listbox);
    } else if (ctrl) {
        should_close = false;
        log_info("CTRL_ENTER_BRANCH: ctrl_enter=%p sel='%s'", (void*)ctx->prompt_callbacks.ctrl_enter, sel ? sel : "(null)");
        if (ctx->prompt_callbacks.ctrl_enter) {
            log_info("CTRL_ENTER_CALLBACK_CALLING");
            should_close = ctx->prompt_callbacks.ctrl_enter(sel, &state->listbox);
            log_info("CTRL_ENTER_CALLBACK_RETURNED should_close=%d", should_close);
        } else if (ctx->prompt_callbacks.enter) {
            log_info("ENTER_FALLBACK_CALLBACK_CALLING");
            should_close = ctx->prompt_callbacks.enter(sel, &state->listbox);
            log_info("ENTER_FALLBACK_CALLBACK_RETURNED should_close=%d", should_close);
        } else {
            log_info("CTRL_ENTER_CALLBACK_IS_NULL");
        }
    } else {
        should_close = true;

        log_info("ENTER_BRANCH: enter=%p sel='%s'",
                 (void*)ctx->prompt_callbacks.enter,
                 sel ? sel : "(null)");

        if (ctx->prompt_callbacks.enter) {
            log_info("ENTER_CALLBACK_CALLING");

            should_close = ctx->prompt_callbacks.enter(sel, &state->listbox);

            log_info("ENTER_CALLBACK_RETURNED should_close=%d",
                     should_close);
        } else {
            log_info("ENTER_CALLBACK_IS_NULL");
        }
    }
    ctx->window.selection_complete = should_close;
    PROFILE_END_VAR(_pt, "Prompt_TriggerEnter");
}

bool Prompt_HandleListClick(PromptState* state, bool ctrl, bool shift, bool alt) {
    PROFILE_START(_pt);
    if (!state || !state->has_listbox) { PROFILE_END_VAR(_pt, "Prompt_HandleListClick"); return false; }
    int list_top = 0, item_height = 0, visible_count = 0;
    ListBox_GetGeometry(&list_top, &item_height, &visible_count);
    if (item_height <= 0 || visible_count <= 0) { PROFILE_END_VAR(_pt, "Prompt_HandleListClick"); return false; }
    ListBoxInfo* lb = &state->listbox;
    int mx = 0, my = 0;
    if (!ListBox_ClientMousePos(&mx, &my)) { PROFILE_END_VAR(_pt, "Prompt_HandleListClick"); return false; }
    int idx = ListBox_RowAtY(lb, list_top, item_height, lb->view_offset, visible_count, my);
    if (idx < 0) { PROFILE_END_VAR(_pt, "Prompt_HandleListClick"); return false; }
    UI_SetSelection(lb,idx);
    Prompt_TriggerEnter(state, ctrl, shift, alt);
    PROFILE_END_VAR(_pt, "Prompt_HandleListClick");
    return true;
}

bool Prompt_HandleKey(PromptState* state, const PopupKeyEvent* key) {
    PROFILE_START(_pt);
    if (!state || !key) { PROFILE_END_VAR(_pt, "Prompt_HandleKey"); return false; }
    int vk = key->vk;
    bool ctrl = key->ctrl;
    bool shift = key->shift;
    bool alt = key->alt;
    FlintContext* ctx = FlintContext_Get();

    if (ctrl && (vk == 'A' || vk == 'a')) {
        int len = (int)strlen(state->filter);
        VimState_BeginSelection(0);
        VimState_UpdateSelection(len);
        state->cursor = len;
        Prompt_Refresh(state);
        PROFILE_END_VAR(_pt, "Prompt_HandleKey");
        return true;
    }
    if (ctrl && (vk == 'C' || vk == 'c')) {
        int ss = 0, se = 0;
        if (VimState_GetSelection(&ss, &se)) {
            if (se < ss) { int t = ss; ss = se; se = t; }
            int len = (int)strlen(state->filter);
            if (ss < 0) ss = 0;
            if (se > len) se = len;
            if (se > ss) {
                char buf[512];
                int n = se - ss;
                if (n >= (int)sizeof(buf)) n = (int)sizeof(buf) - 1;
                memcpy(buf, state->filter + ss, n);
                buf[n] = '\0';
                Platform_CopyToClipboard(buf);
            }
        }
        PROFILE_END_VAR(_pt, "Prompt_HandleKey");
        return true;
    }
    if (PromptController_TryVimMotion(vk, ctrl, shift, alt)) {
        PROFILE_END_VAR(_pt, "Prompt_HandleKey");
        return true;
    }
    if (vk == VK_BACK) {
        if (Prompt_DeleteSelection(state)) {
            Prompt_Refresh(state);
            PROFILE_END_VAR(_pt, "Prompt_HandleKey");
            return true;
        }
        if (ctrl) {
            state->filter[0] = '\0';
            state->cursor = 0;
        } else {
            size_t fcur = (size_t)state->cursor;
            if (!EditBufferBackspacePair(state->filter, &fcur) && IsInsertMode()) {
                EditBufferBackspace(state->filter, &fcur);
            }
            state->cursor = (int)fcur;
        }
        History_ResetCursor(ctx->window.current_feature);
        Prompt_Refresh(state);
        PROFILE_END_VAR(_pt, "Prompt_HandleKey");
        return true;
    }
    if (vk == VK_ESCAPE) {
        WindowManager* wm = WindowManager_Get();
        if (wm && wm->popup_count > 1) {
            WindowManager_PopPopup(wm);
            PROFILE_END_VAR(_pt, "Prompt_HandleKey");
            return true;
        }
        if (ctx->window.editbox_placed) {
            if (!EnableVimMode || !IsInsertMode()) {
                ctx->window.selection_complete = true;
            } else {
                SetMode(MODE_NORMAL);
            }
        } else {
            ctx->window.selection_complete = true;
        }
        PROFILE_END_VAR(_pt, "Prompt_HandleKey");
        return true;
    }
    if (vk == VK_RIGHT) {
        VimState_EndSelection();
        CursorForward(&state->cursor, strlen(state->filter));
        PROFILE_END_VAR(_pt, "Prompt_HandleKey");
        return true;
    }
    if (vk == VK_LEFT) {
        VimState_EndSelection();
        CursorBack(&state->cursor, strlen(state->filter));
        PROFILE_END_VAR(_pt, "Prompt_HandleKey");
        return true;
    }
    if (vk == VK_HOME) {
        VimState_EndSelection();
        SetCursorToStartOfText(ctx);
        PROFILE_END_VAR(_pt, "Prompt_HandleKey");
        return true;
    }
    if (vk == VK_END) {
        VimState_EndSelection();
        SetCursorToEndOfText(ctx);
        PROFILE_END_VAR(_pt, "Prompt_HandleKey");
        return true;
    }
    if (vk == VK_UP) {
        if (ctrl) {
            SetCursorToStartOfText(ctx);
            PROFILE_END_VAR(_pt, "Prompt_HandleKey");
            return true;
        }
        if (state->has_listbox) {
            if (state->listbox.selected_index == -1) {
                char hist[512];
                const char* feat = ctx->window.current_feature;
                if (feat && History_CycleUp(feat, hist, sizeof(hist))) {
                    strncpy(state->filter, hist, sizeof(state->filter) - 1);
                    state->filter[sizeof(state->filter) - 1] = '\0';
                    state->cursor = (int)strlen(state->filter);
                    Prompt_Refresh(state);
                    PROFILE_END_VAR(_pt, "Prompt_HandleKey");
                    return true;
                }
            }
            UI_NavigateUp(&state->listbox);
        }
        PROFILE_END_VAR(_pt, "Prompt_HandleKey");
        return true;
    }
    if (vk == VK_DOWN) {
        if (ctrl) {
            SetCursorToEndOfText(ctx);
            PROFILE_END_VAR(_pt, "Prompt_HandleKey");
            return true;
        }
        if (state->has_listbox) UI_NavigateDown(&state->listbox);
        PROFILE_END_VAR(_pt, "Prompt_HandleKey");
        return true;
    }
    if (vk == VK_DELETE) {
        if (Prompt_DeleteSelection(state)) {
            Prompt_Refresh(state);
            PROFILE_END_VAR(_pt, "Prompt_HandleKey");
            return true;
        }
        if (EditBufferDeleteChar(state->filter, &state->cursor)) {
            History_ResetCursor(ctx->window.current_feature);
            Prompt_Refresh(state);
        }
        PROFILE_END_VAR(_pt, "Prompt_HandleKey");
        return true;
    }
    if (vk == VK_TAB) {
        if (ctx->prompt_callbacks.tab) ctx->prompt_callbacks.tab(&state->listbox);
        PROFILE_END_VAR(_pt, "Prompt_HandleKey");
        return true;
    }
    if (vk == VK_RETURN) {
        Prompt_TriggerEnter(state, ctrl, shift, alt);
        PROFILE_END_VAR(_pt, "Prompt_HandleKey");
        return true;
    }
    if (vk == COPY_SELECTION_KEY && !IsInsertMode()) {
        if (shift || ctrl) {
            int type = shift ? 0 : 1;
            int idx = -1;
            if (state->has_listbox && state->listbox.selected_index >= 0 && state->listbox.selected_index < state->listbox.populated_count) {
                idx = state->listbox.matching_indices[state->listbox.selected_index];
            } else {
                idx = UI_LookupTrueIndex(ctx->window.output_buffer);
            }
            if (ctx->prompt_callbacks.copy) {
                ctx->prompt_callbacks.copy(ctx->window.output_buffer, idx, type);
            } else {
                Platform_CopyToClipboard(ctx->window.output_buffer);
            }
        }
        PROFILE_END_VAR(_pt, "Prompt_HandleKey");
        return true;
    }
    if (vk == HELP_SCREEN_KEY && ctrl) {
        if (ctx->prompt_callbacks.help) ctx->prompt_callbacks.help();
        PROFILE_END_VAR(_pt, "Prompt_HandleKey");
        return true;
    }
    if (vk == 'V' && ctrl) {
        char* clip = Platform_GetClipboardEntry();
        if (!clip) {
            PROFILE_END_VAR(_pt, "Prompt_HandleKey");
            return true;
        }
        clip[strcspn(clip, "\r\n")] = '\0';
        if (clip[0] == '\0') {
            free(clip);
            PROFILE_END_VAR(_pt, "Prompt_HandleKey");
            return true;
        }
        size_t cur_len = strlen(state->filter);
        size_t clip_len = strlen(clip);
        size_t avail = sizeof(state->filter) - cur_len - 1;
        if (clip_len > avail) clip_len = avail;
        if (clip_len > 0) {
            memmove(state->filter + state->cursor + clip_len,
                    state->filter + state->cursor,
                    cur_len - state->cursor + 1);
            memcpy(state->filter + state->cursor, clip, clip_len);
            state->cursor += (int)clip_len;
        }
        free(clip);
        Prompt_Refresh(state);
        PROFILE_END_VAR(_pt, "Prompt_HandleKey");
        return true;
    }
    if (ctrl) {
        if (vk == VK_OEM_MINUS) {
            Platform_AdjustFontSize(-1);
            Config_Save();
            PROFILE_END_VAR(_pt, "Prompt_HandleKey");
            return true;
        }
        if (vk == VK_OEM_PLUS) {
            Platform_AdjustFontSize(1);
            Config_Save();
            PROFILE_END_VAR(_pt, "Prompt_HandleKey");
            return true;
        }
    }
    PROFILE_END_VAR(_pt, "Prompt_HandleKey");
    return false;
}

void Prompt_RedrawNow(ListBoxInfo* lb) {
    PROFILE_START(_pt);
    FlintContext* ctx = FlintContext_Get();
    if (!ctx) { PROFILE_END_VAR(_pt, "Prompt_RedrawNow"); return; }
    if (ctx->prompt_callbacks.refresh != NULL) {
        char buffer[MaxFeatures];
        UI_GetCurrentTextEdit(buffer, sizeof(buffer));
        ctx->prompt_callbacks.refresh(buffer, lb);
    }
    PROFILE_END_VAR(_pt, "Prompt_RedrawNow");
}
