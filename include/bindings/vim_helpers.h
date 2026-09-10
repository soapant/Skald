
#ifndef VIM_HELPERS_H
#define VIM_HELPERS_H

#include <stdbool.h>
#include "core/vim.h"

void DeleteCharUnderCursor(VimContext* ctx);
void GoToFirstListBoxItem(VimContext* ctx);
void GoToLastListBoxItem(VimContext* ctx);
void SetCursorToStartOfText(VimContext* ctx);
void SetCursorToEndOfText(VimContext* ctx);
void DeleteInsideDelimiter(VimContext* ctx, char open_char, char close_char, bool keep_mode);
void DeleteInnerWord(VimContext* ctx, bool keep_mode);

#endif