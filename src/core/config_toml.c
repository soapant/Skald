#include "core/config_toml.h"
#include "core/toml_common.h"
#include "core/types/color.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define CONFIG_TOML_LINE_MAX 2048

void config_toml_error_clear(ConfigTomlError *err) {
  if (!err)
    return;
  err->status = CONFIG_TOML_OK;
  err->line = 0;
  err->message[0] = '\0';
}

const char *config_toml_status_name(ConfigTomlStatus status) {
  switch (status) {
  case CONFIG_TOML_OK:
    return "ok";
  case CONFIG_TOML_ERR_IO:
    return "io";
  case CONFIG_TOML_ERR_PARSE:
    return "parse";
  case CONFIG_TOML_ERR_ARG:
    return "argument";
  case CONFIG_TOML_ERR_REJECTED:
    return "rejected";
  }
  return "unknown";
}

static ConfigTomlStatus toml_fail(ConfigTomlError *err, ConfigTomlStatus status,
                                  int line, const char *message) {
  if (err) {
    err->status = status;
    err->line = line;
    snprintf(err->message, sizeof(err->message), "%s", message ? message : "");
  }
  return status;
}

static bool config_classify(const char *raw, size_t raw_len,
                            ConfigTomlValueType *out_type) {
  if (!out_type)
    return false;
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

ConfigTomlStatus config_toml_parse_ex(const char *path, ConfigTomlVisitFn visit,
                                      void *user, ConfigTomlError *err) {
  config_toml_error_clear(err);
  if (!path || !visit)
    return toml_fail(err, CONFIG_TOML_ERR_ARG, 0, "path or visitor is null");

  FILE *f = fopen(path, "rb");
  if (!f)
    return toml_fail(err, CONFIG_TOML_ERR_IO, 0, "could not open file");

  char section[32] = "";
  char line_buf[CONFIG_TOML_LINE_MAX];
  int line_no = 0;

  while (fgets(line_buf, sizeof(line_buf), f)) {
    line_no++;
    char *nl = strchr(line_buf, '\n');
    if (nl)
      *nl = '\0';

    char stripped[CONFIG_TOML_LINE_MAX];
    toml_strip_comment(line_buf, stripped, sizeof(stripped));
    const char *s = toml_trim_left(stripped);
    size_t s_len = toml_trim_right_len(s, strlen(s));

    if (s_len == 0)
      continue;

    if (s[0] == '[') {
      if (s[s_len - 1] != ']' || s_len < 3) {
        fclose(f);
        return toml_fail(err, CONFIG_TOML_ERR_PARSE, line_no,
                         "unterminated section header");
      }
      size_t tlen = s_len - 2;
      if (tlen >= sizeof(section)) {
        fclose(f);
        return toml_fail(err, CONFIG_TOML_ERR_PARSE, line_no,
                         "section name too long");
      }
      char tbuf[32];
      memcpy(tbuf, s + 1, tlen);
      tbuf[tlen] = '\0';
      if (!toml_is_bare_key(tbuf)) {
        fclose(f);
        return toml_fail(err, CONFIG_TOML_ERR_PARSE, line_no,
                         "invalid section name");
      }
      memcpy(section, tbuf, tlen + 1);
      continue;
    }

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
          fclose(f);
          return toml_fail(err, CONFIG_TOML_ERR_PARSE, line_no,
                           "quoted key too long");
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
            fclose(f);
            return toml_fail(err, CONFIG_TOML_ERR_PARSE, line_no,
                             "unknown escape in quoted key");
          }
        } else {
          keybuf[o++] = c;
        }
        i++;
      }
      if (!closed || o == 0) {
        fclose(f);
        return toml_fail(err, CONFIG_TOML_ERR_PARSE, line_no,
                         "unterminated quoted key");
      }
      keybuf[o] = '\0';
      size_t key_end = i + 1;
      cur = s + key_end;
      rem = s_len - key_end;
    } else {
      const char *eq0 = memchr(s, '=', s_len);
      if (!eq0) {
        fclose(f);
        return toml_fail(err, CONFIG_TOML_ERR_PARSE, line_no,
                         "expected key = value");
      }
      size_t klen = toml_trim_right_len(s, (size_t)(eq0 - s));
      if (klen == 0 || klen >= sizeof(keybuf)) {
        fclose(f);
        return toml_fail(err, CONFIG_TOML_ERR_PARSE, line_no, "invalid key");
      }
      memcpy(keybuf, s, klen);
      keybuf[klen] = '\0';
      if (!toml_is_bare_key(keybuf)) {
        fclose(f);
        return toml_fail(err, CONFIG_TOML_ERR_PARSE, line_no, "invalid key");
      }
      cur = s + klen;
      rem = s_len - klen;
    }

    {
      const char *v = toml_trim_left(cur);
      size_t lead = (size_t)(v - cur);
      rem = rem >= lead ? rem - lead : 0;
      cur = v;
      rem = toml_trim_right_len(cur, rem);
    }

    const char *eq = memchr(cur, '=', rem);
    if (!eq) {
      fclose(f);
      return toml_fail(err, CONFIG_TOML_ERR_PARSE, line_no, "missing '='");
    }

    const char *vs = toml_trim_left(eq + 1);
    size_t vlen = toml_trim_right_len(vs, strlen(vs));
    char raw[1024];
    if (vlen == 0 || vlen >= sizeof(raw)) {
      fclose(f);
      return toml_fail(err, CONFIG_TOML_ERR_PARSE, line_no, "invalid value");
    }
    memcpy(raw, vs, vlen);
    raw[vlen] = '\0';

    ConfigTomlEntry entry;
    memset(&entry, 0, sizeof(entry));
    memcpy(entry.section, section, strlen(section) + 1);
    memcpy(entry.key, keybuf, strlen(keybuf) + 1);
    memcpy(entry.raw, raw, strlen(raw) + 1);
    if (!config_classify(raw, vlen, &entry.type)) {
      fclose(f);
      return toml_fail(err, CONFIG_TOML_ERR_PARSE, line_no,
                       "unrecognized value type");
    }
    if (!visit(&entry, user)) {
      fclose(f);
      return toml_fail(err, CONFIG_TOML_ERR_REJECTED, line_no,
                       "visitor rejected entry");
    }
  }

  fclose(f);
  return CONFIG_TOML_OK;
}

ConfigTomlStatus config_toml_parse(const char *path, ConfigTomlVisitFn visit,
                                   void *user) {
  return config_toml_parse_ex(path, visit, user, NULL);
}

bool config_toml_decode_string(const char *raw, char *out, size_t out_cap) {
  return toml_decode_string(raw, out, out_cap);
}

bool config_toml_encode_string(const char *src, char *out, size_t out_cap) {
  return toml_encode_string(src, out, out_cap);
}

bool config_toml_color_ref(const char *body, ColorRef *out) {
  return toml_color_ref(body, out);
}
