#ifndef VIM_CONTEXT_H
#define VIM_CONTEXT_H
#include "core/types.h"
#include <stddef.h>

typedef struct {
    ListBoxInfo* lb;
    WindowHandle hListBox;
    WindowHandle hEdit;
    PromptState* state;
} VimContext;

void VimCtx_GetText(VimContext* ctx, char* buf, size_t cap);
void VimCtx_SetText(VimContext* ctx, const char* text);
int VimCtx_GetCursor(VimContext* ctx);
void VimCtx_SetCursor(VimContext* ctx, int pos);
VimContext VimCtx_FromGlobals(void);
VimContext VimCtx_FromState(PromptState* state);

#endif
