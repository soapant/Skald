
#ifndef VIM_HELPERS_H
#define VIM_HELPERS_H

#include <stdbool.h>
#include "core/context.h"

void DeleteCharUnderCursor(FlintContext* ctx);
void GoToFirstListBoxItem(FlintContext* ctx);
void GoToLastListBoxItem(FlintContext* ctx);
void SetCursorToStartOfText(FlintContext* ctx);
void SetCursorToEndOfText(FlintContext* ctx);
void DeleteInsideDelimiter(FlintContext* ctx, char open_char, char close_char, bool keep_mode);
void DeleteInnerWord(FlintContext* ctx, bool keep_mode);

#endif