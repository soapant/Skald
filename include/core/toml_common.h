#ifndef TOML_COMMON_H
#define TOML_COMMON_H

#include <stdbool.h>
#include <stddef.h>

#include "core/types/color.h"

const char *toml_trim_left(const char *s);
size_t toml_trim_right_len(const char *s, size_t len);
bool toml_is_bare_key(const char *s);
size_t toml_strip_comment(const char *line, char *out, size_t out_cap);

void toml_trim(char *s);
bool toml_quote_balanced(const char *s);
bool toml_has_control_char(const char *s);
void toml_unquote(char *s);

bool toml_decode_string(const char *raw, char *out, size_t out_cap);
bool toml_encode_string(const char *src, char *out, size_t out_cap);
bool toml_color_ref(const char *body, ColorRef *out);

typedef enum {
  TOML_KV_OK = 0,
  TOML_KV_EMPTY,
  TOML_KV_NO_KEY,
  TOML_KV_KEY_TOO_LONG,
  TOML_KV_BAD_ESCAPE,
  TOML_KV_UNTERMINATED_KEY,
  TOML_KV_INVALID_KEY,
  TOML_KV_MISSING_EQ,
  TOML_KV_INVALID_VALUE
} TomlKeyValueStatus;

typedef struct {
  const char *line;
  size_t len;
  char *key;
  size_t key_cap;
  char *value;
  size_t value_cap;
} TomlParseKeyValue;

TomlKeyValueStatus toml_parse_key_value(TomlParseKeyValue *argspec);
bool toml_next_line(const char **cursor, const char **line, size_t *len);

#endif
