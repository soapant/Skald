#ifndef VIM_H
#define VIM_H
#include "core/types.h"
#include "core/context.h"
#include <stddef.h>
#include <stdbool.h>







typedef struct {
    ListBoxInfo* lb;
    WindowHandle hListBox;
    WindowHandle hEdit;
} VimContext;

void Vim_GetText(FlintContext* ctx, char* buf, size_t cap);
void Vim_SetText(FlintContext* ctx, const char* text);
int Vim_GetCursor(FlintContext* ctx);
void Vim_SetCursor(FlintContext* ctx, int pos);
ListBoxInfo* Vim_GetListBox(FlintContext* ctx);


void SequenceAddKey(char key, bool was_typed);
void SequenceReset(void);
int SequenceTypedCount(void);
int SequenceLength(void);
bool SequenceMatches(const char* pattern);
bool SequenceStartsWith(const char* pattern);
void SequenceStartTimer(void);
bool SequenceIsWaiting(void);
bool SequenceTimedOut(void);
bool SequenceHasPendingWait(void);
bool SequenceConsumeTimeout(void);

void DeleteLastTypedChar(FlintContext* ctx);

#endif
