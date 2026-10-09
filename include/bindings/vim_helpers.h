
#ifndef VIM_HELPERS_H
#define VIM_HELPERS_H

#include <stdbool.h>
#include "core/context.h"

#define VIM_TEXT_BUF 1024

void DeleteCharUnderCursor(FlintContext* ctx);
void GoToFirstListBoxItem(FlintContext* ctx);
void GoToLastListBoxItem(FlintContext* ctx);
void SetCursorToStartOfText(FlintContext* ctx);
void SetCursorToEndOfText(FlintContext* ctx);
void DeleteInsideDelimiter(FlintContext* ctx, char open_char, char close_char, bool keep_mode);
void DeleteInnerWord(FlintContext* ctx, bool keep_mode);
bool Vim_DelimRange(FlintContext* ctx, char open_char, char close_char, int* out_start, int* out_end);
bool Vim_WordRange(FlintContext* ctx, bool bigword, bool around, int* out_start, int* out_end);
void DeleteWordForward(FlintContext* ctx, bool keep_mode);
void DeleteSelectionRange(FlintContext* ctx, int start, int end);

int Vim_FirstNonBlank(const char* text, int len);
int Vim_WordStep(const char* text, int len, int pos, bool bigword, bool to_end, bool forward);
int Vim_FindChar(const char* text, int len, int pos, char target, bool till, bool backward);
int Vim_FindPatternPos(const char* text, int len, int pos, const char* pattern, bool backward);
int Vim_BracketMatch(const char* text, int len, int pos);
bool Vim_WordUnder(const char* text, int len, int pos, char* out, int cap);
void Vim_TransformCase(FlintContext* ctx, int mode);

#endif