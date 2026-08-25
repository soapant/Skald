#ifndef VIM_H
#define VIM_H
#include "core/types.h"
#include <stddef.h>
#include <stdbool.h>

// Unified vim state access.
// A VimContext points at whichever text surface is active
// (edit popup, prompt filter, simple prompt, or raw Win32 edit control).
// All mode state lives in globals managed by core/modal.c,
// all key-sequence state lives in g_seq_state.

typedef struct {
    ListBoxInfo* lb;
    WindowHandle hListBox;
    WindowHandle hEdit;
    PromptState* state;
    SimplePromptState* simple_state;
    SimplePromptState* edit_state;
} VimContext;

void Vim_GetText(VimContext* ctx, char* buf, size_t cap);
void Vim_SetText(VimContext* ctx, const char* text);
int Vim_GetCursor(VimContext* ctx);
void Vim_SetCursor(VimContext* ctx, int pos);
VimContext Vim_FromGlobals(void);
VimContext Vim_FromState(PromptState* state);
VimContext Vim_FromSimpleState(SimplePromptState* state);
VimContext Vim_FromEditState(SimplePromptState* state);

// Key sequences (multi-key chords like di( )
void SequenceAddKey(char key, bool was_typed);
void SequenceReset(void);
int SequenceLen(void);
int SequenceTypedCount(void);
bool SequenceMatches(const char* pattern);
bool SequenceStartsWith(const char* pattern);
void SequenceStartTimer(void);
bool SequenceIsWaiting(void);
bool SequenceTimedOut(void);

void DeleteLastTypedChar(VimContext* ctx);

#endif
