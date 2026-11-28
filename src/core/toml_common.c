#include "core/toml_common.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

const char *toml_trim_left(const char *s) {
  while (*s && isspace((unsigned char)*s))
    s++;
  return s;
}

size_t toml_trim_right_len(const char *s, size_t len) {
  while (len > 0 && isspace((unsigned char)s[len - 1]))
    len--;
  return len;
}

bool toml_is_bare_key(const char *s) {
  if (!s || !s[0])
    return false;
  for (const char *p = s; *p; p++) {
    char c = *p;
    if (!(isalnum((unsigned char)c) || c == '_' || c == '-'))
      return false;
  }
  return true;
}

size_t toml_strip_comment(const char *line, char *out, size_t out_cap) {
  bool in_string = false;
  size_t o = 0;
  for (size_t i = 0; line[i] && o + 1 < out_cap; i++) {
    char c = line[i];
    if (in_string) {
      if (c == '\\' && line[i + 1]) {
        out[o++] = c;
        out[o++] = line[i + 1];
        i++;
        continue;
      }
      if (c == '"')
        in_string = false;
      out[o++] = c;
      continue;
    }
    if (c == '"') {
      in_string = true;
      out[o++] = c;
      continue;
    }
    if (c == '#')
      break;
    out[o++] = c;
  }
  out[o] = '\0';
  return o;
}

void toml_trim(char *s) {
  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ' ||
                   s[n - 1] == '\t'))
    s[--n] = '\0';
  char *p = s;
  while (*p == ' ' || *p == '\t')
    p++;
  if (p != s)
    memmove(s, p, strlen(p) + 1);
}

bool toml_quote_balanced(const char *s) {
  size_t double_q = 0, single_q = 0;
  for (const char *p = s; *p; p++) {
    if (*p == '"')
      double_q++;
    else if (*p == '\'')
      single_q++;
  }
  return (double_q % 2 == 0) && (single_q % 2 == 0);
}

bool toml_has_control_char(const char *s) {
  for (const unsigned char *p = (const unsigned char *)s; *p; p++)
    if (*p < 32 && *p != '\t' && *p != '\r')
      return true;
  return false;
}

void toml_unquote(char *s) {
  size_t i = 0;
  bool in_single = false, in_double = false;
  while (s[i]) {
    if (s[i] == '"' && !in_single)
      in_double = !in_double;
    else if (s[i] == '\'' && !in_double)
      in_single = !in_single;
    else if (s[i] == '#' && !in_single && !in_double) {
      s[i] = '\0';
      break;
    }
    i++;
  }
  toml_trim(s);
  size_t n = strlen(s);
  if (n >= 2 && ((s[0] == '"' && s[n - 1] == '"') ||
                 (s[0] == '\'' && s[n - 1] == '\''))) {
    s[n - 1] = '\0';
    memmove(s, s + 1, n - 1);
  }
}

bool toml_decode_string(const char *raw, char *out, size_t out_cap) {
  if (!raw || !out || out_cap == 0)
    return false;
  size_t len = strlen(raw);
  if (len < 2 || raw[0] != '"' || raw[len - 1] != '"')
    return false;
  size_t o = 0;
  for (size_t i = 1; i + 1 < len && o + 1 < out_cap; i++) {
    char c = raw[i];
    if (c == '\\') {
      if (i + 1 >= len - 1)
        return false;
      char e = raw[++i];
      switch (e) {
      case 'n':
        out[o++] = '\n';
        break;
      case 't':
        out[o++] = '\t';
        break;
      case 'r':
        out[o++] = '\r';
        break;
      case '"':
        out[o++] = '"';
        break;
      case '\\':
        out[o++] = '\\';
        break;
      default:
        return false;
      }
      continue;
    }
    out[o++] = c;
  }
  out[o] = '\0';
  return true;
}

bool toml_encode_string(const char *src, char *out, size_t out_cap) {
  if (!out || out_cap < 3)
    return false;
  size_t o = 0;
  out[o++] = '"';
  for (const char *p = src ? src : ""; *p; p++) {
    const char *ins = NULL;
    switch (*p) {
    case '\\':
      ins = "\\\\";
      break;
    case '"':
      ins = "\\\"";
      break;
    case '\n':
      ins = "\\n";
      break;
    case '\t':
      ins = "\\t";
      break;
    case '\r':
      ins = "\\r";
      break;
    default:
      break;
    }
    if (ins) {
      size_t n = strlen(ins);
      if (o + n + 1 >= out_cap)
        return false;
      memcpy(out + o, ins, n);
      o += n;
      continue;
    }
    if (o + 2 >= out_cap)
      return false;
    out[o++] = *p;
  }
  out[o++] = '"';
  out[o] = '\0';
  return true;
}

bool toml_color_ref(const char *body, ColorRef *out) {
  if (!body || !out)
    return false;
  size_t n = strlen(body);
  if (n > 0 && body[0] == '#') {
    body++;
    n--;
  } else if (n > 2 && body[0] == '0' && (body[1] == 'x' || body[1] == 'X')) {
    body += 2;
    n -= 2;
  }
  if (n != 6)
    return false;
  unsigned v[3];
  for (int i = 0; i < 3; i++) {
    char hi = body[i * 2], lo = body[i * 2 + 1];
    int h, l;
    if (hi >= '0' && hi <= '9')
      h = hi - '0';
    else if (hi >= 'a' && hi <= 'f')
      h = hi - 'a' + 10;
    else if (hi >= 'A' && hi <= 'F')
      h = hi - 'A' + 10;
    else
      return false;
    if (lo >= '0' && lo <= '9')
      l = lo - '0';
    else if (lo >= 'a' && lo <= 'f')
      l = lo - 'a' + 10;
    else if (lo >= 'A' && lo <= 'F')
      l = lo - 'A' + 10;
    else
      return false;
    v[i] = (unsigned)(h * 16 + l);
  }
  *out = (ColorRef)(v[0] | (v[1] << 8) | (v[2] << 16));
  return true;
}

TomlKeyValueStatus toml_parse_key_value(TomlParseKeyValue *argspec) {
  if (!argspec->line || !argspec->key || !argspec->value || !argspec->key_cap ||
      !argspec->value_cap)
    return TOML_KV_INVALID_VALUE;

  TomlKeyValueStatus st = TOML_KV_INVALID_VALUE;
  char *stripped = (char *)malloc(argspec->len + 1);
  if (!stripped)
    return TOML_KV_INVALID_VALUE;
  memcpy(stripped, argspec->line, argspec->len);
  stripped[argspec->len] = '\0';
  toml_strip_comment(stripped, stripped, argspec->len + 1);

  const char *s = toml_trim_left(stripped);
  size_t s_len = toml_trim_right_len(s, strlen(s));
  if (s_len == 0) {
    st = TOML_KV_EMPTY;
    goto done;
  }

  const char *cur = s;
  size_t rem = s_len;
  if (s[0] == '"') {
    size_t i = 1, o = 0;
    bool closed = false;
    while (i < rem) {
      char c = s[i];
      if (c == '"') {
        closed = true;
        break;
      }
      if (o + 1 >= argspec->key_cap) {
        st = TOML_KV_KEY_TOO_LONG;
        goto done;
      }
      if (c == '\\' && i + 1 < rem) {
        char e = s[++i];
        switch (e) {
        case 'n':
          argspec->key[o++] = '\n';
          break;
        case 't':
          argspec->key[o++] = '\t';
          break;
        case 'r':
          argspec->key[o++] = '\r';
          break;
        case '"':
          argspec->key[o++] = '"';
          break;
        case '\\':
          argspec->key[o++] = '\\';
          break;
        default:
          st = TOML_KV_BAD_ESCAPE;
          goto done;
        }
      } else {
        argspec->key[o++] = c;
      }
      i++;
    }
    if (!closed || o == 0) {
      st = TOML_KV_UNTERMINATED_KEY;
      goto done;
    }
    argspec->key[o] = '\0';
    size_t key_end = i + 1;
    cur = s + key_end;
    rem = s_len - key_end;
  } else {
    const char *eq0 = memchr(s, '=', s_len);
    if (!eq0) {
      st = TOML_KV_NO_KEY;
      goto done;
    }
    size_t klen = toml_trim_right_len(s, (size_t)(eq0 - s));
    if (klen == 0 || klen >= argspec->key_cap) {
      st = TOML_KV_INVALID_KEY;
      goto done;
    }
    memcpy(argspec->key, s, klen);
    argspec->key[klen] = '\0';
    if (!toml_is_bare_key(argspec->key)) {
      st = TOML_KV_INVALID_KEY;
      goto done;
    }
    cur = s + klen;
    rem = s_len - klen;
  }

  const char *v = toml_trim_left(cur);
  size_t lead = (size_t)(v - cur);
  rem = rem >= lead ? rem - lead : 0;
  cur = v;
  rem = toml_trim_right_len(cur, rem);

  const char *eq = memchr(cur, '=', rem);
  if (!eq) {
    st = TOML_KV_MISSING_EQ;
    goto done;
  }
  const char *vs = toml_trim_left(eq + 1);
  size_t vlen = toml_trim_right_len(vs, strlen(vs));
  if (vlen == 0 || vlen >= argspec->value_cap) {
    st = TOML_KV_INVALID_VALUE;
    goto done;
  }
  memcpy(argspec->value, vs, vlen);
  argspec->value[vlen] = '\0';
  st = TOML_KV_OK;

done:
  free(stripped);
  return st;
}

bool toml_next_line(const char **cursor, const char **line, size_t *len) {
  if (!cursor || !line || !len || !*cursor || !**cursor)
    return false;
  const char *start = *cursor;
  const char *p = start;
  while (*p && *p != '\n')
    p++;
  size_t n = (size_t)(p - start);
  if (n > 0 && start[n - 1] == '\r')
    n--;
  *line = start;
  *len = n;
  *cursor = *p ? p + 1 : p;
  return true;
}
