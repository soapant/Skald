#ifndef PLATFORM_INPUT_H
#define PLATFORM_INPUT_H

#include <stddef.h>
#include <stdbool.h>

void EditBufferBackspace(char* buf, size_t* cursor);
bool EditBufferDeleteChar(char* buf, size_t* cursor);

#endif
