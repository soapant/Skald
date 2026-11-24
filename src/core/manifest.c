#include "core/manifest.h"
#include "core/toml_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deps/log.h"
#include "helpers/helpers_text.h"

void manifest_trim(char *s) { toml_trim(s); }

void manifest_unquote(char *s) { toml_unquote(s); }

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

    if (toml_has_control_char(line))
      malformed = true;

    char work[512];
    safe_copy(work, sizeof(work), line);
    toml_trim(work);
    if (work[0] == '\0' || work[0] == '#')
      continue;

    if (work[0] == '[') {
      toml_unquote(work);
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
      toml_trim(key);
      char val[384];
      safe_copy(val, sizeof(val), eq + 1);
      toml_trim(val);
      if (key[0] == '\0' || !toml_quote_balanced(val))
        malformed = true;
      toml_unquote(val);
      if (strcmp(key, "name") == 0)
        safe_copy(out->name, sizeof(out->name), val);
      if (strcmp(key, "description") == 0)
        safe_copy(out->description, sizeof(out->description), val);
      section = 0;
      continue;
    }

    if (!toml_quote_balanced(work))
      malformed = true;
    toml_unquote(work);
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

ConfigTomlStatus manifest_parse_file_ex(const char *path, const char *name,
                                        FeatureManifest *out,
                                        ConfigTomlError *err) {
  config_toml_error_clear(err);
  if (!out)
    return CONFIG_TOML_ERR_ARG;
  memset(out, 0, sizeof(*out));
  if (name)
    safe_copy(out->name, sizeof(out->name), name);
  if (!path)
    return CONFIG_TOML_ERR_IO;

  FILE *f = fopen(path, "rb");
  if (!f)
    return CONFIG_TOML_ERR_IO;
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return CONFIG_TOML_ERR_IO;
  }
  long sz = ftell(f);
  if (sz < 0) {
    fclose(f);
    return CONFIG_TOML_ERR_IO;
  }
  rewind(f);
  char *data = (char *)malloc((size_t)sz + 1);
  if (!data) {
    fclose(f);
    return CONFIG_TOML_ERR_IO;
  }
  size_t rd = fread(data, 1, (size_t)sz, f);
  fclose(f);
  data[rd] = '\0';
  manifest_parse_string(name, data, out);
  free(data);

  if (!out->valid)
    return CONFIG_TOML_ERR_PARSE;
  return CONFIG_TOML_OK;
}

bool manifest_parse_file(const char *path, const char *name,
                         FeatureManifest *out) {
  ConfigTomlStatus st = manifest_parse_file_ex(path, name, out, NULL);
  return st == CONFIG_TOML_OK;
}
