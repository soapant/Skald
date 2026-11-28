#include "helpers/helpers_fs.h"
#include "helpers/helpers_text.h"

#include "platform/platform.h"
#include "deps/log.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(_WIN32)
#define FEAT_SEP '\\'
#else
#define FEAT_SEP '/'
#endif

static char s_data_dir_override[1024];

void data_dir_set_override(const char *path) {
  s_data_dir_override[0] = '\0';
  if (path)
    safe_copy(s_data_dir_override, sizeof(s_data_dir_override), path);
}

bool data_dir(char *out, size_t out_len) {
  if (!out || out_len == 0)
    return false;
  if (s_data_dir_override[0]) {
    safe_copy(out, out_len, s_data_dir_override);
    return true;
  }
  const char *home = Platform_GetRealHome();
  if (!home || !home[0])
    return false;
  safe_copy(out, out_len, home);
#if defined(_WIN32)
  safe_cat(out, out_len, "\\AppData\\Roaming\\Skald");
#else
  safe_cat(out, out_len, "/.local/share/skald");
#endif
  return true;
}

static void path_fold_exe(char *out) {
#if !defined(_WIN32)
  size_t total = strlen(out);
  if (total >= 4) {
    size_t i = total - 4;
    if (out[i] == '.' && (out[i + 1] == 'e' || out[i + 1] == 'E') &&
        (out[i + 2] == 'x' || out[i + 2] == 'X') &&
        (out[i + 3] == 'e' || out[i + 3] == 'E'))
      out[i] = '\0';
  }
#endif
}

bool temp_file_write(TempFileWrite *argspec) {
  if (!argspec->name || !argspec->name[0] || !argspec->out_path || argspec->out_len == 0)
    return false;
  if (!argspec->data && argspec->len > 0)
    return false;
  for (const char *p = argspec->name; *p; p++) {
    if (*p == '/' || *p == '\\')
      return false;
  }
  if (strstr(argspec->name, ".."))
    return false;
  char dir[1024];
  if (!Platform_TempDirPath(dir, sizeof(dir)))
    return false;
  const char sep[2] = {(char)FEAT_SEP, '\0'};
  safe_copy(argspec->out_path, argspec->out_len, dir);
  safe_cat(argspec->out_path, argspec->out_len, sep);
  safe_cat(argspec->out_path, argspec->out_len, argspec->name);
  FILE *f = fopen(argspec->out_path, "wb");
  if (!f)
    return false;
  bool ok = true;
  if (argspec->len > 0 && fwrite(argspec->data, 1, argspec->len, f) != argspec->len)
    ok = false;
  if (fclose(f) != 0)
    ok = false;
  if (!ok)
    remove(argspec->out_path);
  return ok;
}

bool expand_path(const char *template_path, char *out, size_t out_size) {

  if (!template_path || !out || out_size == 0)
    return false;

  if (template_path[0] == '~' &&
      (template_path[1] == '/' || template_path[1] == '\\' ||
       template_path[1] == '\0')) {
    const char *home = Platform_GetRealHome();
    if (home && home[0]) {
      size_t hn = strlen(home);
      size_t total = 0;
      if (hn < out_size) {
        memcpy(out, home, hn);
        total = hn;
      }
      const char *rest = template_path + 1;
      while (*rest && total < out_size - 1)
        out[total++] = *rest++;
      out[total] = '\0';
      path_fold_exe(out);
      return true;
    }
  }

  size_t o = 0;
  const char *p = template_path;
  while (*p != '\0' && o < out_size - 1) {
    out[o++] = *p++;
  }
  out[o] = '\0';
  path_fold_exe(out);
  return true;
}

bool feature_path_get(const char *feature, const char *name, char *out,
                      size_t out_len) {

  if (!feature || !feature[0] || !name || !name[0] || !out || out_len == 0)
    return false;
  char dir_path[512];
  if (!data_dir(dir_path, sizeof(dir_path)))
    return false;
  Platform_MkdirIfNotExists(dir_path);
  const char sep[2] = {(char)FEAT_SEP, '\0'};
  char feat_path[720];
  safe_copy(feat_path, sizeof(feat_path), dir_path);
  safe_cat(feat_path, sizeof(feat_path), sep);
  safe_cat(feat_path, sizeof(feat_path), feature);
  Platform_MkdirIfNotExists(feat_path);
  safe_copy(out, out_len, feat_path);
  safe_cat(out, out_len, sep);
  safe_cat(out, out_len, name);
  return true;
}

bool feature_path_data_dir_exists(const char *stem) {

  if (!stem || !stem[0])
    return false;
  char dir_path[512];
  if (!data_dir(dir_path, sizeof(dir_path)))
    return false;
  char stemdir[720];
  const char sep[2] = {(char)FEAT_SEP, '\0'};
  safe_copy(stemdir, sizeof(stemdir), dir_path);
  safe_cat(stemdir, sizeof(stemdir), sep);
  safe_cat(stemdir, sizeof(stemdir), stem);
  return Platform_DirExists(stemdir);
}

bool file_copy_path(const char *src, const char *dst) {
  FILE *in = fopen(src, "rb");
  if (!in) {
    log_error("copy_file: open src %s failed (%s)", src, strerror(errno));
    return false;
  }
  FILE *out = fopen(dst, "wb");
  if (!out) {
    log_error("copy_file: open dst %s failed (%s)", dst, strerror(errno));
    fclose(in);
    return false;
  }
  char buf[8192];
  size_t n;
  bool ok = true;
  while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
    if (fwrite(buf, 1, n, out) != n) {
      ok = false;
      break;
    }
  }
  if (ferror(in))
    ok = false;
  fclose(in);
  if (fclose(out) != 0)
    ok = false;
  if (!ok)
    remove(dst);
  return ok;
}

bool file_remove_path(const char *path) {
  if (!path || !path[0])
    return false;
  return remove(path) == 0;
}

