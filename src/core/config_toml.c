#include "core/config_toml.h"
#include "core/types/color.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define CFG_TOML_LINE_MAX 2048

static const char *cfg_trim(const char *s) {
  while (*s && isspace((unsigned char)*s))
    s++;
  return s;
}

static size_t cfg_trim_end_len(const char *s, size_t len) {
  while (len > 0 && isspace((unsigned char)s[len - 1]))
    len--;
  return len;
}

static bool cfg_is_bare_key(const char *s) {
  if (!s || !s[0])
    return false;
  for (const char *p = s; *p; p++) {
    char c = *p;
    if (!(isalnum((unsigned char)c) || c == '_' || c == '-'))
      return false;
  }
  return true;
}

static size_t cfg_strip_comment(const char *line, char *out, size_t out_cap) {
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

static bool cfg_classify(const char *raw, size_t raw_len,
                         CfgTomlValueType *out_type) {
  if (raw_len == 4 && !memcmp(raw, "true", 4)) {
    *out_type = CFG_TOML_BOOL;
    return true;
  }
  if (raw_len == 5 && !memcmp(raw, "false", 5)) {
    *out_type = CFG_TOML_BOOL;
    return true;
  }
  if (raw_len >= 2 && raw[0] == '"' && raw[raw_len - 1] == '"') {
    *out_type = CFG_TOML_STRING;
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
      *out_type = CFG_TOML_INT;
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
      *out_type = CFG_TOML_FLOAT;
      return true;
    }
  }
  return false;
}

CfgTomlStatus cfg_toml_parse(const char *path, CfgTomlVisitFn visit,
                             void *user) {
  if (!path || !visit)
    return CFG_TOML_ERR_IO;
  FILE *f = fopen(path, "rb");
  if (!f)
    return CFG_TOML_ERR_IO;

  CfgTomlStatus status = CFG_TOML_OK;
  char section[32] = "";
  char line_buf[CFG_TOML_LINE_MAX];

  while (fgets(line_buf, sizeof(line_buf), f)) {
    char *nl = strchr(line_buf, '\n');
    if (nl)
      *nl = '\0';

    char stripped[CFG_TOML_LINE_MAX];
    cfg_strip_comment(line_buf, stripped, sizeof(stripped));
    const char *s = cfg_trim(stripped);
    size_t s_len = cfg_trim_end_len(s, strlen(s));

    if (s_len == 0)
      continue;

    if (s[0] == '[') {
      if (s[s_len - 1] != ']' || s_len < 3) {
        status = CFG_TOML_ERR_PARSE;
        goto out;
      }
      size_t tlen = s_len - 2;
      if (tlen >= sizeof(section)) {
        status = CFG_TOML_ERR_PARSE;
        goto out;
      }
      char tbuf[32];
      memcpy(tbuf, s + 1, tlen);
      tbuf[tlen] = '\0';
      if (!cfg_is_bare_key(tbuf)) {
        status = CFG_TOML_ERR_PARSE;
        goto out;
      }
      memcpy(section, tbuf, tlen + 1);
      continue;
    }

    {
      const char *eq = memchr(s, '=', s_len);
      if (!eq) {
        status = CFG_TOML_ERR_PARSE;
        goto out;
      }
      size_t klen = cfg_trim_end_len(s, (size_t)(eq - s));
      char keybuf[96];
      if (klen == 0 || klen >= sizeof(keybuf)) {
        status = CFG_TOML_ERR_PARSE;
        goto out;
      }
      memcpy(keybuf, s, klen);
      keybuf[klen] = '\0';
      if (!cfg_is_bare_key(keybuf)) {
        status = CFG_TOML_ERR_PARSE;
        goto out;
      }

      const char *vs = cfg_trim(eq + 1);
      size_t vlen = cfg_trim_end_len(vs, strlen(vs));
      char raw[1024];
      if (vlen == 0 || vlen >= sizeof(raw)) {
        status = CFG_TOML_ERR_PARSE;
        goto out;
      }
      memcpy(raw, vs, vlen);
      raw[vlen] = '\0';

      CfgTomlEntry entry;
      memset(&entry, 0, sizeof(entry));
      memcpy(entry.section, section, strlen(section) + 1);
      memcpy(entry.key, keybuf, strlen(keybuf) + 1);
      memcpy(entry.raw, raw, strlen(raw) + 1);
      if (!cfg_classify(raw, vlen, &entry.type)) {
        status = CFG_TOML_ERR_PARSE;
        goto out;
      }
      if (!visit(&entry, user)) {
        status = CFG_TOML_ERR_PARSE;
        goto out;
      }
    }
  }

out:
  fclose(f);
  return status;
}

bool cfg_toml_decode_string(const char *raw, char *out, size_t out_cap) {
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

bool cfg_toml_encode_string(const char *src, char *out, size_t out_cap) {
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

bool cfg_toml_color_ref(const char *body, ColorRef *out) {
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
