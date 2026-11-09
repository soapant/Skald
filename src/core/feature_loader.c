#include "core/feature_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "deps/log.h"
#include "platform/lib.h"
#include "platform/platform.h"
#include "core/window_manager.h"
#include "core/checkbox.h"
#include "config_values.h"
#include "helpers/helpers_hotkey.h"
#include "core/thread_registry.h"

#include "core/cc_registry.h"
#include "core/deferred_init.h"
#include "core/widget_registry.h"
#include "core/cap_registry.h"
#include "core/snippet_manager.h"

#define MAX_RETIRED 64
#define MAX_PINS 16

typedef struct {
  char name[64];
  char path[512];
  char uniq[512];
  LibHandle handle;
} LoadedFeature;

static int g_reload_seq = 0;

static bool copy_file(const char *src, const char *dst) {
  FILE *in = fopen(src, "rb");
  if (!in)
    return false;
  FILE *out = fopen(dst, "wb");
  if (!out) {
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

static LoadedFeature *g_features = NULL;
static int g_feature_count = 0;
static int g_feature_cap = 0;

static bool ensure_feature_capacity(int needed) {
  if (needed <= g_feature_cap)
    return true;
  int cap = g_feature_cap > 0 ? g_feature_cap : 16;
  while (cap < needed)
    cap *= 2;
  LoadedFeature *grown =
      (LoadedFeature *)realloc(g_features, (size_t)cap * sizeof(*g_features));
  if (!grown)
    return false;
  g_features = grown;
  g_feature_cap = cap;
  return true;
}

typedef struct {
  LibHandle handle;
  char uniq[512];
  char owner[512];
  unsigned owner_gen;
  Popup *pins[MAX_PINS];
  int npins;
} RetiredLib;
static RetiredLib g_retired[MAX_RETIRED];
static int g_retired_count = 0;

static void retire_old_lib(LibHandle handle, const char *uniq,
                           const char *owner) {
  if (!handle)
    return;
  WindowManager *wm = WindowManager_Get();
  RetiredLib r;
  r.handle = handle;
  r.uniq[0] = '\0';
  if (uniq && uniq[0]) {
    strncpy(r.uniq, uniq, sizeof(r.uniq) - 1);
    r.uniq[sizeof(r.uniq) - 1] = '\0';
  }
  r.owner_gen = widget_current_generation();
  r.owner[0] = '\0';
  if (owner && owner[0]) {
    strncpy(r.owner, owner, sizeof(r.owner) - 1);
    r.owner[sizeof(r.owner) - 1] = '\0';
  }
  r.npins = 0;
  if (wm && wm->popup_count > MAX_PINS) {
    r.npins = -1;
  } else if (wm) {
    for (int i = 0; i < wm->popup_count; i++)
      r.pins[r.npins++] = wm->popups[i];
  }
  if (g_retired_count >= MAX_RETIRED) {
    log_error("retired lib overflow, force unloading oldest");
    cap_purge_owner(g_retired[0].owner, NULL);
    hotkey_purge_owner(g_retired[0].owner, NULL);
    snippet_purge_owner(g_retired[0].owner, NULL);
    log_info("retired lib %s left loaded (never unloaded)", g_retired[0].owner);
    for (int k = 0; k + 1 < g_retired_count; k++)
      g_retired[k] = g_retired[k + 1];
    g_retired_count--;
  }
  g_retired[g_retired_count++] = r;
}

static void sweep_retired_libs(void) {
  WindowManager *wm = WindowManager_Get();
  int n = 0;
  for (int i = 0; i < g_retired_count; i++) {
    bool alive = false;
    if (g_retired[i].owner[0] && widget_owner_active(g_retired[i].owner))
      alive = true;
    else if (g_retired[i].owner[0] &&
             ThreadRegistry_OwnerActive(g_retired[i].owner))
      alive = true;
    else if (wm && g_retired[i].npins >= 0) {
      for (int a = 0; a < g_retired[i].npins && !alive; a++) {
        for (int b = 0; b < wm->popup_count; b++)
          if (wm->popups[b] == g_retired[i].pins[a]) {
            alive = true;
            break;
          }
      }
    } else if (wm && wm->popup_count > 0)
      alive = true;
    if (alive) {
      g_retired[n++] = g_retired[i];
    } else {
      cap_purge_owner(g_retired[i].owner, NULL);
      hotkey_purge_owner(g_retired[i].owner, NULL);
      snippet_purge_owner(g_retired[i].owner, NULL);
      log_info("retired lib %s left loaded (never unloaded)",
               g_retired[i].owner);
    }
  }
  g_retired_count = n;
}

static void *g_feature_mutex = NULL;
static void feature_mutex_init(void) {
  if (!g_feature_mutex)
    g_feature_mutex = Platform_MutexCreate();
}
#define LOCK()                                                                 \
  do {                                                                         \
    feature_mutex_init();                                                      \
    Platform_MutexLock(g_feature_mutex);                                       \
  } while (0)
#define UNLOCK() Platform_MutexUnlock(g_feature_mutex)

static bool ends_with(const char *str, const char *suffix) {
  if (!str || !suffix)
    return false;
  size_t sl = strlen(str), su = strlen(suffix);
  if (su > sl)
    return false;
  return strcmp(str + sl - su, suffix) == 0;
}

static int g_feature_init_count = 0;

#define MANIFEST_LIST_MAX 32

static bool feature_dir(const char *name, char *out, size_t n) {
  if (!FeaturePath || !FeaturePath[0] || !name || !name[0])
    return false;
  snprintf(out, n, "%s/%s", FeaturePath, name);
  return true;
}

typedef struct {
  char name[64];
  char deps[MANIFEST_LIST_MAX][64];
  int dep_count;
  char provides[MANIFEST_LIST_MAX][96];
  int provide_count;
  bool valid;
} FeatureManifest;

static void manifest_trim(char *s) {
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

static void manifest_unquote(char *s) {
  char *hash = strchr(s, '#');
  if (hash) {
    *hash = '\0';
  }
  manifest_trim(s);
  size_t n = strlen(s);
  if (n >= 2 && ((s[0] == '"' && s[n - 1] == '"') ||
                 (s[0] == '\'' && s[n - 1] == '\''))) {
    s[n - 1] = '\0';
    memmove(s, s + 1, n - 1);
  }
}

static bool manifest_path(const char *name, char *out, size_t n) {
  char dir[512];
  if (!feature_dir(name, dir, sizeof(dir)))
    return false;
  snprintf(out, n, "%s/manifest.toml", dir);
  return true;
}

static void manifest_write_baseline(const char *name) {
  char path[512];
  if (!manifest_path(name, path, sizeof(path)))
    return;
  FILE *f = fopen(path, "rb");
  if (f) {
    fclose(f);
    return;
  }

  char dir[512];
  if (!feature_dir(name, dir, sizeof(dir)))
    return;
  Platform_MkdirIfNotExists(dir);
  f = fopen(path, "wb");
  if (!f)
    return;
  fprintf(f, "name = \"%s\"\n", name);
  fprintf(f, "description = \"\"\n\n");
  fprintf(f, "[provides]\n\n");
  fprintf(f, "[depends]\n\n");
  fclose(f);
  log_info("manifest created for feature '%s'", name);
}

void FeatureLoader_OwnerFeatureName(const char *owner_path, char *out,
                                    size_t n) {
  if (!out || n == 0)
    return;
  out[0] = '\0';
  if (!owner_path || !owner_path[0])
    return;
  const char *base = strrchr(owner_path, '/');
  const char *bslash = strrchr(owner_path, '\\');
  if (bslash > base)
    base = bslash;
  base = base ? base + 1 : owner_path;
  snprintf(out, n, "%s", base);
  char *dot = strchr(out, '.');
  if (dot)
    *dot = '\0';
}

static void manifest_add_entry(const char *name, const char *section,
                               const char *entry) {
  char path[512];
  if (!manifest_path(name, path, sizeof(path)))
    return;

  char buf[16384];
  size_t len = 0;
  FILE *f = fopen(path, "rb");
  bool had_file = false;
  if (f) {
    had_file = true;
    len = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
  }
  buf[len] = '\0';

  if (!had_file) {
    manifest_write_baseline(name);
    f = fopen(path, "rb");
    if (!f)
      return;
    len = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[len] = '\0';
  }

  bool have_section = false;
  bool have_entry = false;
  size_t insert_at = 0;
  size_t section_end = 0;

  size_t off = 0;
  while (off < len) {
    char *nl = memchr(buf + off, '\n', len - off);
    size_t line_len = nl ? (size_t)(nl - (buf + off)) : (len - off);
    char line[512];
    size_t cl = line_len < sizeof(line) - 1 ? line_len : sizeof(line) - 1;
    memcpy(line, buf + off, cl);
    line[cl] = '\0';

    char work[512];
    snprintf(work, sizeof(work), "%s", line);
    manifest_trim(work);

    if (work[0] == '[') {
      if (strstr(work, section)) {
        have_section = true;
        section_end = off + line_len + (nl ? 1 : 0);
        insert_at = section_end;
      }
      off += line_len + (nl ? 1 : 0);
      continue;
    }

    if (have_section && section_end == off + line_len + (nl ? 1 : 0)) {
      if (work[0] != '\0')
        insert_at = off + line_len + (nl ? 1 : 0);
    }

    if (have_section && work[0] != '\0' && work[0] != '#') {
      char e[512];
      snprintf(e, sizeof(e), "%s", work);
      manifest_unquote(e);
      if (strcmp(e, entry) == 0)
        have_entry = true;
    }

    off += line_len + (nl ? 1 : 0);
  }

  if (have_entry)
    return;

  char new_line[128];
  snprintf(new_line, sizeof(new_line), "%s\n", entry);

  if (!have_section) {
    char header[64];
    int hn = snprintf(header, sizeof(header), "\n[%s]\n", section);
    size_t n = len;
    if (hn <= 0 || n + (size_t)hn + strlen(new_line) + 16 >= sizeof(buf))
      return;
    memcpy(buf + n, header, (size_t)hn);
    n += (size_t)hn;
    memcpy(buf + n, new_line, strlen(new_line));
    n += strlen(new_line);
    len = n;
    insert_at = len - strlen(new_line);
  } else if (insert_at + strlen(new_line) > sizeof(buf)) {
    return;
  } else {
    memmove(buf + insert_at + strlen(new_line), buf + insert_at,
            len - insert_at);
    memcpy(buf + insert_at, new_line, strlen(new_line));
    len += strlen(new_line);
  }

  f = fopen(path, "wb");
  if (!f)
    return;
  fwrite(buf, 1, len, f);
  fclose(f);
  log_info("manifest '%s' %s += %s", name, section, entry);
}

void FeatureLoader_ManifestAddProvides(const char *owner_path,
                                       const char *key) {
  if (!owner_path || !owner_path[0] || !key || !key[0])
    return;
  char name[64];
  FeatureLoader_OwnerFeatureName(owner_path, name, sizeof(name));
  if (!name[0])
    return;
  manifest_add_entry(name, "provides", key);
}

void FeatureLoader_ManifestAddDepends(const char *owner_path,
                                      const char *dep_name) {
  if (!owner_path || !owner_path[0] || !dep_name || !dep_name[0])
    return;
  char name[64];
  FeatureLoader_OwnerFeatureName(owner_path, name, sizeof(name));
  if (!name[0])
    return;
  if (strcmp(name, dep_name) == 0)
    return;
  manifest_add_entry(name, "depends", dep_name);
}

static void manifest_parse(const char *name, FeatureManifest *m) {
  memset(m, 0, sizeof(*m));
  snprintf(m->name, sizeof(m->name), "%s", name);

  char path[512];
  manifest_path(name, path, sizeof(path));
  FILE *f = fopen(path, "rb");
  if (!f)
    return;

  char line[512];
  int section = 0; /* 0=none 1=provides 2=depends */
  while (fgets(line, sizeof(line), f)) {
    char work[512];
    snprintf(work, sizeof(work), "%s", line);
    manifest_trim(work);
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
      snprintf(key, sizeof(key), "%s", work);
      manifest_trim(key);
      char val[384];
      snprintf(val, sizeof(val), "%s", eq + 1);
      manifest_trim(val);
      manifest_unquote(val);
      if (strcmp(key, "name") == 0)
        snprintf(m->name, sizeof(m->name), "%s", val);
      else if (strcmp(key, "description") == 0) { /* informational */
      }
      section = 0;
      continue;
    }

    manifest_unquote(work);
    if (work[0] == '\0')
      continue;
    if (section == 1 && m->provide_count < MANIFEST_LIST_MAX) {
      snprintf(m->provides[m->provide_count++], sizeof(m->provides[0]), "%s",
               work);
    } else if (section == 2 && m->dep_count < MANIFEST_LIST_MAX) {
      snprintf(m->deps[m->dep_count++], sizeof(m->deps[0]), "%s", work);
    }
  }
  fclose(f);
  m->valid = true;
}

static bool load_single(const char *dir, const char *filename) {
  if (!ensure_feature_capacity(g_feature_count + 1))
    return false;
  char full[1024];
  snprintf(full, sizeof(full), "%s/%s", dir, filename);
  char name[64];
  strncpy(name, filename, sizeof(name) - 1);
  name[sizeof(name) - 1] = '\0';
  char *dot = strrchr(name, '.');
  if (dot)
    *dot = '\0';

  for (int i = 0; i < g_feature_count; i++)
    if (strcmp(g_features[i].name, name) == 0)
      return true;

  LibHandle h = Platform_LoadLibrary(full);
  if (!h) {
    log_error("dlopen %s failed: %s", full, Platform_GetLoadError());
    return false;
  }
  typedef void (*InitFn)(void);
  InitFn init = (InitFn)Platform_GetSymbol(h, "FlintFeatureInit");
  if (!init)
    init = (InitFn)Platform_GetSymbol(h, "FlintFeatureReload");
  if (init) {
    g_feature_init_count++;
  } else {
    log_warn("feature %s has no FlintFeatureInit/Reload symbol", name);
  }

  LoadedFeature *f = &g_features[g_feature_count++];
  snprintf(f->name, sizeof(f->name), "%s", name);
  snprintf(f->path, sizeof(f->path), "%s", full);
  f->handle = h;
  return true;
}

static bool load_named(const char *dir, const char *ext, const char *name) {
  char filename[128];
  snprintf(filename, sizeof(filename), "%s%s", name, ext);
  return load_single(dir, filename);
}

bool FeatureLoader_LoadAllFromDir(const char *dir) {
  LOCK();
  const char *ext = Platform_LibExtension();
  DIR *d = opendir(dir);
  if (!d) {
    UNLOCK();
    return false;
  }

  char (*names)[64] = NULL;
  int names_cap = 0;
  int n_names = 0;
  struct dirent *e;
  while ((e = readdir(d)) != NULL) {
    if (e->d_name[0] == '.')
      continue;
    if (!ends_with(e->d_name, ext))
      continue;
    if (n_names >= names_cap) {
      int cap = names_cap > 0 ? names_cap * 2 : 32;
      char (*grown)[64] = (char (*)[64])realloc(names, (size_t)cap * 64);
      if (!grown)
        break;
      names = grown;
      names_cap = cap;
    }

    char nm[64];
    snprintf(nm, sizeof(nm), "%s", e->d_name);
    char *dot = strrchr(nm, '.');
    if (dot)
      *dot = '\0';
    manifest_write_baseline(nm);
    snprintf(names[n_names++], sizeof(names[0]), "%s", nm);
  }
  closedir(d);

  FeatureManifest *mans =
      (FeatureManifest *)calloc((size_t)n_names, sizeof(FeatureManifest));
  bool *loaded = (bool *)calloc((size_t)n_names, sizeof(bool));
  if (!mans || !loaded) {
    free(mans);
    free(loaded);
    UNLOCK();
    return false;
  }
  for (int i = 0; i < n_names; i++)
    manifest_parse(names[i], &mans[i]);

  for (int i = 0; i < n_names; i++) {
    const char *missing = NULL;
    for (int k = 0; k < mans[i].dep_count && !missing; k++) {
      bool found = false;
      for (int j = 0; j < n_names; j++) {
        if (strcmp(names[j], mans[i].deps[k]) == 0) {
          found = true;
          break;
        }
      }
      if (!found)
        missing = mans[i].deps[k];
    }
    if (missing) {
      printf("flint: skipping feature '%s' - missing dependency '%s'\n",
             names[i], missing);
      log_warn("feature '%s' skipped, dep '%s' not found", names[i], missing);
      continue;
    }
    loaded[i] = true;
  }

  for (int pass = 0; pass < n_names; pass++) {
    bool progress = false;
    for (int i = 0; i < n_names; i++) {
      if (!loaded[i])
        continue;
      bool ready = true;
      for (int k = 0; k < mans[i].dep_count; k++) {
        for (int j = 0; j < n_names; j++) {
          if (strcmp(names[j], mans[i].deps[k]) == 0 && !loaded[j]) {
            ready = false;
            break;
          }
        }
        if (!ready)
          break;
      }
      if (!ready)
        continue;
      loaded[i] = false;
      if (load_named(dir, ext, names[i]))
        progress = true;
    }
    if (!progress) {
      for (int i = 0; i < n_names; i++) {
        if (loaded[i]) {
          loaded[i] = false;
          load_named(dir, ext, names[i]);
        }
      }
      break;
    }
  }

  free(mans);
  free(loaded);
  free(names);
  UNLOCK();
  log_info("FeatureLoader_LoadAllFromDir completed");
  return true;
}

bool FeatureLoader_Init(void) {
  LOCK();
  g_feature_count = 0;
  UNLOCK();
  g_feature_init_count = 0;
  {
    DIR *d = opendir("build/features");
    if (d) {
      struct dirent *e;
      while ((e = readdir(d)) != NULL) {
        const char *dot = strstr(e->d_name, ".hot");
        if (!dot)
          continue;
        const char *end = dot + 4;
        bool digits = true;
        for (const char *p = end; *p; p++)
          if (*p < '0' || *p > '9') {
            digits = false;
            break;
          }
        if (!digits || !*end)
          continue;
        char junk[512];
        snprintf(junk, sizeof(junk), "build/features/%s", e->d_name);
        if (remove(junk) == 0)
          log_info("FeatureLoader_Init: removed stale hot-reload lib %s",
                   e->d_name);
      }
      closedir(d);
    }
  }
  FeatureLoader_LoadAllFromDir("build/features");
  DeferredInit_RunAll();
  LOCK();
  int cnt = g_feature_count;
  UNLOCK();
  log_info("init done, %d features loaded (hot reload dir: build/features)",
           cnt);
  return true;
}

void FeatureLoader_Shutdown(void) {
  LOCK();
  for (int i = 0; i < g_feature_count; i++) {
    if (g_features[i].handle) {
      typedef void (*ShutdownFn)(void);
      ShutdownFn shut = (ShutdownFn)Platform_GetSymbol(g_features[i].handle,
                                                       "FlintFeatureShutdown");
      if (shut)
        shut();
      cap_purge_owner(NULL, g_features[i].path);
      hotkey_purge_owner(NULL, g_features[i].path);
      snippet_purge_owner(NULL, g_features[i].path);
      widget_purge_owner(NULL, g_features[i].path, widget_bump_generation());
      cc_registry_purge_owner(NULL, g_features[i].path);
      g_features[i].uniq[0] = '\0';
      g_features[i].handle = 0;
    }
  }
  for (int i = 0; i < g_retired_count; i++) {
    g_retired[i].uniq[0] = '\0';
    g_retired[i].handle = 0;
  }
  g_retired_count = 0;
  g_feature_count = 0;
  free(g_features);
  g_features = NULL;
  g_feature_cap = 0;
  UNLOCK();
  log_info("FeatureLoader_Shutdown completed");
}

void FeatureLoader_ReloadFrom(const char *name, const char *src_path) {
  if (!name)
    return FeatureLoader_ReloadAll();
  LOCK();
  sweep_retired_libs();
  for (int i = 0; i < g_feature_count; i++) {
    if (strcmp(g_features[i].name, name) == 0) {
      char path_copy[512];
      strncpy(path_copy, g_features[i].path, sizeof(path_copy) - 1);
      path_copy[sizeof(path_copy) - 1] = '\0';
      const char *src = (src_path && src_path[0]) ? src_path : path_copy;
      LibHandle old = g_features[i].handle;
      char uniquepath[1024];
      snprintf(uniquepath, sizeof(uniquepath), "%s.hot%d", path_copy,
               ++g_reload_seq);
      if (!copy_file(src, uniquepath)) {
        log_error("reload %s: copy failed (keeping old)", name);
        UNLOCK();
        return;
      }
      LibHandle h = Platform_LoadLibrary(uniquepath);
      if (!h) {
        log_error("reload %s failed: %s (keeping old)", name,
                  Platform_GetLoadError());
        remove(uniquepath);
        UNLOCK();
        return;
      }
      typedef void (*InitFn)(void);
      InitFn init = (InitFn)Platform_GetSymbol(h, "FlintFeatureInit");
      if (!init)
        init = (InitFn)Platform_GetSymbol(h, "FlintFeatureReload");
      if (!init) {
        log_error("reload %s: no init symbol, keeping old", name);
        Platform_UnloadLibrary(h);
        remove(uniquepath);
        UNLOCK();
        return;
      }
      typedef void (*ShutdownFn)(void);
      ShutdownFn shut =
          (ShutdownFn)Platform_GetSymbol(old, "FlintFeatureShutdown");
      if (shut)
        shut();
      Checkbox_ResetAll();
      unsigned retire_gen = widget_current_generation();
      char retire_owner[512];
      retire_owner[0] = '\0';
      if (g_features[i].uniq[0]) {
        strncpy(retire_owner, g_features[i].uniq, sizeof(retire_owner) - 1);
      } else {
        strncpy(retire_owner, g_features[i].path, sizeof(retire_owner) - 1);
      }
      retire_owner[sizeof(retire_owner) - 1] = '\0';
      retire_old_lib(old, g_features[i].uniq, retire_owner);
      widget_bump_generation();
      log_info("feature_reload: %s (constructor ran at dlopen)", name);
      g_features[i].handle = h;
      strncpy(g_features[i].path, uniquepath, sizeof(g_features[i].path) - 1);
      g_features[i].path[sizeof(g_features[i].path) - 1] = '\0';
      strncpy(g_features[i].uniq, uniquepath, sizeof(g_features[i].uniq) - 1);
      g_features[i].uniq[sizeof(g_features[i].uniq) - 1] = '\0';
      cap_purge_owner(retire_owner, path_copy);
      hotkey_purge_owner(retire_owner, path_copy);
      snippet_purge_owner(retire_owner, path_copy);
      widget_purge_owner(retire_owner, path_copy, retire_gen);
      cc_registry_purge_owner(retire_owner, path_copy);
      sweep_retired_libs();
      UNLOCK();
      DeferredInit_RunAll();
      LOCK();
      log_info("reloaded %s", name);
      UNLOCK();
      return;
    }
  }
  char file[512];
  snprintf(file, sizeof(file), "%s%s", name, Platform_LibExtension());
  bool ok = load_single("build/features", file);
  (void)ok;
  UNLOCK();
  log_info("FeatureLoader_Reload completed");
  return;
}

void FeatureLoader_ReloadAll(void) {
  LOCK();
  for (int i = 0; i < g_feature_count; i++) {
    if (g_features[i].handle) {
      typedef void (*ShutdownFn)(void);
      ShutdownFn shut = (ShutdownFn)Platform_GetSymbol(g_features[i].handle,
                                                       "FlintFeatureShutdown");
      if (shut)
        shut();
      {
        char loaded[512];
        loaded[0] = '\0';
        if (g_features[i].uniq[0]) {
          strncpy(loaded, g_features[i].uniq, sizeof(loaded) - 1);
        } else {
          strncpy(loaded, g_features[i].path, sizeof(loaded) - 1);
        }
        loaded[sizeof(loaded) - 1] = '\0';
        retire_old_lib(g_features[i].handle, g_features[i].uniq, loaded);
      }
      g_features[i].handle = 0;
      g_features[i].uniq[0] = '\0';
    }
  }
  g_feature_count = 0;
  sweep_retired_libs();
  UNLOCK();
  widget_bump_generation();
  FeatureLoader_Init();
  LOCK();
  for (int i = 0; i < g_retired_count; i++) {
    if (g_retired[i].owner[0]) {
      cap_purge_owner(g_retired[i].owner, NULL);
      hotkey_purge_owner(g_retired[i].owner, NULL);
      snippet_purge_owner(g_retired[i].owner, NULL);
      widget_purge_owner(g_retired[i].owner, NULL, g_retired[i].owner_gen);
      cc_registry_purge_owner(g_retired[i].owner, NULL);
    }
  }
  sweep_retired_libs();
  UNLOCK();

  log_info("FeatureLoader_ReloadAll completed");
}

void FeatureLoader_Lock(void) { LOCK(); }

bool FeatureLoader_TryLock(void) {
  feature_mutex_init();
  return Platform_MutexTryLock(g_feature_mutex);
}

void FeatureLoader_Unlock(void) { UNLOCK(); }

void FeatureLoader_Reload(const char *name) {
  FeatureLoader_ReloadFrom(name, NULL);
}
