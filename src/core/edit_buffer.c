#include <stdbool.h>
#include <string.h>
#include "helpers/helpers_edit_buffer.h"
#include "config_values.h"

void edit_buffer_backspace(char *buf, size_t *cursor) {
  size_t n = strlen(buf);
  if (*cursor > n)
    *cursor = n;
  if (*cursor > 0) {
    memmove(buf + *cursor - 1, buf + *cursor, n - *cursor + 1);
    (*cursor)--;
  }
}

bool edit_buffer_delete_char(char *buf, size_t *cursor) {
  size_t n = strlen(buf);
  if (*cursor >= n)
    return false;
  memmove(buf + *cursor, buf + *cursor + 1, n - *cursor);
  return true;
}

static bool edit_is_quote(char c) { return c == '"' || c == '\''; }

static bool edit_is_closer(char c) {
  return c == '}' || c == ')' || c == ']' || edit_is_quote(c);
}

static bool edit_quote_at_word_start(const char *buf, size_t cursor) {

  if (cursor == 0)
    return true;
  unsigned char prev = (unsigned char)buf[cursor - 1];
  if (prev >= 0x80)
    return false;
  if (prev >= 'a' && prev <= 'z')
    return false;
  if (prev >= 'A' && prev <= 'Z')
    return false;
  if (prev >= '0' && prev <= '9')
    return false;
  return true;
}

char edit_buffer_pair_for(char c) {
  switch (c) {
  case '{':
    return '}';
  case '(':
    return ')';
  case '[':
    return ']';
  case '"':
    return '"';
  case '\'':
    return '\'';
  default:
    return 0;
  }
}

size_t edit_buffer_insert_text(EditBufferInsertText *argspec) {

  if (!argspec->f_buf || !argspec->f_cursor || !argspec->f_text || argspec->f_len == 0 || argspec->f_cap == 0)
    return 0;
  size_t n = strlen(argspec->f_buf);
  if (*argspec->f_cursor > n)
    *argspec->f_cursor = n;

  char closer = 0;
  if (EnableAutoPairing && argspec->f_len == 1) {
    if (edit_is_closer(argspec->f_text[0]) && *argspec->f_cursor < n && argspec->f_buf[*argspec->f_cursor] == argspec->f_text[0]) {
      (*argspec->f_cursor)++;
      return 0;
    }
    if (!edit_is_quote(argspec->f_text[0]) || edit_quote_at_word_start(argspec->f_buf, *argspec->f_cursor)) {
      closer = edit_buffer_pair_for(argspec->f_text[0]);
    }
  }

  size_t total = argspec->f_len + (closer ? (size_t)1 : (size_t)0);
  if (n + total >= argspec->f_cap)
    return 0;
  memmove(argspec->f_buf + *argspec->f_cursor + total, argspec->f_buf + *argspec->f_cursor, n - *argspec->f_cursor + 1);
  memcpy(argspec->f_buf + *argspec->f_cursor, argspec->f_text, argspec->f_len);
  if (closer)
    argspec->f_buf[*argspec->f_cursor + argspec->f_len] = closer;
  *argspec->f_cursor += argspec->f_len;
  return total;
}

bool edit_buffer_backspace_pair(char *buf, size_t *cursor) {
  if (!buf || !cursor || !EnableAutoPairing)
    return false;
  size_t c = *cursor;
  size_t n = strlen(buf);
  if (c == 0 || c > n)
    return false;
  char closer = buf[c];
  if (closer == '\0' || !edit_is_closer(closer))
    return false;
  if (edit_buffer_pair_for(buf[c - 1]) != closer)
    return false;
  memmove(buf + c - 1, buf + c + 1, n - c);
  (*cursor)--;
  return true;
}

bool cursor_back(size_t *cursor) {
  if (*cursor == 0)
    return false;
  (*cursor)--;
  return true;
}

bool cursor_forward(size_t *cursor, size_t text_len) {
  if (*cursor >= text_len)
    return false;
  (*cursor)++;
  return true;
}

