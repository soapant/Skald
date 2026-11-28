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

ConfigTomlStatus config_toml_error_set(ConfigTomlError *err,
                                       ConfigTomlStatus status, int line,
                                       const char *message) {
  return toml_fail(err, status, line, message);
}

static bool config_number_shape(const char *raw, size_t raw_len,
                                bool *out_is_float) {
  size_t i = 0;
  if (i < raw_len && (raw[i] == '-' || raw[i] == '+'))
    i++;
  size_t int_digits = 0;
  for (; i < raw_len && isdigit((unsigned char)raw[i]); i++)
    int_digits++;
  size_t frac_digits = 0;
  bool dot = false;
  if (i < raw_len && raw[i] == '.') {
    dot = true;
    i++;
    for (; i < raw_len && isdigit((unsigned char)raw[i]); i++)
      frac_digits++;
  }
  bool exponent = false;
  if (i < raw_len && (raw[i] == 'e' || raw[i] == 'E')) {
    exponent = true;
    i++;
    if (i < raw_len && (raw[i] == '-' || raw[i] == '+'))
      i++;
    size_t exp_digits = 0;
    for (; i < raw_len && isdigit((unsigned char)raw[i]); i++)
      exp_digits++;
    if (exp_digits == 0)
      return false;
  }
  if (i != raw_len || int_digits + frac_digits == 0)
    return false;
  *out_is_float = dot || exponent;
  return true;
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
  bool is_float = false;
  if (!config_number_shape(raw, raw_len, &is_float))
    return false;
  *out_type = is_float ? CONFIG_TOML_FLOAT : CONFIG_TOML_INT;
  return true;
}

ConfigTomlStatus config_toml_parse_ex(const char *path, ConfigTomlVisitFn visit,
                                      void *user, ConfigTomlError *err) {
  config_toml_error_clear(err);
  if (!path || !visit)
    return toml_fail(err, CONFIG_TOML_ERR_ARG, 0, "path or visitor is null");

  FILE *f = fopen(path, "rb");
  if (!f)
    return toml_fail(err, CONFIG_TOML_ERR_IO, 0, "could not open file");

  char section[CONFIG_TOML_SECTION_MAX] = "";
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
      char tbuf[CONFIG_TOML_SECTION_MAX];
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
    char raw[1024];
    TomlKeyValueStatus kvs = toml_parse_key_value(&(FnTomlParseKeyValueSpec){.f_line = s, .f_len = s_len, .f_key = keybuf, .f_key_cap = sizeof(keybuf), .f_value = raw, .f_value_cap = sizeof(raw)});
    if (kvs != TOML_KV_OK) {
      const char *msg = "invalid key";
      switch (kvs) {
      case TOML_KV_NO_KEY:
        msg = "expected key = value";
        break;
      case TOML_KV_KEY_TOO_LONG:
        msg = "quoted key too long";
        break;
      case TOML_KV_BAD_ESCAPE:
        msg = "unknown escape in quoted key";
        break;
      case TOML_KV_UNTERMINATED_KEY:
        msg = "unterminated quoted key";
        break;
      case TOML_KV_INVALID_KEY:
        msg = "invalid key";
        break;
      case TOML_KV_MISSING_EQ:
        msg = "missing '='";
        break;
      case TOML_KV_INVALID_VALUE:
        msg = "invalid value";
        break;
      default:
        break;
      }
      fclose(f);
      return toml_fail(err, CONFIG_TOML_ERR_PARSE, line_no, msg);
    }
    size_t vlen = strlen(raw);

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
