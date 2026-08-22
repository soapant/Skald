#include "core/vim_context.h"
#include "common.h"
#include "core/common_ui.h"
#include <string.h>
#include <stdio.h>
#if defined(_WIN32)
#include <windows.h>
#endif

void VimCtx_GetText(VimContext* ctx, char* buf, size_t cap) {
    if (!ctx || !buf || cap==0) return;
    buf[0]='\0';
    if (ctx->state) {
        strncpy(buf, ctx->state->filter, cap-1);
        buf[cap-1]='\0';
    } else if (ctx->simple_state) {
        strncpy(buf, ctx->simple_state->user_input, cap-1);
        buf[cap-1]='\0';
    } else {
#if defined(_WIN32)
        if (WH_TO_HWND(ctx->hEdit)) GetWindowTextA(WH_TO_HWND(ctx->hEdit), buf, (int)cap);
#else
        // fallback for X11 without state - try global active edit via UI
        UI_GetCurrentText(ctx->hEdit, buf);
#endif
    }
}
void VimCtx_SetText(VimContext* ctx, const char* text) {
    if (!ctx || !text) return;
    if (ctx->state) {
        strncpy(ctx->state->filter, text, sizeof(ctx->state->filter)-1);
        ctx->state->filter[sizeof(ctx->state->filter)-1]='\0';
        ctx->state->cursor = (int)strlen(ctx->state->filter);
        if (g_refresh_callback) g_refresh_callback(ctx->state->filter, ctx->hListBox, ctx->lb);
    } else if (ctx->simple_state) {
        strncpy(ctx->simple_state->user_input, text, sizeof(ctx->simple_state->user_input)-1);
        ctx->simple_state->user_input[sizeof(ctx->simple_state->user_input)-1]='\0';
        ctx->simple_state->cursor = (int)strlen(ctx->simple_state->user_input);
    } else {
        UI_SetText(ctx->hEdit, (char*)text);
    }
}
int VimCtx_GetCursor(VimContext* ctx) {
    if (!ctx) return 0;
    if (ctx->state) return ctx->state->cursor;
    if (ctx->simple_state) return ctx->simple_state->cursor;
    uint32_t s=0,e=0;
    UI_Edit_GetSel(ctx->hEdit, &s, &e);
    return (int)s;
}
void VimCtx_SetCursor(VimContext* ctx, int pos) {
    if (!ctx) return;
    if (ctx->state) {
        int len = (int)strlen(ctx->state->filter);
        if (pos<0) pos=0;
        if (pos>len) pos=len;
        ctx->state->cursor = pos;
    } else if (ctx->simple_state) {
        int len = (int)strlen(ctx->simple_state->user_input);
        if (pos<0) pos=0;
        if (pos>len) pos=len;
        ctx->simple_state->cursor = pos;
    } else {
        UI_SetCursorPosition(ctx->hEdit, pos);
    }
}
VimContext VimCtx_FromGlobals(void) {
    VimContext ctx = {0};
#if defined(_WIN32)
    ctx.hEdit = g_ActiveEditControl;
    ctx.hListBox = hListBox;
    if (WH_TO_HWND(hPopupWnd) && IsWindow(WH_TO_HWND(hPopupWnd))) {
        ctx.lb = (ListBoxInfo*)GetWindowLongPtrA(WH_TO_HWND(hPopupWnd), GWLP_USERDATA);
    }
    ctx.state = NULL;
    ctx.simple_state = NULL;
    if (!ctx.lb) {
        extern PromptState* g_currentPromptState;
        extern SimplePromptState* g_currentSimplePromptState;
        if (g_currentPromptState) {
            ctx.lb = g_currentPromptState->has_listbox ? &g_currentPromptState->listbox : NULL;
            ctx.state = g_currentPromptState;
        } else if (g_currentSimplePromptState) {
            ctx.simple_state = g_currentSimplePromptState;
            ctx.lb = NULL;
        }
    }
#else
    extern PromptState* g_currentPromptState;
    extern SimplePromptState* g_currentSimplePromptState;
    ctx.state = g_currentPromptState;
    ctx.simple_state = g_currentSimplePromptState;
    if (ctx.state) {
        ctx.lb = ctx.state->has_listbox ? &ctx.state->listbox : NULL;
        ctx.hListBox = hListBox;
        ctx.hEdit = g_ActiveEditControl;
    } else if (ctx.simple_state) {
        ctx.lb = NULL;
        ctx.hListBox = hListBox;
        ctx.hEdit = g_ActiveEditControl;
    } else {
        ctx.lb = NULL;
        ctx.hListBox = hListBox;
        ctx.hEdit = g_ActiveEditControl;
    }
#endif
    return ctx;
}
VimContext VimCtx_FromState(PromptState* state) {
    VimContext ctx = {0};
    ctx.state = state;
    if (state) {
        ctx.lb = state->has_listbox ? &state->listbox : NULL;
        ctx.hListBox = hListBox;
        ctx.hEdit = g_ActiveEditControl;
    }
    return ctx;
}
VimContext VimCtx_FromSimpleState(SimplePromptState* state) {
    VimContext ctx = {0};
    ctx.simple_state = state;
    ctx.lb = NULL;
    ctx.hListBox = hListBox;
    ctx.hEdit = g_ActiveEditControl;
    return ctx;
}
