
#ifndef EDIT_HELPERS_H
#define EDIT_HELPERS_H

#include <stdbool.h>
#include "core/context.h"

#define EDIT_TEXT_BUF 1024

void DeleteCharUnderCursor(FlintContext *ctx);
void GoToFirstListBoxItem(FlintContext *ctx);
void GoToLastListBoxItem(FlintContext *ctx);
void SetCursorToStartOfText(FlintContext *ctx);
void SetCursorToEndOfText(FlintContext *ctx);
void DeleteInsideDelimiter(FlintContext *ctx, char open_char, char close_char,
                           bool keep_mode);
void DeleteInnerWord(FlintContext *ctx, bool keep_mode);
bool Edit_DelimRange(FlintContext *ctx, char open_char, char close_char,
                    int *out_start, int *out_end);
bool Edit_WordRange(FlintContext *ctx, bool bigword, bool around, int *out_start,
                   int *out_end);
void DeleteWordForward(FlintContext *ctx, bool keep_mode);
void DeleteSelectionRange(FlintContext *ctx, int start, int end);

int Edit_FirstNonBlank(const char *text, int len);
int Edit_WordStep(const char *text, int len, int pos, bool bigword, bool to_end,
                 bool forward);
int Edit_FindChar(const char *text, int len, int pos, char target, bool till,
                 bool backward);
int Edit_FindPatternPos(const char *text, int len, int pos, const char *pattern,
                       bool backward);
int Edit_BracketMatch(const char *text, int len, int pos);
bool Edit_WordUnder(const char *text, int len, int pos, char *out, int cap);
void Edit_TransformCase(FlintContext *ctx, int mode);

#endif