#include "core/manifest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "helpers/helpers_text.h"

void Manifest_Trim(char *s) {
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

void Manifest_Unquote(char *s) {
  char *hash = strchr(s, '#');
  if (hash) {
    *hash = '\0';
  }
  Manifest_Trim(s);
  size_t n = strlen(s);
  if (n >= 2 && ((s[0] == '"' && s[n - 1] == '"') ||
                 (s[0] == '\'' && s[n - 1] == '\''))) {
    s[n - 1] = '\0';
    memmove(s, s + 1, n - 1);
  }
}

bool Manifest_ParseString(const char *name, const char *data,
                          FeatureManifest *out) {
  if (!out || !data)
    return false;
  memset(out, 0, sizeof(*out));
  if (name)
    SafeCopy(out->name, sizeof(out->name), name);

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

    char work[512];
    SafeCopy(work, sizeof(work), line);
    Manifest_Trim(work);
    if (work[0] == '\0' || work[0] == '#')
      continue;

    if (work[0] == '[') {
      if (strstr(work, "provides"))
        section = 1;
      else if (strstr(work, "depends"))
        section = 2;
      else
        section = 0;
      continue;
    }

    char *eq = strchr(work, '=');
    if (eq) {
      *eq = '\0';
      char key[128];
      SafeCopy(key, sizeof(key), work);
      Manifest_Trim(key);
      char val[384];
      SafeCopy(val, sizeof(val), eq + 1);
      Manifest_Trim(val);
      Manifest_Unquote(val);
      if (strcmp(key, "name") == 0)
        SafeCopy(out->name, sizeof(out->name), val);
      else if (strcmp(key, "description") == 0) {
      }
      section = 0;
      continue;
    }

    Manifest_Unquote(work);
    if (work[0] == '\0')
      continue;
    if (section == 1 && out->provide_count < MANIFEST_LIST_MAX) {
      SafeCopy(out->provides[out->provide_count++], sizeof(out->provides[0]),
               work);
    } else if (section == 2 && out->dep_count < MANIFEST_LIST_MAX) {
      SafeCopy(out->deps[out->dep_count++], sizeof(out->deps[0]), work);
    }
  }
  out->valid = true;
  return true;
}

bool Manifest_ParseFile(const char *path, const char *name,
                        FeatureManifest *out) {
  if (!out)
    return false;
  memset(out, 0, sizeof(*out));
  if (name)
    SafeCopy(out->name, sizeof(out->name), name);
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
  bool ok = Manifest_ParseString(name, data, out);
  free(data);
  return ok;
}
