#ifndef PLATFORM_INPUT_H
#define PLATFORM_INPUT_H

#include <stddef.h>
#include <stdbool.h>

void EditBufferBackspace(char *buf, size_t *cursor);
bool EditBufferDeleteChar(char *buf, size_t *cursor);
char EditBufferPairFor(char c);
size_t EditBufferInsertText(char *buf, size_t cap, size_t *cursor,
                            const char *text, size_t len);
bool EditBufferBackspacePair(char *buf, size_t *cursor);
bool CursorBack(size_t *cursor);
bool CursorForward(size_t *cursor, size_t text_len);

#endif
