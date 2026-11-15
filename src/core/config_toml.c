#include "core/config_toml.h"
#include "core/types/color.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define CONFIG_TOML_LINE_MAX 2048

static const char *config_trim(const char *s) {
  while (*s && isspace((unsigned char)*s))
    s++;
  return s;
}

static size_t config_trim_end_len(const char *s, size_t len) {
  while (len > 0 && isspace((unsigned char)s[len - 1]))
    len--;
  return len;
}

static bool config_is_bare_key(const char *s) {
  if (!s || !s[0])
    return false;
  for (const char *p = s; *p; p++) {
    char c = *p;
    if (!(isalnum((unsigned char)c) || c == '_' || c == '-'))
      return false;
  }
  return true;
}

static size_t config_strip_comment(const char *line, char *out,
                                   size_t out_cap) {
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

static bool config_classify(const char *raw, size_t raw_len,
                            ConfigTomlValueType *out_type) {
  if (raw_len == 4 && !memcmp(raw, "true", 4)) {
    *out_type = CONFIG_TOML_BOOL;
    return true;
  }
  if (raw_len == 5 && !memcmp(raw, "false", 5)) {
    *out_type = CONFIG_TOML_BOOL;
    return true;
  }
  if (raw_len >= 2 && raw[0] == '"' && raw[raw_len - 1] == '"') {
    *out_type = CONFIG_TOML_STRING;
    return true;
  }
  {
    size_t i = 0;
    if (raw[0] == '-' || raw[0] == '+')
      i++;
    if (i == raw_len)
      return false;
    for (; i < raw_len; i++)
      if (!isdigit((unsigned char)raw[i]))
        break;
    if (i == raw_len && i > 0) {
      *out_type = CONFIG_TOML_INT;
      return true;
    }
  }
  {
    size_t i = 0;
    if (raw[0] == '-' || raw[0] == '+')
      i++;
    bool saw_digit = false, saw_dot = false;
    for (; i < raw_len; i++) {
      char c = raw[i];
      if (isdigit((unsigned char)c))
        saw_digit = true;
      else if (c == '.' && !saw_dot)
        saw_dot = true;
      else
        return false;
    }
    if (saw_digit && saw_dot) {
      *out_type = CONFIG_TOML_FLOAT;
      return true;
    }
  }
  return false;
}

ConfigTomlStatus config_toml_parse(const char *path, ConfigTomlVisitFn visit,
                                   void *user) {
  if (!path || !visit)
    return CONFIG_TOML_ERR_IO;
  FILE *f = fopen(path, "rb");
  if (!f)
    return CONFIG_TOML_ERR_IO;

  ConfigTomlStatus status = CONFIG_TOML_OK;
  char section[32] = "";
  char line_buf[CONFIG_TOML_LINE_MAX];

  while (fgets(line_buf, sizeof(line_buf), f)) {
    char *nl = strchr(line_buf, '\n');
    if (nl)
      *nl = '\0';

    char stripped[CONFIG_TOML_LINE_MAX];
    config_strip_comment(line_buf, stripped, sizeof(stripped));
    const char *s = config_trim(stripped);
    size_t s_len = config_trim_end_len(s, strlen(s));

    if (s_len == 0)
      continue;

    if (s[0] == '[') {
      if (s[s_len - 1] != ']' || s_len < 3) {
        status = CONFIG_TOML_ERR_PARSE;
        goto out;
      }
      size_t tlen = s_len - 2;
      if (tlen >= sizeof(section)) {
        status = CONFIG_TOML_ERR_PARSE;
        goto out;
      }
      char tbuf[32];
      memcpy(tbuf, s + 1, tlen);
      tbuf[tlen] = '\0';
      if (!config_is_bare_key(tbuf)) {
        status = CONFIG_TOML_ERR_PARSE;
        goto out;
      }
      memcpy(section, tbuf, tlen + 1);
      continue;
    }

    {
      char keybuf[384];
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
          if (o + 1 >= sizeof(keybuf)) {
            status = CONFIG_TOML_ERR_PARSE;
            goto out;
          }
          if (c == '\\' && i + 1 < rem) {
            char e = s[++i];
            switch (e) {
            case 'n':
              keybuf[o++] = '\n';
              break;
            case 't':
              keybuf[o++] = '\t';
              break;
            case 'r':
              keybuf[o++] = '\r';
              break;
            case '"':
              keybuf[o++] = '"';
              break;
            case '\\':
              keybuf[o++] = '\\';
              break;
            default:
              status = CONFIG_TOML_ERR_PARSE;
              goto out;
            }
          } else {
            if (o + 1 >= sizeof(keybuf)) {
              status = CONFIG_TOML_ERR_PARSE;
              goto out;
            }
            keybuf[o++] = c;
          }
          i++;
        }
        if (!closed || o == 0) {
          status = CONFIG_TOML_ERR_PARSE;
          goto out;
        }
        keybuf[o] = '\0';
        size_t key_end = i + 1;
        cur = s + key_end;
        rem = s_len - key_end;
      } else {
        const char *eq0 = memchr(s, '=', s_len);
        if (!eq0) {
          status = CONFIG_TOML_ERR_PARSE;
          goto out;
        }
        size_t klen = config_trim_end_len(s, (size_t)(eq0 - s));
        if (klen == 0 || klen >= sizeof(keybuf)) {
          status = CONFIG_TOML_ERR_PARSE;
          goto out;
        }
        memcpy(keybuf, s, klen);
        keybuf[klen] = '\0';
        if (!config_is_bare_key(keybuf)) {
          status = CONFIG_TOML_ERR_PARSE;
          goto out;
        }
        cur = s + klen;
        rem = s_len - klen;
      }

      {
        const char *v = config_trim(cur);
        size_t lead = (size_t)(v - cur);
        rem = rem >= lead ? rem - lead : 0;
        cur = v;
        rem = config_trim_end_len(cur, rem);
      }

      const char *eq = memchr(cur, '=', rem);
      if (!eq) {
        status = CONFIG_TOML_ERR_PARSE;
        goto out;
      }

      const char *vs = config_trim(eq + 1);
      size_t vlen = config_trim_end_len(vs, strlen(vs));
      char raw[1024];
      if (vlen == 0 || vlen >= sizeof(raw)) {
        status = CONFIG_TOML_ERR_PARSE;
        goto out;
      }
      memcpy(raw, vs, vlen);
      raw[vlen] = '\0';

      ConfigTomlEntry entry;
      memset(&entry, 0, sizeof(entry));
      memcpy(entry.section, section, strlen(section) + 1);
      memcpy(entry.key, keybuf, strlen(keybuf) + 1);
      memcpy(entry.raw, raw, strlen(raw) + 1);
      if (!config_classify(raw, vlen, &entry.type)) {
        status = CONFIG_TOML_ERR_PARSE;
        goto out;
      }
      if (!visit(&entry, user)) {
        status = CONFIG_TOML_ERR_PARSE;
        goto out;
      }
    }
  }

out:
  fclose(f);
  return status;
}

bool config_toml_decode_string(const char *raw, char *out, size_t out_cap) {
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

bool config_toml_encode_string(const char *src, char *out, size_t out_cap) {
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

bool config_toml_color_ref(const char *body, ColorRef *out) {
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
