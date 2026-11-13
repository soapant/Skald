#include "core/feature_loader.h"
#include "core/manifest.h"
#include "helpers/helpers_key.h"
#include "helpers/helpers_fs.h"
#include "helpers/helpers_text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <utime.h>

#include "deps/log.h"
#include "platform/lib.h"
#include "platform/platform.h"
#include "core/window_manager.h"
#include "core/checkbox.h"
#include "config_values.h"
#include "helpers/helpers_hotkey.h"
#include "core/thread_registry.h"

#include "core/cc_registry.h"
#include "core/widget_registry.h"
#include "core/hover_widget.h"
#include "core/cap_registry.h"
#include "core/snippet_manager.h"
#include "helpers/helpers_input_state.h"

#define DEFERRED_INIT_MAX 32
static void (*deferred_inits[DEFERRED_INIT_MAX])(void);
static int deferred_count = 0;

void DeferredInit_Register(void (*cb)(void)) {
  if (!cb)
    return;
  if (deferred_count >= DEFERRED_INIT_MAX) {
    log_error("deferred init queue full (%d), dropping registration",
              DEFERRED_INIT_MAX);
    return;
  }
  deferred_inits[deferred_count++] = cb;
}

void DeferredInit_RunAll(void) {
  for (int i = 0; i < deferred_count; i++) {
    void (*cb)(void) = deferred_inits[i];
    deferred_inits[i] = NULL;
    cb();
  }
  deferred_count = 0;
}

static void fl_copy(char *dst, size_t dst_size, const char *src) {
  if (dst_size == 0)
    return;
  SafeCopy(dst, dst_size, src);
}

#define MAX_PINS 16

typedef struct {
  char name[64];
  char path[512];
  char uniq[512];
  LibHandle handle;
} LoadedFeature;

static int reload_seq = 0;

static LoadedFeature *features = NULL;
static int feature_count = 0;
static int feature_cap = 0;

static bool ensure_feature_capacity(int needed) {
  if (needed <= feature_cap)
    return true;
  int cap = feature_cap > 0 ? feature_cap : 16;
  while (cap < needed)
    cap *= 2;
  LoadedFeature *grown =
      (LoadedFeature *)realloc(features, (size_t)cap * sizeof(*features));
  if (!grown) {
    log_error("out of memory growing feature table to %d", cap);
    return false;
  }
  features = grown;
  feature_cap = cap;
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
static RetiredLib *retired = NULL;
static int retired_count = 0;
static int retired_cap = 0;

static bool ensure_retired_capacity(int needed) {
  if (needed <= retired_cap)
    return true;
  int cap = retired_cap > 0 ? retired_cap : 16;
  while (cap < needed)
    cap *= 2;
  RetiredLib *grown =
      (RetiredLib *)realloc(retired, (size_t)cap * sizeof(*retired));
  if (!grown) {
    log_error("out of memory growing retired-lib table to %d", cap);
    return false;
  }
  retired = grown;
  retired_cap = cap;
  return true;
}

static void retire_old_lib(LibHandle handle, const char *uniq,
                           const char *owner) {
  if (!handle)
    return;
  WindowManager *wm = WindowManager_Get();
  RetiredLib r;
  r.handle = handle;
  r.uniq[0] = '\0';
  if (uniq && uniq[0])
    fl_copy(r.uniq, sizeof(r.uniq), uniq);
  r.owner_gen = Widget_CurrentGeneration();
  r.owner[0] = '\0';
  if (owner && owner[0])
    fl_copy(r.owner, sizeof(r.owner), owner);
  r.npins = 0;
  if (wm && wm->popup_count > MAX_PINS) {
    r.npins = -1;
  } else if (wm) {
    for (int i = 0; i < wm->popup_count; i++)
      r.pins[r.npins++] = wm->popups[i];
  }
  if (!ensure_retired_capacity(retired_count + 1)) {
    log_error("retire: cannot track lib %s, leaving it loaded",
              owner ? owner : "?");
    return;
  }
  retired[retired_count++] = r;
}

typedef struct {
  LibHandle handle;
  char uniq[512];
  char owner[512];
} DeferredUnload;

static DeferredUnload defer_unloads[DEFERRED_INIT_MAX];
static int defer_unload_count = 0;

static void feature_mutex_init(void);
static void *feature_mutex;

int FeatureLoader_PendingUnloadCount(void) { return defer_unload_count; }

void FeatureLoader_DrainRetiredUnloads(void) {
  if (defer_unload_count == 0)
    return;
  feature_mutex_init();
  Platform_MutexLock(feature_mutex);
  for (int i = 0; i < defer_unload_count; i++) {
    log_info("draining deferred unload of retired lib %s",
             defer_unloads[i].owner);
    Platform_UnloadLibrary(defer_unloads[i].handle);
    if (defer_unloads[i].uniq[0])
      remove(defer_unloads[i].uniq);
  }
  defer_unload_count = 0;
  Platform_MutexUnlock(feature_mutex);
}

static void defer_unload(LibHandle handle, const char *uniq,
                         const char *owner) {
  if (!handle)
    return;
  if (defer_unload_count >= DEFERRED_INIT_MAX) {
    log_error("deferred unload list full, unloading %s inline",
              owner ? owner : "?");
    Platform_UnloadLibrary(handle);
    if (uniq && uniq[0])
      remove(uniq);
    return;
  }
  DeferredUnload *u = &defer_unloads[defer_unload_count++];
  u->handle = handle;
  u->uniq[0] = '\0';
  u->owner[0] = '\0';
  if (uniq && uniq[0])
    SafeCopy(u->uniq, sizeof(u->uniq), uniq);
  if (owner && owner[0])
    SafeCopy(u->owner, sizeof(u->owner), owner);
}

static void sweep_retired_libs(void) {
  WindowManager *wm = WindowManager_Get();
  Checkbox_ResetAll();
  int n = 0;
  for (int i = 0; i < retired_count; i++) {
    bool alive = false;
    if (retired[i].owner[0] && Widget_OwnerActive(retired[i].owner))
      alive = true;
    else if (retired[i].owner[0] &&
             ThreadRegistry_OwnerActive(retired[i].owner))
      alive = true;
    else if (wm && retired[i].npins >= 0) {
      for (int a = 0; a < retired[i].npins && !alive; a++) {
        for (int b = 0; b < wm->popup_count; b++)
          if (wm->popups[b] == retired[i].pins[a]) {
            alive = true;
            break;
          }
      }
    } else if (wm && wm->popup_count > 0)
      alive = true;
    if (alive) {
      retired[n++] = retired[i];
    } else {
      Cap_PurgeOwner(retired[i].owner, NULL);
      Hotkey_PurgeOwner(retired[i].owner, NULL);
      Snippet_PurgeOwner(retired[i].owner, NULL);
      Widget_PurgeOwner(retired[i].owner, NULL, retired[i].owner_gen);
      CcRegistry_PurgeOwner(retired[i].owner, NULL);
      FeatureList_PurgeOwner(retired[i].owner, NULL);
      KeyCallback_PurgeOwner(retired[i].owner, NULL);
      Key_PurgeInterceptorOwner(retired[i].owner, NULL);
      EventBus_PurgeOwner(retired[i].owner, NULL);
      HoverWidget_PurgeOwner(retired[i].owner, NULL);
      Platform_MessagePopup_PurgeOwner(retired[i].owner, NULL);
      log_info("deferring unload of retired lib %s", retired[i].owner);
      defer_unload(retired[i].handle, retired[i].uniq, retired[i].owner);
    }
  }
  retired_count = n;
}

static void feature_mutex_init(void) {
  if (!feature_mutex)
    feature_mutex = Platform_MutexCreate();
}
#define LOCK()                                                                 \
  do {                                                                         \
    feature_mutex_init();                                                      \
    Platform_MutexLock(feature_mutex);                                         \
  } while (0)
#define UNLOCK() Platform_MutexUnlock(feature_mutex)

static bool ends_with(const char *str, const char *suffix) {
  if (!str || !suffix)
    return false;
  size_t sl = strlen(str), su = strlen(suffix);
  if (su > sl)
    return false;
  return strcmp(str + sl - su, suffix) == 0;
}

static int feature_init_count = 0;

static bool warned_no_featurepath = false;

static bool feature_dir(const char *name, char *out, size_t n) {
  if (!FeaturePath || !FeaturePath[0]) {
    if (!warned_no_featurepath) {
      log_warn("FeaturePath is empty, feature asset lookup disabled "
               "(check config)");
      warned_no_featurepath = true;
    }
    return false;
  }
  if (!name || !name[0])
    return false;
  snprintf(out, n, "%s/%s", FeaturePath, name);
  return true;
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
  if (!f) {
    log_error("manifest baseline: open %s for write failed (%s)", path,
              strerror(errno));
    return;
  }
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
  if (bslash && (!base || bslash > base))
    base = bslash;
  base = base ? base + 1 : owner_path;
  SafeCopy(out, n, base);
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
    if (fseek(f, 0, SEEK_END) == 0) {
      long sz = ftell(f);
      if (sz > (long)(sizeof(buf) - 1)) {
        log_error("manifest '%s' is %ld bytes, too large to rewrite (%d)", path,
                  sz, (int)(sizeof(buf) - 1));
        fclose(f);
        return;
      }
      rewind(f);
    }
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
    SafeCopy(work, sizeof(work), line);
    Manifest_Trim(work);

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
      SafeCopy(e, sizeof(e), work);
      Manifest_Unquote(e);
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
  if (!f) {
    log_error("manifest add: open %s for write failed (%s)", path,
              strerror(errno));
    return;
  }
  if (fwrite(buf, 1, len, f) != len)
    log_error("manifest add: write %s incomplete (%s)", path, strerror(errno));
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

static bool load_single(const char *dir, const char *filename) {
  if (!ensure_feature_capacity(feature_count + 1))
    return false;
  char full[1024];
  snprintf(full, sizeof(full), "%s/%s", dir, filename);
  char name[64];
  fl_copy(name, sizeof(name), filename);
  char *dot = strrchr(name, '.');
  if (dot)
    *dot = '\0';

  for (int i = 0; i < feature_count; i++)
    if (strcmp(features[i].name, name) == 0)
      return true;

  LibHandle h = Platform_LoadLibrary(full);
  if (!h) {
    log_error("dlopen %s failed: %s", full, Platform_GetLoadError());
    return false;
  }
  typedef void (*InitFn)(void);
  InitFn init = (InitFn)Platform_GetSymbol(h, "SkaldFeatureInit");
  if (!init)
    init = (InitFn)Platform_GetSymbol(h, "SkaldFeatureReload");
  if (init) {
    feature_init_count++;
  } else {
    log_warn("feature %s has no SkaldFeatureInit/Reload symbol", name);
  }

  LoadedFeature *f = &features[feature_count++];
  memset(f, 0, sizeof(*f));
  SafeCopy(f->name, sizeof(f->name), name);
  SafeCopy(f->path, sizeof(f->path), full);
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
    log_error("FeatureLoader_LoadAllFromDir: opendir %s failed (%s)", dir,
              strerror(errno));
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
    SafeCopy(nm, sizeof(nm), e->d_name);
    char *dot = strrchr(nm, '.');
    if (dot)
      *dot = '\0';
    manifest_write_baseline(nm);
    SafeCopy(names[n_names++], sizeof(names[0]), nm);
  }
  closedir(d);

  FeatureManifest *mans =
      (FeatureManifest *)calloc((size_t)n_names, sizeof(FeatureManifest));
  bool *loaded = (bool *)calloc((size_t)n_names, sizeof(bool));
  if (!mans || !loaded) {
    log_error("out of memory allocating manifest arrays (%d features)",
              n_names);
    free(mans);
    free(loaded);
    UNLOCK();
    return false;
  }
  for (int i = 0; i < n_names; i++) {
    char mpath[512];
    if (manifest_path(names[i], mpath, sizeof(mpath)))
      Manifest_ParseFile(mpath, names[i], &mans[i]);
  }

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
      printf("skald: skipping feature '%s' - missing dependency '%s'\n",
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

static struct {
  char dir[256];
  time_t src_mtime;
} s_failed[64];
static int s_failed_count = 0;

static void feature_mark_failed(const char *dir, time_t mtime) {
  if (s_failed_count < 64) {
    SafeCopy(s_failed[s_failed_count].dir,
             sizeof(s_failed[s_failed_count].dir), dir);
    s_failed[s_failed_count].src_mtime = mtime;
    s_failed_count++;
  }
}

int FeatureLoader_BuildStaleSources(bool hot_reload) {
  const char *ext = Platform_LibExtension();
  if (!ext)
    return 0;
  if (!FeaturePath || !FeaturePath[0])
    return 0;
  if (!hot_reload) {
    Platform_MkdirIfNotExists("build");
    Platform_MkdirIfNotExists("build/features");
  }
  DIR *dir = opendir(FeaturePath);
  if (!dir)
    return 0;
  struct dirent *ent;
  int done = 0;
  while ((ent = readdir(dir)) != NULL) {
    if (ent->d_name[0] == '.')
      continue;
    char subpath[512];
    snprintf(subpath, sizeof(subpath), "%s/%s", FeaturePath, ent->d_name);
    struct stat st;
    if (stat(subpath, &st) != 0 || !S_ISDIR(st.st_mode))
      continue;
    char src[512], dll[512];
    snprintf(src, sizeof(src), "%s/%s.c", subpath, ent->d_name);
    snprintf(dll, sizeof(dll), "build/features/%s%s", ent->d_name, ext);
    struct stat st_src, st_dll;
    if (stat(src, &st_src) != 0)
      continue;
    if (stat(dll, &st_dll) == 0 && st_src.st_mtime <= st_dll.st_mtime)
      continue;
    bool known_bad = false;
    for (int fi = 0; fi < s_failed_count; fi++) {
      if (s_failed[fi].src_mtime == st_src.st_mtime &&
          strncmp(s_failed[fi].dir, ent->d_name, sizeof(s_failed[fi].dir)) ==
              0) {
        known_bad = true;
        break;
      }
    }
    if (known_bad)
      continue;
    static unsigned build_seq = 0;
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s.tmp.%u", dll, ++build_seq);
    const char *argv[PLATFORM_BUILD_ARGV_MAX];
    if (!Platform_GetFeatureBuildArgv(src, tmp, argv,
                                      PLATFORM_BUILD_ARGV_MAX)) {
      remove(tmp);
      continue;
    }
    int status = -1;
    char *outStr = Platform_RunArgvCapture(argv, NULL, &status);
    bool ok = outStr != NULL && status == 0;
    if (!ok) {
      if (outStr) {
        log_error("%s %s failed (exit %d):\n%s",
                  hot_reload ? "hot-reload" : "feature build", ent->d_name,
                  status, outStr);
        free(outStr);
      } else
        log_error("%s %s failed: could not run compiler",
                  hot_reload ? "hot-reload" : "feature build", ent->d_name);
    } else
      free(outStr);
    if (!ok) {
      remove(tmp);
      feature_mark_failed(ent->d_name, st_src.st_mtime);
      continue;
    }
    if (hot_reload) {
      bool reloaded_ok = FeatureLoader_ReloadFrom(ent->d_name, tmp);
      remove(tmp);
      if (!reloaded_ok) {
        feature_mark_failed(ent->d_name, st_src.st_mtime);
        continue;
      }
    } else {
      if (!Platform_AtomicRename(tmp, dll)) {
        log_error("feature build %s: rename %s -> %s failed", ent->d_name, tmp,
                  dll);
        remove(tmp);
        continue;
      }
    }
    for (int fi = 0; fi < s_failed_count; fi++) {
      if (strncmp(s_failed[fi].dir, ent->d_name, sizeof(s_failed[fi].dir)) == 0)
        s_failed[fi].src_mtime = 0;
    }
    struct utimbuf times;
    times.actime = st_src.st_mtime;
    times.modtime = st_src.st_mtime;
    utime(dll, &times);
    done++;
    if (!hot_reload)
      log_info("feature build: %s from source", ent->d_name);
  }
  closedir(dir);
  return done;
}

static int feature_build_stale_sources(void) {
  return FeatureLoader_BuildStaleSources(false);
}

bool FeatureLoader_Init(void) {
  LOCK();
  feature_count = 0;
  UNLOCK();
  feature_init_count = 0;
  {
    DIR *d = opendir("build/features");
    if (!d)
      log_warn("init: cannot open build/features (%s), no features will load",
               strerror(errno));
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
  int built = feature_build_stale_sources();
  if (built > 0)
    log_info("FeatureLoader_Init: built %d feature(s) from source", built);
  FeatureLoader_LoadAllFromDir("build/features");
  DeferredInit_RunAll();
  LOCK();
  int cnt = feature_count;
  UNLOCK();
  log_info("init done, %d features loaded (hot reload dir: build/features)",
           cnt);
  return true;
}

static void call_feature_shutdown(LibHandle handle) {
  typedef void (*ShutdownFn)(void);
  ShutdownFn shut =
      (ShutdownFn)Platform_GetSymbol(handle, "SkaldFeatureShutdown");
  if (shut)
    shut();
}

void FeatureLoader_Shutdown(void) {
  LOCK();
  for (int i = 0; i < feature_count; i++) {
    if (features[i].handle) {
      call_feature_shutdown(features[i].handle);
      Cap_PurgeOwner(NULL, features[i].path);
      Hotkey_PurgeOwner(NULL, features[i].path);
      Snippet_PurgeOwner(NULL, features[i].path);
      Widget_PurgeOwner(NULL, features[i].path, Widget_BumpGeneration());
      CcRegistry_PurgeOwner(NULL, features[i].path);
      FeatureList_PurgeOwner(NULL, features[i].path);
      features[i].uniq[0] = '\0';
      features[i].handle = 0;
    }
  }
  free(retired);
  retired = NULL;
  retired_count = 0;
  retired_cap = 0;
  feature_count = 0;
  free(features);
  features = NULL;
  feature_cap = 0;
  UNLOCK();
  log_info("FeatureLoader_Shutdown completed");
}

bool FeatureLoader_ReloadFrom(const char *name, const char *src_path) {
  if (!name) {
    FeatureLoader_ReloadAll();
    return true;
  }
  LOCK();
  sweep_retired_libs();
  for (int i = 0; i < feature_count; i++) {
    if (strcmp(features[i].name, name) == 0) {
      char path_copy[512];
      fl_copy(path_copy, sizeof(path_copy), features[i].path);
      const char *src = (src_path && src_path[0]) ? src_path : path_copy;
      LibHandle old = features[i].handle;
      char uniquepath[1024];
      snprintf(uniquepath, sizeof(uniquepath), "%s.hot%d", path_copy,
               ++reload_seq);
      if (!File_CopyPath(src, uniquepath)) {
        log_error("reload %s: copy failed (keeping old)", name);
        UNLOCK();
        return false;
      }
      LibHandle h = Platform_LoadLibrary(uniquepath);
      if (!h) {
        log_error("reload %s failed: %s (keeping old)", name,
                  Platform_GetLoadError());
        remove(uniquepath);
        UNLOCK();
        return false;
      }
      typedef void (*InitFn)(void);
      InitFn init = (InitFn)Platform_GetSymbol(h, "SkaldFeatureInit");
      if (!init)
        init = (InitFn)Platform_GetSymbol(h, "SkaldFeatureReload");
      if (!init) {
        log_error("reload %s: no init symbol, keeping old", name);
        Platform_UnloadLibrary(h);
        remove(uniquepath);
        UNLOCK();
        return false;
      }
      typedef void (*ShutdownFn)(void);
      ShutdownFn shut =
          (ShutdownFn)Platform_GetSymbol(old, "SkaldFeatureShutdown");
      if (shut)
        shut();
      Checkbox_ResetAll();
      unsigned retire_gen = Widget_CurrentGeneration();
      char retire_owner[512];
      fl_copy(retire_owner, sizeof(retire_owner),
              features[i].uniq[0] ? features[i].uniq : features[i].path);
      retire_old_lib(old, features[i].uniq, retire_owner);
      Widget_BumpGeneration();
      log_info("feature_reload: %s (constructor ran at dlopen)", name);
      features[i].handle = h;
      fl_copy(features[i].path, sizeof(features[i].path), uniquepath);
      fl_copy(features[i].uniq, sizeof(features[i].uniq), uniquepath);
      Cap_PurgeOwner(retire_owner, path_copy);
      Hotkey_PurgeOwner(retire_owner, path_copy);
      Snippet_PurgeOwner(retire_owner, path_copy);
      Widget_PurgeOwner(retire_owner, path_copy, retire_gen);
      CcRegistry_PurgeOwner(retire_owner, path_copy);
      FeatureList_PurgeOwner(retire_owner, path_copy);
      KeyCallback_PurgeOwner(retire_owner, path_copy);
      Key_PurgeInterceptorOwner(retire_owner, path_copy);
      EventBus_PurgeOwner(retire_owner, path_copy);
      HoverWidget_PurgeOwner(retire_owner, path_copy);
      Platform_MessagePopup_PurgeOwner(retire_owner, path_copy);
      sweep_retired_libs();
      UNLOCK();
      DeferredInit_RunAll();
      LOCK();
      log_info("reloaded %s", name);
      UNLOCK();
      return true;
    }
  }
  char file[512];
  snprintf(file, sizeof(file), "%s%s", name, Platform_LibExtension());
  bool ok = load_single("build/features", file);
  UNLOCK();
  log_info("FeatureLoader_Reload completed");
  return ok;
}

void FeatureLoader_ReloadAll(void) {
  LOCK();
  for (int i = 0; i < feature_count; i++) {
    if (features[i].handle) {
      call_feature_shutdown(features[i].handle);
      {
        char loaded[512];
        fl_copy(loaded, sizeof(loaded),
                features[i].uniq[0] ? features[i].uniq : features[i].path);
        retire_old_lib(features[i].handle, features[i].uniq, loaded);
      }
      features[i].handle = 0;
      features[i].uniq[0] = '\0';
    }
  }
  feature_count = 0;
  sweep_retired_libs();
  UNLOCK();
  Widget_BumpGeneration();
  FeatureLoader_Init();
  LOCK();
  sweep_retired_libs();
  UNLOCK();

  log_info("FeatureLoader_ReloadAll completed");
}

void FeatureLoader_Lock(void) { LOCK(); }

void FeatureLoader_SweepRetired(void) {
  feature_mutex_init();
  if (!Platform_MutexTryLock(feature_mutex))
    return;
  sweep_retired_libs();
  Platform_MutexUnlock(feature_mutex);
}

bool FeatureLoader_TryLock(void) {
  feature_mutex_init();
  return Platform_MutexTryLock(feature_mutex);
}

void FeatureLoader_Unlock(void) { UNLOCK(); }

void FeatureLoader_Reload(const char *name) {
  (void)FeatureLoader_ReloadFrom(name, NULL);
}
