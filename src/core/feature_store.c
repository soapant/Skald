#include "core/feature_store.h"
#include "core/cc_registry.h"
#include "core/owner_registry.h"
#include "helpers/helpers_fs.h"
#include "helpers/helpers_misc.h"
#include "helpers/helpers_owner.h"
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

static bool store_sanitize(const char *src, size_t len, char *out, size_t cap) {
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

bool feature_store_feature_name(const char *source_file, char *out,
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

static bool store_path(const char *feature_name, const char *key, char *out,
                       size_t cap) {
  char feature[128];
  char name[128];
  if (!store_key_name(feature_name, feature, sizeof(feature)) ||
      !store_key_name(key, name, sizeof(name)))
    return false;
  char base[512];
  if (!data_dir(base, sizeof(base)))
    return false;
  char dir[800];
  int n = snprintf(dir, sizeof(dir), "%s%s%s", base, STORE_SEP, feature);
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

bool feature_store_set(const char *feature, const char *key,
                       const char *value) {
  const char *values[1] = {value};
  return feature_store_replace_all(feature, key, values, 1);
}

bool feature_store_append(const char *feature, const char *key,
                          const char *value) {
  if (!store_value_valid(value))
    return false;
  char path[1024];
  if (!store_path(feature, key, path, sizeof(path)))
    return false;
  void *mutex = store_lock();
  Platform_MutexLock(mutex);
  bool ok = store_append_unlocked(path, value);
  Platform_MutexUnlock(mutex);
  return ok;
}

StringList feature_store_get_all(const char *feature, const char *key) {
  char path[1024];
  if (!store_path(feature, key, path, sizeof(path))) {
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

char *feature_store_get_last(const char *feature, const char *key) {
  StringList lines = feature_store_get_all(feature, key);
  char *last = NULL;
  if (lines.count > 0)
    last = store_strdup(lines.items[lines.count - 1]);
  string_list_free(&lines);
  return last;
}

bool feature_store_replace_all(const char *feature, const char *key,
                               const char *const *values, int count) {
  if (!store_values_valid(values, count))
    return false;
  char path[1024];
  if (!store_path(feature, key, path, sizeof(path)))
    return false;
  void *mutex = store_lock();
  Platform_MutexLock(mutex);
  bool ok = count == 0 ? store_remove_unlocked(path)
                       : store_write_unlocked(path, values, count);
  Platform_MutexUnlock(mutex);
  return ok;
}

bool feature_store_clear(const char *feature, const char *key) {
  char path[1024];
  if (!store_path(feature, key, path, sizeof(path)))
    return false;
  void *mutex = store_lock();
  Platform_MutexLock(mutex);
  bool ok = store_remove_unlocked(path);
  Platform_MutexUnlock(mutex);
  return ok;
}

typedef struct {
  char name[128];
  void (*cb)(void);
  char icon[512];
  OwnerTag tag;
} FeatureReg;

static FeatureReg **feature_order = NULL;
static int feature_order_count = 0;
static int feature_order_cap = 0;

int feature_list_find(const char *name, void (*cb)(void)) {
  for (int i = 0; i < feature_order_count; i++) {
    if (cb && feature_order[i]->cb == cb)
      return i;
    if (name && feature_order[i]->name[0] &&
        strcmp(feature_order[i]->name, name) == 0)
      return i;
  }
  return -1;
}

bool feature_list_add(const char *name, void (*cb)(void), const char *icon,
                      const char *owner) {
  if (!name || !name[0])
    return false;
  int idx = feature_list_find(name, cb);
  if (idx < 0) {
    if (feature_order_count >= feature_order_cap) {
      int ncap = feature_order_cap ? feature_order_cap * 2 : 32;
      FeatureReg **grown =
          (FeatureReg **)realloc(feature_order, (size_t)ncap * sizeof(*grown));
      if (!grown)
        return false;
      feature_order = grown;
      feature_order_cap = ncap;
    }
    idx = feature_order_count;
    feature_order[idx] = (FeatureReg *)calloc(1, sizeof(**feature_order));
    if (!feature_order[idx])
      return false;
    feature_order_count++;
  }
  FeatureReg *reg = feature_order[idx];
  safe_copy(reg->name, sizeof(reg->name), name);
  reg->cb = cb;
  safe_copy(reg->icon, sizeof(reg->icon), icon);
  owner_tag_set(&reg->tag, owner, 0);
  return true;
}

void feature_list_purge_owner(const char *path_a, const char *path_b) {
  if (owner_purge_inactive(path_a, path_b))
    return;
  int n = 0;
  for (int i = 0; i < feature_order_count; i++) {
    if (owner_tag_matches(&feature_order[i]->tag, path_a, path_b)) {
      owner_tag_clear(&feature_order[i]->tag);
      free(feature_order[i]);
      continue;
    }
    feature_order[n++] = feature_order[i];
  }
  feature_order_count = n;
}

int feature_list_count(void) { return feature_order_count; }

bool feature_list_at(int index, const char **name, void (**cb)(void),
                     const char **icon) {
  if (index < 0 || index >= feature_order_count)
    return false;
  FeatureReg *reg = feature_order[index];
  if (name)
    *name = reg->name;
  if (cb)
    *cb = reg->cb;
  if (icon)
    *icon = reg->icon[0] ? reg->icon : NULL;
  return true;
}
