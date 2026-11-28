#include "core/manifest.h"
#include "core/toml_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deps/log.h"
#include "helpers/helpers_text.h"

void manifest_trim(char *s) { toml_trim(s); }

void manifest_unquote(char *s) { toml_unquote(s); }

static void manifest_mark_bad(bool *malformed, int *bad_line, int line_no) {
  *malformed = true;
  if (bad_line && *bad_line == 0)
    *bad_line = line_no;
}

static bool manifest_parse_string_ex(const char *name, const char *data,
                                     FeatureManifest *out, int *bad_line) {
  if (!out || !data)
    return false;
  memset(out, 0, sizeof(*out));
  if (name)
    safe_copy(out->name, sizeof(out->name), name);

  bool malformed = false;
  int line_no = 0;
  int section = 0;
  const char *cursor = data;
  const char *line = NULL;
  size_t line_len = 0;
  char *work = NULL;
  size_t work_cap = 0;
  while (toml_next_line(&cursor, &line, &line_len)) {
    line_no++;
    if (line_len + 1 > work_cap) {
      size_t cap = work_cap ? work_cap : 256;
      while (cap < line_len + 1)
        cap *= 2;
      char *grown = (char *)realloc(work, cap);
      if (!grown) {
        free(work);
        out->valid = false;
        return false;
      }
      work = grown;
      work_cap = cap;
    }
    memcpy(work, line, line_len);
    work[line_len] = '\0';

    if (toml_has_control_char(work))
      manifest_mark_bad(&malformed, bad_line, line_no);

    toml_trim(work);
    if (work[0] == '\0' || work[0] == '#')
      continue;

    if (work[0] == '[') {
      toml_unquote(work);
      if (strchr(work, ']') == NULL) {
        manifest_mark_bad(&malformed, bad_line, line_no);
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
        manifest_mark_bad(&malformed, bad_line, line_no);
      toml_unquote(val);
      if (strcmp(key, "name") == 0)
        safe_copy(out->name, sizeof(out->name), val);
      if (strcmp(key, "description") == 0)
        safe_copy(out->description, sizeof(out->description), val);
      continue;
    }

    if (!toml_quote_balanced(work))
      manifest_mark_bad(&malformed, bad_line, line_no);
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
  free(work);
  out->valid = !malformed;
  return true;
}

bool manifest_parse_string(const char *name, const char *data,
                           FeatureManifest *out) {
  return manifest_parse_string_ex(name, data, out, NULL);
}

ConfigTomlStatus manifest_parse_file_ex(const char *path, const char *name,
                                        FeatureManifest *out,
                                        ConfigTomlError *err) {
  ConfigTomlStatus status = CONFIG_TOML_OK;
  int bad_line = 0;
  bool alloc_failed = false;
  config_toml_error_clear(err);
  if (!out)
    return config_toml_error_set(err, CONFIG_TOML_ERR_ARG, 0,
                                 "manifest output is null");
  memset(out, 0, sizeof(*out));
  if (name)
    safe_copy(out->name, sizeof(out->name), name);
  if (!path) {
    status = CONFIG_TOML_ERR_ARG;
    goto done;
  }

  FILE *f = fopen(path, "rb");
  if (!f) {
    status = CONFIG_TOML_ERR_IO;
    goto done;
  }
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    status = CONFIG_TOML_ERR_IO;
    goto done;
  }
  long sz = ftell(f);
  if (sz < 0) {
    fclose(f);
    status = CONFIG_TOML_ERR_IO;
    goto done;
  }
  rewind(f);
  char *data = (char *)malloc((size_t)sz + 1);
  if (!data) {
    fclose(f);
    status = CONFIG_TOML_ERR_IO;
    goto done;
  }
  size_t rd = fread(data, 1, (size_t)sz, f);
  fclose(f);
  data[rd] = '\0';
  bool parsed = manifest_parse_string_ex(name, data, out, &bad_line);
  free(data);

  if (!parsed) {
    alloc_failed = true;
    status = CONFIG_TOML_ERR_IO;
  } else if (!out->valid) {
    status = CONFIG_TOML_ERR_PARSE;
  }

done:
  if (status != CONFIG_TOML_OK) {
    char message[256];
    if (status == CONFIG_TOML_ERR_PARSE)
      snprintf(message, sizeof(message), "malformed manifest on line %d",
               bad_line);
    else if (status == CONFIG_TOML_ERR_IO && alloc_failed)
      snprintf(message, sizeof(message), "out of memory parsing manifest '%s'",
               path ? path : "(null)");
    else if (status == CONFIG_TOML_ERR_IO)
      snprintf(message, sizeof(message), "cannot read manifest '%s'",
               path ? path : "(null)");
    else
      snprintf(message, sizeof(message), "invalid manifest arguments");
    return config_toml_error_set(err, status, bad_line, message);
  }
  return CONFIG_TOML_OK;
}

bool manifest_parse_file(const char *path, const char *name,
                         FeatureManifest *out) {
  ConfigTomlStatus st = manifest_parse_file_ex(path, name, out, NULL);
  return st == CONFIG_TOML_OK;
}
