#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "core/config_section.h"
#include "core/config_toml.h"
#include "deps/log.h"
#include "platform/platform.h"

static bool section_is_bare_key(const char *s) {
  if (!s || !*s)
    return false;
  for (const char *p = s; *p; p++) {
    char c = *p;
    if (!(isalnum((unsigned char)c) || c == '_' || c == '-'))
      return false;
  }
  return true;
}

static bool section_encode_key(char *out, size_t cap, const char *name) {
  if (section_is_bare_key(name) && strlen(name) < 384) {
    if (strlen(name) >= cap)
      return false;
    memcpy(out, name, strlen(name) + 1);
    return true;
  }
  return config_toml_encode_string(name, out, cap);
}

static char *section_trim(const char *s, size_t len) {
  while (len > 0 && isspace((unsigned char)s[0])) {
    s++;
    len--;
  }
  while (len > 0 && isspace((unsigned char)s[len - 1]))
    len--;
  char *out = (char *)malloc(len + 1);
  if (!out)
    return NULL;
  memcpy(out, s, len);
  out[len] = '\0';
  return out;
}

static size_t section_strip_comment(const char *s, size_t len) {
  bool in_string = false;
  for (size_t i = 0; i < len; i++) {
    char c = s[i];
    if (in_string) {
      if (c == '\\' && i + 1 < len) {
        i++;
        continue;
      }
      if (c == '"')
        in_string = false;
      continue;
    }
    if (c == '"') {
      in_string = true;
      continue;
    }
    if (c == '#')
      return i;
  }
  return len;
}

static int section_parse_entry_line(const char *line, size_t line_len,
                                    char **key_out, char **value_out) {
  *key_out = NULL;
  *value_out = NULL;
  line_len = section_strip_comment(line, line_len);
  char *trimmed = section_trim(line, line_len);
  if (!trimmed)
    return -1;
  if (!trimmed[0] || trimmed[0] == '#') {
    free(trimmed);
    return 0;
  }
  const char *eq = strchr(trimmed, '=');
  if (!eq) {
    free(trimmed);
    return -1;
  }
  char *key_raw = section_trim(trimmed, (size_t)(eq - trimmed));
  char *value_raw = section_trim(eq + 1, strlen(eq + 1));
  free(trimmed);
  if (!key_raw || !value_raw) {
    free(key_raw);
    free(value_raw);
    return -1;
  }
  char key[384];
  if (key_raw[0] == '"') {
    if (!config_toml_decode_string(key_raw, key, sizeof(key))) {
      free(key_raw);
      free(value_raw);
      return -1;
    }
  } else {
    if (!section_is_bare_key(key_raw)) {
      free(key_raw);
      free(value_raw);
      return -1;
    }
    snprintf(key, sizeof(key), "%s", key_raw);
  }
  if (strlen(key) >= sizeof(key) - 1) {
    free(key_raw);
    free(value_raw);
    return -1;
  }
  char value[1024];
  if (!config_toml_decode_string(value_raw, value, sizeof(value))) {
    free(key_raw);
    free(value_raw);
    return -1;
  }
  free(key_raw);
  free(value_raw);
  *key_out = strdup(key);
  *value_out = strdup(value);
  if (!*key_out || !*value_out) {
    free(*key_out);
    free(*value_out);
    *key_out = NULL;
    *value_out = NULL;
    return -1;
  }
  return 1;
}

typedef struct {
  size_t count;
  size_t offset;
} ConfigSectionLine;

static bool section_apply(const char *path, const char *section,
                          const char *name, const char *value, bool is_clear) {
  char *orig = NULL;
  long fsize = 0;
  FILE *f = fopen(path, "rb");
  if (f) {
    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize < 0 || fsize > 1048576) {
      fclose(f);
      log_error("config-section: %s size bad", path);
      return false;
    }
    orig = malloc((size_t)fsize + 1);
    if (!orig) {
      fclose(f);
      return false;
    }
    size_t got = fread(orig, 1, (size_t)fsize, f);
    fclose(f);
    if (got != (size_t)fsize) {
      free(orig);
      log_error("config-section: %s read short", path);
      return false;
    }
    orig[got] = '\0';
  } else {
    if (is_clear)
      return true;
    fsize = 0;
  }

  size_t line_cap = 64, line_count = 0;
  ConfigSectionLine *lines = malloc(line_cap * sizeof(*lines));
  if (!lines) {
    free(orig);
    return false;
  }
  if (orig) {
    size_t pos = 0;
    while (pos < (size_t)fsize) {
      size_t end = pos;
      while (end < (size_t)fsize && orig[end] != '\n')
        end++;
      if (line_count == line_cap) {
        size_t ncap = line_cap * 2;
        ConfigSectionLine *grown = realloc(lines, ncap * sizeof(*grown));
        if (!grown) {
          free(lines);
          free(orig);
          return false;
        }
        lines = grown;
        line_cap = ncap;
      }
      lines[line_count].count = end - pos;
      lines[line_count].offset = pos;
      line_count++;
      pos = end + 1;
    }
  }

  char header[96];
  snprintf(header, sizeof(header), "[%s]", section);

  size_t sec_header = SIZE_MAX;
  for (size_t i = 0; i < line_count; i++) {
    const char *s = orig + lines[i].offset;
    size_t len = lines[i].count;
    char *trimmed = section_trim(s, len);
    bool is_match = trimmed && strcmp(trimmed, header) == 0;
    free(trimmed);
    if (is_match) {
      if (sec_header != SIZE_MAX) {
        free(lines);
        free(orig);
        log_error(
            "config-section: multiple [%s] sections in %s, not touching file",
            section, path);
        return false;
      }
      sec_header = i;
    }
  }

  size_t sec_end = line_count;
  if (sec_header != SIZE_MAX) {
    for (size_t i = sec_header + 1; i < line_count; i++) {
      const char *s = orig + lines[i].offset;
      size_t len = lines[i].count;
      char *trimmed = section_trim(s, len);
      bool is_header =
          trimmed && trimmed[0] == '[' && trimmed[strlen(trimmed) - 1] == ']';
      free(trimmed);
      if (is_header) {
        sec_end = i;
        break;
      }
    }
  }

  char enc_key[512], enc_val[1100];
  if (!is_clear) {
    if (!section_encode_key(enc_key, sizeof(enc_key), name)) {
      free(lines);
      free(orig);
      log_error("config-section: key '%s' too long or invalid", name);
      return false;
    }
    if (!config_toml_encode_string(value, enc_val, sizeof(enc_val)) ||
        strlen(enc_val) >= 1024 || strlen(enc_key) >= 384) {
      free(lines);
      free(orig);
      log_error("config-section: value too long for '%s'", name);
      return false;
    }
  }

  size_t replace_idx = SIZE_MAX;
  if (sec_header != SIZE_MAX) {
    for (size_t i = sec_header + 1; i < sec_end; i++) {
      char *key = NULL, *entry_value = NULL;
      int pr = section_parse_entry_line(orig + lines[i].offset, lines[i].count,
                                        &key, &entry_value);
      if (pr < 0) {
        free(lines);
        free(orig);
        log_error("config-section: malformed line %zu in [%s] of %s, not "
                  "touching file",
                  i + 1, section, path);
        return false;
      }
      if (pr == 0)
        continue;
      bool match = strcmp(key, name) == 0;
      free(key);
      free(entry_value);
      if (match) {
        replace_idx = i;
        break;
      }
    }
  }

  bool found = sec_header != SIZE_MAX;
  size_t entry_len = strlen(enc_key) + 3 + strlen(enc_val) + 1;

  size_t cap = (size_t)fsize + entry_len + 64;
  char *out = malloc(cap + 1);
  if (!out) {
    free(lines);
    free(orig);
    return false;
  }
  size_t out_len = 0;

  for (size_t i = 0; i < line_count; i++) {
    if (is_clear && i == replace_idx)
      continue;
    if (!is_clear && i == replace_idx) {
      memcpy(out + out_len, enc_key, strlen(enc_key));
      out_len += strlen(enc_key);
      memcpy(out + out_len, " = ", 3);
      out_len += 3;
      memcpy(out + out_len, enc_val, strlen(enc_val));
      out_len += strlen(enc_val);
      out[out_len++] = '\n';
      continue;
    }
    if (!is_clear && sec_header != SIZE_MAX && i == sec_end &&
        replace_idx == SIZE_MAX) {
      memcpy(out + out_len, enc_key, strlen(enc_key));
      out_len += strlen(enc_key);
      memcpy(out + out_len, " = ", 3);
      out_len += 3;
      memcpy(out + out_len, enc_val, strlen(enc_val));
      out_len += strlen(enc_val);
      out[out_len++] = '\n';
    }
    memcpy(out + out_len, orig + lines[i].offset, lines[i].count);
    out_len += lines[i].count;
    if (lines[i].count == 0 ||
        orig[lines[i].offset + lines[i].count - 1] != '\n')
      out[out_len++] = '\n';
  }

  if (!found && !is_clear) {
    if (out_len > 0 && out[out_len - 1] != '\n')
      out[out_len++] = '\n';
    memcpy(out + out_len, "\n", 1);
    out_len += 1;
    memcpy(out + out_len, header, strlen(header));
    out_len += strlen(header);
    out[out_len++] = '\n';
    memcpy(out + out_len, enc_key, strlen(enc_key));
    out_len += strlen(enc_key);
    memcpy(out + out_len, " = ", 3);
    out_len += 3;
    memcpy(out + out_len, enc_val, strlen(enc_val));
    out_len += strlen(enc_val);
    out[out_len++] = '\n';
  } else if (!is_clear && replace_idx == SIZE_MAX && sec_end == line_count) {
    memcpy(out + out_len, enc_key, strlen(enc_key));
    out_len += strlen(enc_key);
    memcpy(out + out_len, " = ", 3);
    out_len += 3;
    memcpy(out + out_len, enc_val, strlen(enc_val));
    out_len += strlen(enc_val);
    out[out_len++] = '\n';
  }

  char tmp[512];
  snprintf(tmp, sizeof(tmp), "%s.tmp.%d", path, (int)getpid());
  FILE *outf = fopen(tmp, "wb");
  bool ok = outf != NULL;
  if (ok) {
    if (fwrite(out, 1, out_len, outf) != out_len)
      ok = false;
    if (fclose(outf) != 0)
      ok = false;
  }
  free(out);
  free(lines);
  free(orig);
  if (!ok) {
    remove(tmp);
    log_error("config-section: write to %s failed", tmp);
    return false;
  }
  if (!Platform_AtomicRename(tmp, path)) {
    remove(tmp);
    log_error("config-section: rename %s failed", tmp);
    return false;
  }
  return true;
}

bool config_section_set(const char *path, const char *section, const char *name,
                        const char *value) {
  if (!path || !path[0] || !section || !section[0] || !name || !name[0] ||
      !value || !value[0])
    return false;
  if (!section_is_bare_key(section))
    return false;
  return section_apply(path, section, name, value, false);
}

bool config_section_clear(const char *path, const char *section,
                          const char *name) {
  if (!path || !path[0] || !section || !section[0] || !name || !name[0])
    return false;
  if (!section_is_bare_key(section))
    return false;
  return section_apply(path, section, name, NULL, true);
}
