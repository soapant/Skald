#ifndef HELPERS_EDIT_BUFFER_H
#define HELPERS_EDIT_BUFFER_H

#include <stddef.h>
#include <stdbool.h>

void edit_buffer_backspace(char *buf, size_t *cursor);
bool edit_buffer_delete_char(char *buf, size_t *cursor);
char edit_buffer_pair_for(char c);
typedef struct {
  char *f_buf;
  size_t f_cap;
  size_t *f_cursor;
  const char *f_text;
  size_t f_len;
} EditBufferInsertText;

size_t edit_buffer_insert_text(EditBufferInsertText *argspec);
bool edit_buffer_backspace_pair(char *buf, size_t *cursor);
bool cursor_back(size_t *cursor);
bool cursor_forward(size_t *cursor, size_t text_len);

#endif

