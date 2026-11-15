#include "core/manifest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deps/log.h"
#include "helpers/helpers_text.h"

void manifest_trim(char *s) {
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

static bool quote_balanced(const char *s) {
  size_t double_q = 0, single_q = 0;
  for (const char *p = s; *p; p++) {
    if (*p == '"')
      double_q++;
    else if (*p == '\'')
      single_q++;
  }
  return (double_q % 2 == 0) && (single_q % 2 == 0);
}

static bool has_control_char(const char *s) {
  for (const unsigned char *p = (const unsigned char *)s; *p; p++)
    if (*p < 32 && *p != '\t' && *p != '\r')
      return true;
  return false;
}

void manifest_unquote(char *s) {
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
  manifest_trim(s);
  size_t n = strlen(s);
  if (n >= 2 && ((s[0] == '"' && s[n - 1] == '"') ||
                 (s[0] == '\'' && s[n - 1] == '\''))) {
    s[n - 1] = '\0';
    memmove(s, s + 1, n - 1);
  }
}

bool manifest_parse_string(const char *name, const char *data,
                           FeatureManifest *out) {
  if (!out || !data)
    return false;
  memset(out, 0, sizeof(*out));
  if (name)
    safe_copy(out->name, sizeof(out->name), name);

  bool malformed = false;
  int section = 0;
  const char *off = data;
  while (*off) {
    const char *nl = strchr(off, '\n');
    size_t line_len = nl ? (size_t)(nl - off) : strlen(off);
    char line[512];
    size_t cl = line_len < sizeof(line) - 1 ? line_len : sizeof(line) - 1;
    memcpy(line, off, cl);
    line[cl] = '\0';
    off = nl ? nl + 1 : off + line_len;

    if (has_control_char(line))
      malformed = true;

    char work[512];
    safe_copy(work, sizeof(work), line);
    manifest_trim(work);
    if (work[0] == '\0' || work[0] == '#')
      continue;

    if (work[0] == '[') {
      manifest_unquote(work);
      if (strchr(work, ']') == NULL) {
        malformed = true;
        section = 0;
        continue;
      }
      if (strcmp(work, "[provides]") == 0)
        section = 1;
      else if (strcmp(work, "[depends]") == 0)
        section = 2;
      else
        section = 0;
      continue;
    }

    char *eq = strchr(work, '=');
    if (eq) {
      *eq = '\0';
      char key[128];
      safe_copy(key, sizeof(key), work);
      manifest_trim(key);
      char val[384];
      safe_copy(val, sizeof(val), eq + 1);
      manifest_trim(val);
      if (key[0] == '\0' || !quote_balanced(val))
        malformed = true;
      manifest_unquote(val);
      if (strcmp(key, "name") == 0)
        safe_copy(out->name, sizeof(out->name), val);
      else if (strcmp(key, "description") == 0) {
      }
      section = 0;
      continue;
    }

    if (!quote_balanced(work))
      malformed = true;
    manifest_unquote(work);
    if (work[0] == '\0')
      continue;
    if (section == 1 && out->provide_count < MANIFEST_LIST_MAX) {
      safe_copy(out->provides[out->provide_count++], sizeof(out->provides[0]),
                work);
    } else if (section == 2 && out->dep_count < MANIFEST_LIST_MAX) {
      safe_copy(out->deps[out->dep_count++], sizeof(out->deps[0]), work);
    } else if (section != 0) {
      log_warn("manifest: ignoring extra entry '%s' in section %d", work,
               section);
    }
  }
  out->valid = !malformed;
  return true;
}

bool manifest_parse_file(const char *path, const char *name,
                         FeatureManifest *out) {
  if (!out)
    return false;
  memset(out, 0, sizeof(*out));
  if (name)
    safe_copy(out->name, sizeof(out->name), name);
  if (!path)
    return false;
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return false;
  }
  long sz = ftell(f);
  if (sz < 0) {
    fclose(f);
    return false;
  }
  rewind(f);
  char *data = (char *)malloc((size_t)sz + 1);
  if (!data) {
    fclose(f);
    return false;
  }
  size_t rd = fread(data, 1, (size_t)sz, f);
  fclose(f);
  data[rd] = '\0';
  bool ok = manifest_parse_string(name, data, out);
  free(data);
  return ok;
}
