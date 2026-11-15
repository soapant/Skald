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

size_t edit_buffer_insert_text(char *buf, size_t cap, size_t *cursor,
                               const char *text, size_t len) {

  if (!buf || !cursor || !text || len == 0 || cap == 0)
    return 0;
  size_t n = strlen(buf);
  if (*cursor > n)
    *cursor = n;

  char closer = 0;
  if (EnableAutoPairing && len == 1) {
    if (edit_is_closer(text[0]) && *cursor < n && buf[*cursor] == text[0]) {
      (*cursor)++;
      return 0;
    }
    if (!edit_is_quote(text[0]) || edit_quote_at_word_start(buf, *cursor)) {
      closer = edit_buffer_pair_for(text[0]);
    }
  }

  size_t total = len + (closer ? (size_t)1 : (size_t)0);
  if (n + total >= cap)
    return 0;
  memmove(buf + *cursor + total, buf + *cursor, n - *cursor + 1);
  memcpy(buf + *cursor, text, len);
  if (closer)
    buf[*cursor + len] = closer;
  *cursor += len;
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
