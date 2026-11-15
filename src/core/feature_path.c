#include <stdint.h>
#include "core/feature_path.h"
#include "helpers/helpers_fs.h"

#include <string.h>

#include "core/context.h"
#include "platform/platform.h"
#include "platform/lib.h"
#include "config_values.h"
#include <stdio.h>
#include "deps/log.h"

static int fp_lower(int c) {
  if (c >= 'A' && c <= 'Z')
    return c + 32;
  return c;
}

static int fp_lower_cmp(const char *a, const char *b, size_t n) {
  for (size_t i = 0; i < n; i++) {
    int ca = fp_lower((unsigned char)a[i]);
    int cb = fp_lower((unsigned char)b[i]);
    if (ca != cb)
      return ca - cb;
    if (ca == 0)
      return 0;
  }
  return 0;
}

static int fp_strnicmp(const char *a, const char *b, size_t n) {
  return fp_lower_cmp(a, b, n);
}

static int fp_strcasecmp(const char *a, const char *b) {
  return fp_lower_cmp(a, b, SIZE_MAX);
}

void feature_path_sanitize_stem(const char *src, char *out, size_t out_len) {
  if (!out || out_len == 0) {
    return;
  }
  out[0] = '\0';
  if (!src) {
    return;
  }
  size_t w = 0;
  for (const char *p = src; *p && w + 1 < out_len; p++) {
    char c = *p;
    if (c >= 'A' && c <= 'Z')
      out[w++] = (char)(c + 32);
    else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
             c == '_')
      out[w++] = c;
    else if (c == ' ')
      out[w++] = '_';
  }
  out[w] = '\0';
  if (w == 0 || strcmp(out, ".") == 0 || strcmp(out, "..") == 0)
    out[0] = '\0';
}

bool feature_path_lib_basename_stem(const char *base, char *out,
                                    size_t out_len) {
  if (!base || !out || out_len == 0)
    return false;
  char work[256];
  size_t len = strlen(base);
  if (len == 0 || len >= sizeof(work))
    return false;
  memcpy(work, base, len + 1);
  for (;;) {
    if (len < 4)
      break;
    size_t dot_at = 0;
    for (size_t i = len; i > 0; i--) {
      if (work[i - 1] == '.') {
        dot_at = i - 1;
        break;
      }
    }
    if (dot_at == 0)
      break;
    if (fp_strnicmp(work + dot_at + 1, "hot", 3) != 0)
      break;
    bool digits = false;
    for (size_t i = dot_at + 4; i < len; i++) {
      if (work[i] < '0' || work[i] > '9') {
        digits = false;
        break;
      }
      digits = true;
    }
    if (!digits)
      break;
    len = dot_at;
    work[len] = '\0';
  }
  if (len >= 3 && fp_strcasecmp(work + len - 3, ".so") == 0)
    len -= 3;
  else if (len >= 4 && fp_strcasecmp(work + len - 4, ".dll") == 0)
    len -= 4;
  else if (len >= 5 && fp_strcasecmp(work + len - 5, ".dylib") == 0)
    len -= 5;
  else
    return false;
  if (len == 0 || len >= out_len)
    return false;
  memcpy(out, work, len);
  out[len] = '\0';
  return true;
}

bool feature_path_stem_from_caller(void *caller, char *out, size_t out_len) {
  if (!caller || !out || out_len == 0)
    return false;
  out[0] = '\0';
  char mod_path[512];
  if (!Platform_GetModuleOwnerPath(caller, mod_path, sizeof(mod_path))) {
    log_warn("feature_path: Platform_GetModuleOwnerPath failed for %p, "
             "feature assets may not resolve",
             caller);
    return false;
  }
  const char *base = strrchr(mod_path, '\\');
  const char *fs = strrchr(mod_path, '/');
  if (fs && (!base || fs > base))
    base = fs;
  base = base ? base + 1 : mod_path;
  char stem[128];
  if (!feature_path_lib_basename_stem(base, stem, sizeof(stem))) {
    return false;
  }
  feature_path_sanitize_stem(stem, out, out_len);
  return out[0] != '\0';
}

bool feature_path_resolve_data_path(void *caller, const char *name, char *out,
                                    size_t out_len) {

  if (!name || !name[0] || !out || out_len == 0) {
    return false;
  }

  const char *filename = name;
  for (const char *p = name; *p; p++) {
    if (*p == '/' || *p == '\\') {
      filename = p + 1;
    }
  }

  if (!filename[0]) {
    return false;
  }

  if (strstr(filename, ".."))
    return false;

  size_t stemlen = strlen(filename);

  char stem[64] = {0};
  if (stemlen > 0 && stemlen < 64) {
    int stemok = 1;
    for (size_t i = 0; i < stemlen; i++) {
      char c = filename[i];
      if (c >= 'A' && c <= 'Z')
        continue;
      if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
          c == '_')
        continue;
      if (c == '.')
        continue;
      stemok = 0;
      break;
    }
    if (stemok) {
      for (size_t i = 0; i < stemlen; i++) {
        char c = filename[i];
        stem[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
      }
      stem[stemlen] = '\0';
      char stemdir[64];
      snprintf(stemdir, sizeof(stemdir), "%s", stem);
      char *dot = strrchr(stemdir, '.');
      if (dot)
        *dot = '\0';
      if (stemdir[0] && feature_path_data_dir_exists(stemdir)) {
        return feature_path_get(stemdir, stem, out, out_len);
      }
    }
  } else {
    return false;
  }

  char from_caller[128];
  if (feature_path_stem_from_caller(caller, from_caller, sizeof(from_caller))) {
    return feature_path_get(from_caller, stem, out, out_len);
  }

  const char *feat = SkaldContext_Get()->window.current_feature;
  if (!feat || !feat[0])
    return false;
  char safe[128];
  feature_path_sanitize_stem(feat, safe, sizeof(safe));
  if (safe[0] == '\0')
    return false;
  return feature_path_get(safe, stem, out, out_len);
}

bool feature_path_resolve_asset_path(void *caller, const char *name, char *out,
                                     size_t out_len) {

  if (!name || !name[0] || !out || out_len == 0) {
    return false;
  }
  if (!FeaturePath || !FeaturePath[0]) {
    log_warn("resolve_asset_file: FeaturePath empty, check config");
    return false;
  }
  const char *rel = name;
  while (*rel == '/' || *rel == '\\')
    rel++;
  if (!rel[0])
    return false;
  for (const char *p = rel; *p; p++) {
    if (*p == '/' || *p == '\\') {
      return false;
    }
  }
  if (strstr(rel, ".."))
    return false;
  char stem[128];
  if (!feature_path_stem_from_caller(caller, stem, sizeof(stem))) {
    return false;
  }
  return Platform_ResolveAssetFile(stem, rel, out, out_len);
}
