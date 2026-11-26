#include "core/feature_store.h"
#include "helpers/helpers_fs.h"
#include "helpers/helpers_misc.h"
#include "helpers/helpers_text.h"
#include "platform/platform.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define STORE_SEP "\\"
#else
#define STORE_SEP "/"
#endif

static void *g_store_mutex;

static void *store_lock(void) { return lazy_mutex_get(&g_store_mutex); }

static bool store_sanitize(const char *src, size_t len, char *out,
                           size_t cap) {
  if (!src || len == 0 || !out || cap <= 1)
    return false;
  size_t o = 0;
  for (size_t i = 0; i < len && o + 1 < cap; i++) {
    unsigned char c = (unsigned char)src[i];
    if (isalnum(c) || c == '_' || c == '-')
      out[o++] = (char)c;
    else
      out[o++] = '_';
  }
  out[o] = '\0';
  return o > 0;
}

static bool store_feature_name(const char *source_file, char *out,
                               size_t cap) {
  if (!source_file || !source_file[0])
    return false;
  const char *base = source_file;
  for (const char *p = source_file; *p; p++) {
    if (*p == '/' || *p == '\\')
      base = p + 1;
  }
  size_t len = strlen(base);
  if (len >= 2 && base[len - 2] == '.' &&
      (base[len - 1] == 'c' || base[len - 1] == 'C'))
    len -= 2;
  return store_sanitize(base, len, out, cap);
}

static bool store_key_name(const char *key, char *out, size_t cap) {
  if (!key)
    return false;
  return store_sanitize(key, strlen(key), out, cap);
}

static bool store_path(const char *source_file, const char *key, char *out,
                       size_t cap) {
  char feature[128];
  char name[128];
  if (!store_feature_name(source_file, feature, sizeof(feature)) ||
      !store_key_name(key, name, sizeof(name)))
    return false;
  char base[512];
  if (!data_dir(base, sizeof(base)))
    return false;
  char stores[640];
  int n = snprintf(stores, sizeof(stores), "%s%sstores", base, STORE_SEP);
  if (n < 0 || (size_t)n >= sizeof(stores))
    return false;
  if (!Platform_MkdirIfNotExists(stores))
    return false;
  char dir[800];
  n = snprintf(dir, sizeof(dir), "%s%s%s", stores, STORE_SEP, feature);
  if (n < 0 || (size_t)n >= sizeof(dir))
    return false;
  if (!Platform_MkdirIfNotExists(dir))
    return false;
  n = snprintf(out, cap, "%s%s%s.txt", dir, STORE_SEP, name);
  return n >= 0 && (size_t)n < cap;
}

static bool store_value_valid(const char *value) {
  return value && !strchr(value, '\n') && !strchr(value, '\r');
}

static bool store_values_valid(const char *const *values, int count) {
  if (count < 0 || (count > 0 && !values))
    return false;
  for (int i = 0; i < count; i++)
    if (!store_value_valid(values[i]))
      return false;
  return true;
}

static bool store_remove_unlocked(const char *path) {
  if (!path || !path[0])
    return false;
  if (remove(path) == 0)
    return true;
  return errno == ENOENT;
}

static bool store_append_unlocked(const char *path, const char *value) {
  FILE *f = fopen(path, "ab+");
  if (!f)
    return false;
  bool ok = true;
  long end = ftell(f);
  if (end > 0) {
    if (fseek(f, -1, SEEK_END) != 0) {
      ok = false;
    } else {
      int last = fgetc(f);
      if (last != EOF && last != '\n' && fputc('\n', f) == EOF)
        ok = false;
    }
  }
  if (ok && (fputs(value, f) == EOF || fputc('\n', f) == EOF))
    ok = false;
  if (fflush(f) != 0 || ferror(f))
    ok = false;
  if (fclose(f) != 0)
    ok = false;
  return ok;
}

static bool store_write_unlocked(const char *path, const char *const *values,
                                 int count) {
  char tmp[1100];
  int tn = snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  if (tn < 0 || (size_t)tn >= sizeof(tmp))
    return false;
  FILE *f = fopen(tmp, "wb");
  if (!f)
    return false;
  bool ok = true;
  for (int i = 0; i < count; i++) {
    if (fputs(values[i], f) == EOF || fputc('\n', f) == EOF) {
      ok = false;
      break;
    }
  }
  if (fflush(f) != 0 || ferror(f))
    ok = false;
  if (fclose(f) != 0)
    ok = false;
  if (!ok || !Platform_AtomicRename(tmp, path)) {
    remove(tmp);
    return false;
  }
  return true;
}

bool feature_store_set(const char *source_file, const char *key,
                       const char *value) {
  const char *values[1] = {value};
  return feature_store_replace_all(source_file, key, values, 1);
}

bool feature_store_append(const char *source_file, const char *key,
                          const char *value) {
  if (!store_value_valid(value))
    return false;
  char path[1024];
  if (!store_path(source_file, key, path, sizeof(path)))
    return false;
  void *mutex = store_lock();
  Platform_MutexLock(mutex);
  bool ok = store_append_unlocked(path, value);
  Platform_MutexUnlock(mutex);
  return ok;
}

StringList feature_store_get_all(const char *source_file, const char *key) {
  char path[1024];
  if (!store_path(source_file, key, path, sizeof(path))) {
    StringList empty = {0};
    return empty;
  }
  void *mutex = store_lock();
  Platform_MutexLock(mutex);
  StringList lines = File_ReadLines(path);
  Platform_MutexUnlock(mutex);
  return lines;
}

static char *store_strdup(const char *s) {
  if (!s)
    return NULL;
  size_t n = strlen(s) + 1;
  char *copy = (char *)malloc(n);
  if (copy)
    memcpy(copy, s, n);
  return copy;
}

char *feature_store_get_last(const char *source_file, const char *key) {
  StringList lines = feature_store_get_all(source_file, key);
  char *last = NULL;
  if (lines.count > 0)
    last = store_strdup(lines.items[lines.count - 1]);
  string_list_free(&lines);
  return last;
}

bool feature_store_replace_all(const char *source_file, const char *key,
                               const char *const *values, int count) {
  if (!store_values_valid(values, count))
    return false;
  char path[1024];
  if (!store_path(source_file, key, path, sizeof(path)))
    return false;
  void *mutex = store_lock();
  Platform_MutexLock(mutex);
  bool ok = count == 0 ? store_remove_unlocked(path)
                       : store_write_unlocked(path, values, count);
  Platform_MutexUnlock(mutex);
  return ok;
}

bool feature_store_clear(const char *source_file, const char *key) {
  char path[1024];
  if (!store_path(source_file, key, path, sizeof(path)))
    return false;
  void *mutex = store_lock();
  Platform_MutexLock(mutex);
  bool ok = store_remove_unlocked(path);
  Platform_MutexUnlock(mutex);
  return ok;
}
