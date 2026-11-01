#ifndef EDIT_H
#define EDIT_H
#include "core/types.h"
#include "core/context.h"
#include <stddef.h>
#include <stdbool.h>

typedef struct {
  ListBoxInfo *lb;
} EditContext;

void Edit_GetText(FlintContext *ctx, char *buf, size_t cap);
void Edit_SetText(FlintContext *ctx, const char *text);
int Edit_GetCursor(FlintContext *ctx);
void Edit_SetCursor(FlintContext *ctx, int pos);
ListBoxInfo *Edit_GetListBox(FlintContext *ctx);
PromptState* Edit_GetPromptState();

void SequenceAddKey(char key, bool was_typed);
void SequenceReset(void);
int SequenceTypedCount(void);
int SequenceLength(void);
bool SequenceMatches(const char *pattern);
bool SequenceStartsWith(const char *pattern);
void SequenceStartTimer(void);
bool SequenceIsWaiting(void);
bool SequenceTimedOut(void);
bool SequenceHasPendingWait(void);
bool SequenceConsumeTimeout(void);

void DeleteLastTypedChar(FlintContext *ctx);

#endif
