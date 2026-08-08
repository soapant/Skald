// vim_helpers.h
#ifndef VIM_HELPERS_H
#define VIM_HELPERS_H

#include <stdbool.h>

void DeleteCharUnderCursor(void);
void GoToFirstListBoxItem(void);
void GoToLastListBoxItem(void);
void DeleteInsideDelimiter(char open_char, char close_char, bool keep_mode);
void DeleteInnerWord(bool keep_mode);

#endif
