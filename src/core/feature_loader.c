#include "core/feature_loader.h"
#include "core/lua_host.h"
#include "core/reload_lib.h"
#include "core/manifest.h"
#include "helpers/helpers_key.h"
#include "helpers/helpers_text.h"

#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <utime.h>

#include "deps/log.h"
#include "platform/lib.h"
#include "platform/platform.h"
#include "core/window_manager.h"
#include "core/checkbox.h"
#include "core/slider.h"
#include "config_values.h"
#include "helpers/helpers_hotkey.h"
#include "core/thread_registry.h"
#include "core/owner_purge.h"
#include "core/owner_registry.h"
#include "helpers/helpers_ui.h"
#include "helpers/helpers_render_thread.h"

#include "core/cc_registry.h"
#include "core/widget_registry.h"
#include "core/hover_widget.h"
#include "core/cap_registry.h"
#include "core/snippet_manager.h"
#include "helpers/helpers_fs.h"
#include "helpers/helpers_input_state.h"
#include "deps/stb_ds.h"
#include "helpers/helpers_misc.h"

static void (**deferred_inits)(void);
#define deferred_count arrlenu(deferred_inits)

static void feature_loader_load_lua_all(void);

void deferred_init_register(void (*cb)(void)) {
  if (!cb)
    return;
  arrput(deferred_inits, cb);
}

void deferred_init_run_all(void) {
  void (**cbs)(void) = deferred_inits;
  deferred_inits = NULL;
  for (size_t i = 0; i < arrlenu(cbs); i++) {
    if (cbs[i])
      cbs[i]();
  }
  arrfree(cbs);
}

static void fl_copy(char *dst, size_t dst_size, const char *src) {
  if (dst_size == 0)
    return;
  safe_copy(dst, dst_size, src);
}

#define MAX_PINS 16

typedef struct {
  char *name;
  char *path;
  char *uniq;
  LibHandle handle;
  bool lua;
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
  char *uniq;
  OwnerTag tag;
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

static bool retire_old_lib(LibHandle handle, const char *uniq,
                           const char *owner) {
  if (!handle)
    return true;
  WindowManager *wm = WindowManager_Get();
  RetiredLib r;
  memset(&r, 0, sizeof(r));
  r.handle = handle;
  r.uniq = (uniq && uniq[0]) ? owner_strdup(uniq) : NULL;
  owner_tag_set_owned(&r.tag, (owner && owner[0]) ? owner_strdup(owner) : NULL,
                      widget_current_generation());
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
    owner_strfree(&r.uniq);
    owner_tag_clear(&r.tag);
    return false;
  }
  retired[retired_count++] = r;
  return true;
}

typedef struct {
  LibHandle handle;
  char *uniq;
  char *owner;
} DeferredUnload;

static DeferredUnload *defer_unloads;
#define defer_unload_count arrlenu(defer_unloads)

static void feature_mutex_init(void);
static void *feature_mutex;

int feature_loader_pending_unload_count(void) { return defer_unload_count; }

void feature_loader_drain_retired_unloads(void) {
  if (defer_unload_count == 0)
    return;
  render_thread_sync();
  feature_mutex_init();
  Platform_MutexLock(feature_mutex);
  for (size_t i = 0; i < defer_unload_count; i++) {
    log_info("draining deferred unload of retired lib %s",
             defer_unloads[i].owner ? defer_unloads[i].owner : "");
    Platform_UnloadLibrary(defer_unloads[i].handle);
    if (defer_unloads[i].uniq)
      file_remove_path(defer_unloads[i].uniq);
    owner_strfree(&defer_unloads[i].uniq);
    owner_strfree(&defer_unloads[i].owner);
  }
  arrfree(defer_unloads);
  Platform_MutexUnlock(feature_mutex);
}

static void defer_unload(LibHandle handle, const char *uniq,
                         const char *owner) {
  if (!handle)
    return;
  DeferredUnload *u = arraddnptr(defer_unloads, 1);
  u->handle = handle;
  u->uniq = (uniq && uniq[0]) ? owner_strdup(uniq) : NULL;
  u->owner = (owner && owner[0]) ? owner_strdup(owner) : NULL;
}

static void sweep_retired_libs(void) {
  const WindowManager *wm = WindowManager_Get();
  int n = 0;
  for (int i = 0; i < retired_count; i++) {
    const char *owner = retired[i].tag.owner;
    bool alive = false;
    if (owner && owner[0] && widget_owner_active(owner))
      alive = true;
    else if (owner && owner[0] && thread_registry_owner_active(owner))
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
      char *owner_copy = owner_strdup(owner);
      purge_owner_set(owner_copy, NULL, retired[i].tag.gen);
      log_info("deferring unload of retired lib %s",
               owner_copy ? owner_copy : "");
      defer_unload(retired[i].handle, retired[i].uniq, owner);
      owner_strfree(&owner_copy);
      owner_strfree(&retired[i].uniq);
      owner_tag_clear(&retired[i].tag);
    }
  }
  retired_count = n;
}

static void feature_mutex_init(void) { lazy_mutex_get(&feature_mutex); }

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
  return safe_copy(out, n, dir) && safe_cat(out, n, "/manifest.toml");
}

static atomic_uint manifest_tmp_seq = 0;

static bool manifest_write_full(const char *name, const FeatureManifest *m) {
  char path[512];
  if (!manifest_path(name, path, sizeof(path)))
    return false;
  char tmp[600];
  char suffix[32];
  snprintf(
      suffix, sizeof(suffix), ".tmp.%d.%u", (int)getpid(),
      atomic_fetch_add_explicit(&manifest_tmp_seq, 1, memory_order_relaxed));
  if (!safe_copy(tmp, sizeof(tmp), path) || !safe_cat(tmp, sizeof(tmp), suffix))
    return false;
  FILE *f = fopen(tmp, "wb");
  if (!f) {
    log_error("manifest write: open %s failed (%s)", tmp, strerror(errno));
    return false;
  }
  char enc[512];
  bool ok = true;
  config_toml_encode_string(m->name, enc, sizeof(enc));
  ok = fprintf(f, "name = %s\n", enc) >= 0;
  config_toml_encode_string(m->description, enc, sizeof(enc));
  if (ok)
    ok = fprintf(f, "description = %s\n\n", enc) >= 0;
  if (ok)
    ok = fputs("[provides]\n\n", f) >= 0;
  for (int i = 0; ok && i < m->provide_count; i++) {
    config_toml_encode_string(m->provides[i], enc, sizeof(enc));
    ok = fprintf(f, "%s\n", enc) >= 0;
  }
  if (ok)
    ok = fputs("\n[depends]\n\n", f) >= 0;
  for (int i = 0; ok && i < m->dep_count; i++) {
    config_toml_encode_string(m->deps[i], enc, sizeof(enc));
    ok = fprintf(f, "%s\n", enc) >= 0;
  }
  if (fclose(f) != 0)
    ok = false;
  if (!ok) {
    remove(tmp);
    return false;
  }
  if (!Platform_AtomicRename(tmp, path)) {
    remove(tmp);
    log_error("manifest write: rename %s failed", tmp);
    return false;
  }
  return true;
}

static void manifest_write_baseline(const char *name) {
  char path[512];
  if (!manifest_path(name, path, sizeof(path)))
    return;
  long long mtime = 0, size = 0;
  if (Platform_FileInfo(path, &mtime, &size))
    return;
  char dir[512];
  if (!feature_dir(name, dir, sizeof(dir)))
    return;
  Platform_MkdirIfNotExists(dir);
  FeatureManifest m;
  memset(&m, 0, sizeof(m));
  safe_copy(m.name, sizeof(m.name), name);
  manifest_write_full(name, &m);
  log_info("manifest created for feature '%s'", name);
}

void feature_loader_owner_feature_name(const char *owner_path, char *out,
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
  safe_copy(out, n, base);
  char *dot = strchr(out, '.');
  if (dot)
    *dot = '\0';
}

static void manifest_add_entry(const char *name, const char *section,
                               const char *entry) {
  char path[512];
  if (!manifest_path(name, path, sizeof(path)))
    return;
  FeatureManifest m;
  ConfigTomlStatus st = manifest_parse_file_ex(path, name, &m, NULL);
  if (st == CONFIG_TOML_ERR_IO) {
    manifest_write_baseline(name);
    st = manifest_parse_file_ex(path, name, &m, NULL);
  }
  if (st != CONFIG_TOML_OK) {
    log_error("manifest add: cannot parse %s (%s)", path,
              config_toml_status_name(st));
    return;
  }

  bool provides = strcmp(section, "provides") == 0;
  char value[96];
  if (!safe_copy(value, sizeof(value), entry))
    return;
  int count = provides ? m.provide_count : m.dep_count;
  char *items = provides ? (char *)m.provides : (char *)m.deps;
  size_t item_size = provides ? sizeof(m.provides[0]) : sizeof(m.deps[0]);
  for (int i = 0; i < count; i++) {
    if (strcmp((char *)items + (size_t)i * item_size, value) == 0)
      return;
  }
  if (count >= MANIFEST_LIST_MAX) {
    log_warn("manifest add: %s is full in %s", path, section);
    return;
  }
  safe_copy((char *)items + (size_t)count * item_size, item_size, value);
  if (provides)
    m.provide_count++;
  else
    m.dep_count++;
  manifest_write_full(name, &m);
}

void feature_loader_manifest_add_provides(const char *owner_path,
                                          const char *key) {
  if (!owner_path || !owner_path[0] || !key || !key[0])
    return;
  size_t name_cap = strlen(owner_path) + 1;
  char *name = (char *)malloc(name_cap);
  if (!name)
    return;
  feature_loader_owner_feature_name(owner_path, name, name_cap);
  if (!name[0]) {
    free(name);
    return;
  }
  manifest_add_entry(name, "provides", key);
  free(name);
}

void feature_loader_manifest_add_depends(const char *owner_path,
                                         const char *dep_name) {
  if (!owner_path || !owner_path[0] || !dep_name || !dep_name[0])
    return;
  size_t name_cap = strlen(owner_path) + 1;
  char *name = (char *)malloc(name_cap);
  if (!name)
    return;
  feature_loader_owner_feature_name(owner_path, name, name_cap);
  if (!name[0] || strcmp(name, dep_name) == 0) {
    free(name);
    return;
  }
  manifest_add_entry(name, "depends", dep_name);
  free(name);
}

static bool load_single(const char *dir, const char *filename) {
  if (!ensure_feature_capacity(feature_count + 1))
    return false;
  char full[1024];
  safe_copy(full, sizeof(full), dir);
  safe_cat(full, sizeof(full), "/");
  safe_cat(full, sizeof(full), filename);
  char name[64];
  fl_copy(name, sizeof(name), filename);
  char *dot = strrchr(name, '.');
  if (dot)
    *dot = '\0';

  for (int i = 0; i < feature_count; i++)
    if (features[i].name && strcmp(features[i].name, name) == 0)
      return true;

  LibHandle h = Platform_LoadLibrary(full);
  if (!h) {
    log_error("dlopen %s failed: %s", full, Platform_GetLoadError());
    return false;
  }
  void (*init)(void) =
      (void (*)(void))Platform_GetSymbol(h, "skald_feature_init");
  if (!init)
    init = (void (*)(void))Platform_GetSymbol(h, "SkaldFeatureReload");
  if (init) {
    feature_init_count++;
  } else {
    log_warn("feature %s has no skald_feature_init/reload symbol", name);
  }

  LoadedFeature *f = &features[feature_count++];
  memset(f, 0, sizeof(*f));
  owner_strfree(&f->name);
  f->name = owner_strdup(name);
  owner_strfree(&f->path);
  f->path = owner_strdup(full);
  f->handle = h;
  return true;
}

static bool load_named(const char *dir, const char *ext, const char *name) {
  char filename[128];
  safe_copy(filename, sizeof(filename), name);
  safe_cat(filename, sizeof(filename), ext);
  return load_single(dir, filename);
}

bool feature_loader_load_all_from_dir(const char *dir) {
  LOCK();
  const char *ext = Platform_LibExtension();
  DirIter *d = Platform_DirOpen(dir);
  if (!d) {
    log_error("feature_loader_load_all_from_dir: opendir %s failed", dir);
    UNLOCK();
    return false;
  }

  char (*names)[64] = NULL;
  int names_cap = 0;
  int n_names = 0;
  bool names_oom = false;
  char dname[256];
  bool d_is_dir;
  while (Platform_DirNext(d, dname, sizeof(dname), &d_is_dir)) {
    if (dname[0] == '.')
      continue;
    if (!ends_with(dname, ext))
      continue;
    if (n_names >= names_cap) {
      int cap = names_cap > 0 ? names_cap * 2 : 32;
      char (*grown)[64] =
          (char (*)[64])realloc(names, (size_t)cap * sizeof(names[0]));
      if (!grown) {
        log_error("LoadAllFromDir: out of memory collecting feature list");
        free(names);
        names = NULL;
        n_names = 0;
        names_oom = true;
        break;
      }
      names = grown;
      names_cap = cap;
    }

    char nm[64];
    safe_copy(nm, sizeof(nm), dname);
    char *dot = strrchr(nm, '.');
    if (dot)
      *dot = '\0';
    manifest_write_baseline(nm);
    safe_copy(names[n_names++], sizeof(names[0]), nm);
  }
  Platform_DirClose(d);
  if (names_oom) {
    UNLOCK();
    return false;
  }

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
    mans[i].valid = false;
    char mpath[512];
    if (!manifest_path(names[i], mpath, sizeof(mpath)))
      continue;
    ConfigTomlError merr;
    ConfigTomlStatus mst =
        manifest_parse_file_ex(mpath, names[i], &mans[i], &merr);
    if (mst != CONFIG_TOML_OK || !mans[i].valid) {
      log_error("feature '%s': manifest %s %s (line %d): %s, skipping",
                names[i], mpath, config_toml_status_name(mst), merr.line,
                merr.message[0] ? merr.message : "malformed");
      continue;
    }
    mans[i].valid = true;
  }

  for (int i = 0; i < n_names; i++) {
    if (!mans[i].valid)
      continue;
    const char *missing = NULL;
    for (int k = 0; k < mans[i].dep_count && !missing; k++) {
      bool found = false;
      for (int j = 0; j < n_names; j++) {
        if (strcmp(names[j], mans[i].deps[k]) == 0) {
          found = mans[j].valid;
          break;
        }
      }
      if (!found)
        missing = mans[i].deps[k];
    }
    if (missing) {
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
  feature_loader_load_lua_all();
  log_info("feature_loader_load_all_from_dir completed");
  return true;
}

typedef struct {
  char dir[256];
  time_t src_mtime;
} FailedFeature;

static FailedFeature *s_failed;
#define s_failed_count arrlenu(s_failed)

static void feature_mark_failed(const char *dir, time_t mtime) {
  FailedFeature *f = arraddnptr(s_failed, 1);
  safe_copy(f->dir, sizeof(f->dir), dir);
  f->src_mtime = mtime;
}

typedef struct {
  char name[256];
  long long mtime;
} LuaReloadStamp;

static LuaReloadStamp *g_lua_stamps;
#define g_lua_stamp_count arrlenu(g_lua_stamps)

static long long lua_reload_stamp(const char *name) {
  for (size_t i = 0; i < g_lua_stamp_count; i++)
    if (strcmp(g_lua_stamps[i].name, name) == 0)
      return g_lua_stamps[i].mtime;
  return 0;
}

static void lua_reload_mark(const char *name, long long mtime) {
  for (size_t i = 0; i < g_lua_stamp_count; i++) {
    if (strcmp(g_lua_stamps[i].name, name) == 0) {
      g_lua_stamps[i].mtime = mtime;
      return;
    }
  }
  LuaReloadStamp *stamp = arraddnptr(g_lua_stamps, 1);
  safe_copy(stamp->name, sizeof(stamp->name), name);
  stamp->mtime = mtime;
}

typedef struct {
  char ent_name[256];
  char src[1024];
  char dll[1024];
  char out[1024];
  long long src_mtime;
} FeatureBuildJob;

typedef struct {
  FeatureBuildJob job;
  bool ok;
} FeatureBuildDone;

static void *build_mutex;
static FeatureBuildJob *build_queue;
#define build_queue_count arrlenu(build_queue)
static FeatureBuildDone *build_done;
#define build_done_count arrlenu(build_done)
static int builds_in_flight;
static bool reload_session_active;
static int reload_session_ok;
static int reload_session_fail;
static unsigned build_seq = 0;
static void *build_worker;
static bool build_worker_failed;

static void build_mutex_init(void) { lazy_mutex_get(&build_mutex); }

#define B_LOCK()                                                               \
  do {                                                                         \
    build_mutex_init();                                                        \
    Platform_MutexLock(build_mutex);                                           \
  } while (0)
#define B_UNLOCK() Platform_MutexUnlock(build_mutex)

static void note_feature_built(const char *ent_name, const char *dll,
                               long long src_mtime) {
  for (size_t fi = 0; fi < s_failed_count; fi++) {
    if (strncmp(s_failed[fi].dir, ent_name, sizeof(s_failed[fi].dir)) == 0)
      s_failed[fi].src_mtime = 0;
  }
  struct utimbuf times;
  times.actime = (time_t)src_mtime;
  times.modtime = (time_t)src_mtime;
  utime(dll, &times);
}

static bool persist_hot_lib(const char *out, const char *dll) {
  if (Platform_AtomicRename(out, dll))
    return true;
  if (file_copy_path(out, dll)) {
    remove(out);
    return true;
  }
  log_warn("hot-reload: could not persist %s (will rebuild next start)", dll);
  return false;
}

static int finish_hot_build(const char *ent_name, const char *dll,
                            const char *out, long long src_mtime, bool ok) {
  bool persisted = false;
  if (ok)
    ok = feature_loader_reload_from(ent_name, out);
  if (ok)
    persisted = persist_hot_lib(out, dll);
  remove(out);
  if (!ok) {
    feature_mark_failed(ent_name, (time_t)src_mtime);
    return 0;
  }
  if (persisted)
    note_feature_built(ent_name, dll, src_mtime);
  return 1;
}

static void *feature_build_worker_main(void *unused) {
  (void)unused;
  for (;;) {
    FeatureBuildJob job;
    bool have = false;
    B_LOCK();
    if (build_queue_count > 0) {
      job = build_queue[0];
      arrdel(build_queue, 0);
      have = true;
    }
    B_UNLOCK();
    if (!have) {
      Platform_RenderThreadMiniSleep(25);
      continue;
    }
    bool ok = reload_lib_compile(job.src, job.out, "hot-reload");
    B_LOCK();
    builds_in_flight--;
    FeatureBuildDone *d = arraddnptr(build_done, 1);
    d->job = job;
    d->ok = ok;
    B_UNLOCK();
  }
  return NULL;
}

static bool feature_build_enqueue(const FeatureBuildJob *job) {
  B_LOCK();
  arrput(build_queue, *job);
  builds_in_flight++;
  if (!build_worker && !build_worker_failed) {
    build_worker = Platform_ThreadCreate(feature_build_worker_main, NULL);
    if (!build_worker) {
      builds_in_flight--;
      arrdel(build_queue, build_queue_count - 1);
      build_worker_failed = true;
      log_warn("hot-reload worker failed to start, hot reload disabled");
      B_UNLOCK();
      return false;
    }
  }
  B_UNLOCK();
  return true;
}

void feature_loader_service_builds(void) {
  FeatureBuildDone *done = NULL;
  B_LOCK();
  done = build_done;
  build_done = NULL;
  B_UNLOCK();
  int ok_n = 0, fail_n = 0;
  for (size_t i = 0; i < arrlenu(done); i++) {
    if (finish_hot_build(done[i].job.ent_name, done[i].job.dll, done[i].job.out,
                         done[i].job.src_mtime, done[i].ok))
      ok_n++;
    else
      fail_n++;
  }
  arrfree(done);
  if (!reload_session_active)
    return;
  reload_session_ok += ok_n;
  reload_session_fail += fail_n;
  B_LOCK();
  bool still =
      builds_in_flight != 0 || build_queue_count != 0 || build_done_count != 0;
  B_UNLOCK();
  if (still)
    return;
  reload_session_active = false;
  reload_session_ok = 0;
  reload_session_fail = 0;
}

int feature_loader_build_stale_sources(bool hot_reload) {
  const char *ext = Platform_LibExtension();
  if (!ext)
    return 0;
  if (!FeaturePath || !FeaturePath[0])
    return 0;
  if (!hot_reload) {
    Platform_MkdirIfNotExists("build");
    Platform_MkdirIfNotExists("build/features");
  }
  DirIter *dir = Platform_DirOpen(FeaturePath);
  if (!dir)
    return 0;
  char ent_name[256];
  bool ent_is_dir;
  int done = 0;
  while (Platform_DirNext(dir, ent_name, sizeof(ent_name), &ent_is_dir)) {
    if (ent_name[0] == '.')
      continue;
    char subpath[1024];
    snprintf(subpath, sizeof(subpath), "%.255s/%.255s", FeaturePath, ent_name);
    if (!Platform_DirExists(subpath))
      continue;
    char luasrc[1024];
    snprintf(luasrc, sizeof(luasrc), "%.255s/%.255s.lua", subpath, ent_name);
    long long lua_mtime = 0;
    if (Platform_FileInfo(luasrc, &lua_mtime, NULL) &&
        lua_mtime != lua_reload_stamp(ent_name)) {
      if (hot_reload)
        feature_loader_reload_from(ent_name, luasrc);
      lua_reload_mark(ent_name, lua_mtime);
      if (hot_reload)
        done++;
      continue;
    }

    char src[1024], dll[1024];
    snprintf(src, sizeof(src), "%.255s/%.255s.c", subpath, ent_name);
    snprintf(dll, sizeof(dll), "build/features/%.255s%.8s", ent_name, ext);
    long long src_mtime = 0;
    if (!Platform_FileInfo(src, &src_mtime, NULL))
      continue;
    if (reload_lib_is_fresh(src, dll))
      continue;
    bool known_bad = false;
    for (size_t fi = 0; fi < s_failed_count; fi++) {
      if (s_failed[fi].src_mtime == src_mtime &&
          strncmp(s_failed[fi].dir, ent_name, sizeof(s_failed[fi].dir)) == 0) {
        known_bad = true;
        break;
      }
    }
    if (known_bad)
      continue;
    if (hot_reload) {
      FeatureBuildJob job;
      memset(&job, 0, sizeof(job));
      snprintf(job.ent_name, sizeof(job.ent_name), "%s", ent_name);
      snprintf(job.src, sizeof(job.src), "%s", src);
      snprintf(job.dll, sizeof(job.dll), "%s", dll);
      snprintf(job.out, sizeof(job.out), "%.1000s.tmp.%u", dll, ++build_seq);
      job.src_mtime = src_mtime;
      if (!feature_build_enqueue(&job)) {
        log_warn("feature build queue full, skipping hot reload for %s",
                 ent_name);
        feature_mark_failed(ent_name, src_mtime);
        continue;
      }
      if (!reload_session_active) {
        reload_session_active = true;
        reload_session_ok = 0;
        reload_session_fail = 0;
      }
      done++;
      continue;
    }
    char out_path[1024];
    snprintf(out_path, sizeof(out_path), "%s", dll);
    bool ok = reload_lib_compile(src, out_path, "feature build");
    if (!ok) {
      feature_mark_failed(ent_name, src_mtime);
      continue;
    }
    note_feature_built(ent_name, dll, src_mtime);
    done++;
    log_info("feature build: %s from source", ent_name);
  }
  Platform_DirClose(dir);
  return done;
}

static int feature_build_stale_sources(void) {
  return feature_loader_build_stale_sources(false);
}

bool feature_loader_init(void) {
  LOCK();
  if (feature_count > 0)
    log_warn("feature_loader_init called with %d feature(s) already loaded",
             feature_count);
  feature_count = 0;
  UNLOCK();
  feature_init_count = 0;
  {
    int removed = reload_lib_sweep_stale_hot_libs("build/features",
                                                  Platform_LibExtension());
    if (removed < 0)
      log_warn("init: cannot open build/features, no features will load");
    else if (removed > 0)
      log_info("feature_loader_init: removed %d stale hot-reload lib(s)",
               removed);
  }
  int built = feature_build_stale_sources();
  if (built > 0)
    log_info("feature_loader_init: built %d feature(s) from source", built);
  feature_loader_load_all_from_dir("build/features");
  deferred_init_run_all();
  LOCK();
  int cnt = feature_count;
  UNLOCK();
  log_info("init done, %d features loaded (hot reload dir: build/features)",
           cnt);
  return true;
}

static bool feature_name_loaded(const char *name) {
  for (int i = 0; i < feature_count; i++)
    if (features[i].name && strcmp(features[i].name, name) == 0)
      return true;
  return false;
}

static bool load_lua_named(const char *name) {
  if (!FeaturePath || !FeaturePath[0] || feature_name_loaded(name))
    return true;
  char path[1024];
  snprintf(path, sizeof(path), "%.255s/%.255s/%.255s.lua", FeaturePath, name,
           name);
  FILE *fp = fopen(path, "rb");
  if (!fp)
    return false;
  fclose(fp);

  manifest_write_baseline(name);
  FeatureManifest man = {0};
  char mpath[512];
  ConfigTomlError merr;
  ConfigTomlStatus mst = CONFIG_TOML_ERR_IO;
  if (manifest_path(name, mpath, sizeof(mpath)))
    mst = manifest_parse_file_ex(mpath, name, &man, &merr);
  if (mst == CONFIG_TOML_OK && man.valid) {
    for (int i = 0; i < man.dep_count; i++) {
      if (!feature_name_loaded(man.deps[i])) {
        log_warn("lua feature '%s' skipped, dep '%s' not loaded", name,
                 man.deps[i]);
        return false;
      }
    }
  }

  if (!slua_load(name, path)) {
    log_error("lua feature '%s' failed to load", name);
    return false;
  }
  if (!ensure_feature_capacity(feature_count + 1))
    return false;
  LoadedFeature *f = &features[feature_count++];
  memset(f, 0, sizeof(*f));
  f->name = owner_strdup(name);
  f->path = owner_strdup(path);
  f->handle = (LibHandle)1;
  f->lua = true;
  feature_init_count++;
  return true;
}

static void feature_loader_load_lua_all(void) {
  if (!FeaturePath || !FeaturePath[0])
    return;
  DirIter *d = Platform_DirOpen(FeaturePath);
  if (!d)
    return;
  char dname[256];
  bool is_dir = false;
  while (Platform_DirNext(d, dname, sizeof(dname), &is_dir)) {
    if (!is_dir || dname[0] == '.')
      continue;
    load_lua_named(dname);
  }
  Platform_DirClose(d);
}

static void call_feature_shutdown(LibHandle handle) {
  void (*shut)(void) =
      (void (*)(void))Platform_GetSymbol(handle, "skald_feature_shutdown");
  if (shut)
    shut();
}

static void purge_failed_library(LibHandle h, const char *path) {
  void (*init)(void) =
      (void (*)(void))Platform_GetSymbol(h, "skald_feature_init");
  if (!init)
    init = (void (*)(void))Platform_GetSymbol(h, "SkaldFeatureReload");
  char *owner = init ? owner_resolve_for_func_alloc((void *)init) : NULL;
  if (!owner || !owner[0]) {
    owner_strfree(&owner);
    owner = owner_strdup(path);
  }
  if (owner && owner[0])
    purge_owner_set(owner, path, widget_current_generation());
  owner_strfree(&owner);
}

void feature_loader_shutdown(void) {
  LOCK();
  int count = feature_count;
  LibHandle *handles = NULL;
  if (count > 0) {
    handles = (LibHandle *)calloc((size_t)count, sizeof(*handles));
    if (!handles)
      log_error("feature_loader_shutdown: cannot snapshot feature handles");
  }
  for (int i = 0; i < count && handles; i++)
    handles[i] = features[i].handle;
  UNLOCK();

  if (handles) {
    for (int i = 0; i < count; i++) {
      if (features[i].lua)
        slua_shutdown(features[i].name);
      else if (handles[i])
        call_feature_shutdown(handles[i]);
    }
    free(handles);
  }

  LOCK();
  for (int i = 0; i < feature_count; i++) {
    if (features[i].handle) {
      char *path_copy = owner_strdup(features[i].path);
      purge_owner_set(NULL, path_copy, widget_bump_generation());
      owner_strfree(&path_copy);
      if (!features[i].lua)
        Platform_UnloadLibrary(features[i].handle);
      if (features[i].uniq)
        file_remove_path(features[i].uniq);
      features[i].handle = 0;
    }
    owner_strfree(&features[i].name);
    owner_strfree(&features[i].path);
    owner_strfree(&features[i].uniq);
  }
  for (int i = 0; i < retired_count; i++) {
    char *owner_copy = owner_strdup(retired[i].tag.owner);
    purge_owner_set(owner_copy, NULL, retired[i].tag.gen);
    owner_strfree(&owner_copy);
    Platform_UnloadLibrary(retired[i].handle);
    if (retired[i].uniq)
      file_remove_path(retired[i].uniq);
    owner_strfree(&retired[i].uniq);
    owner_tag_clear(&retired[i].tag);
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

  feature_loader_drain_retired_unloads();
  log_info("feature_loader_shutdown completed");
}

bool feature_loader_reload_from(const char *name, const char *src_path) {
  if (!name) {
    feature_loader_reload_all();
    return true;
  }
  LOCK();
  sweep_retired_libs();
  for (int i = 0; i < feature_count; i++) {
    if (features[i].name && strcmp(features[i].name, name) == 0) {
      if (features[i].lua) {
        UNLOCK();
        return slua_reload(name);
      }
      char *path_copy = owner_strdup(features[i].path);
      const char *base = path_copy ? path_copy : "";
      const char *src = (src_path && src_path[0]) ? src_path : base;
      LibHandle old = features[i].handle;
      char *uniquepath = reload_lib_unique_copy(src, base, ++reload_seq, NULL);
      if (!uniquepath) {
        log_error("reload %s: copy failed (keeping old)", name);
        owner_strfree(&path_copy);
        UNLOCK();
        return false;
      }
      LibHandle h = Platform_LoadLibrary(uniquepath);
      if (h)
        file_remove_path(uniquepath);
      if (!h) {
        log_error("reload %s failed: %s (keeping old)", name,
                  Platform_GetLoadError());
        file_remove_path(uniquepath);
        free(uniquepath);
        owner_strfree(&path_copy);
        UNLOCK();
        return false;
      }
      void (*init)(void) =
          (void (*)(void))Platform_GetSymbol(h, "skald_feature_init");
      if (!init)
        init = (void (*)(void))Platform_GetSymbol(h, "SkaldFeatureReload");
      if (!init) {
        log_error("reload %s: no init symbol, keeping old", name);
        purge_failed_library(h, uniquepath);
        Platform_UnloadLibrary(h);
        file_remove_path(uniquepath);
        free(uniquepath);
        owner_strfree(&path_copy);
        UNLOCK();
        return false;
      }
      unsigned retire_gen = widget_current_generation();
      char *retire_owner = owner_strdup(
          (features[i].uniq && features[i].uniq[0]) ? features[i].uniq
                                                    : features[i].path);
      if (!retire_old_lib(old, features[i].uniq, retire_owner)) {
        log_error("reload %s: cannot retire old lib, keeping it", name);
        purge_failed_library(h, uniquepath);
        Platform_UnloadLibrary(h);
        file_remove_path(uniquepath);
        free(uniquepath);
        owner_strfree(&retire_owner);
        owner_strfree(&path_copy);
        UNLOCK();
        return false;
      }
      void (*shut)(void) =
          (void (*)(void))Platform_GetSymbol(old, "skald_feature_shutdown");
      if (shut)
        shut();
      widget_bump_generation();
      log_info("feature_reload: %s (constructor ran at dlopen)", name);
      features[i].handle = h;
      owner_strfree(&features[i].uniq);
      features[i].uniq = owner_strdup(uniquepath);
      free(uniquepath);
      purge_owner_set(retire_owner, path_copy, retire_gen);
      owner_strfree(&retire_owner);
      owner_strfree(&path_copy);
      sweep_retired_libs();
      UNLOCK();
      deferred_init_run_all();
      LOCK();
      log_info("reloaded %s", name);
      UNLOCK();
      return true;
    }
  }
  if (src_path && src_path[0]) {
    if (!ensure_feature_capacity(feature_count + 1)) {
      log_error("reload %s: feature table full, new feature not loaded", name);
      UNLOCK();
      return false;
    }
    manifest_write_baseline(name);
    char new_src[512];
    fl_copy(new_src, sizeof(new_src), src_path);
    char *uniquepath =
        reload_lib_unique_copy(src_path, new_src, ++reload_seq, NULL);
    if (!uniquepath) {
      log_error("reload %s: copy of new build failed", name);
      UNLOCK();
      return false;
    }
    LibHandle h = Platform_LoadLibrary(uniquepath);
    if (h)
      file_remove_path(uniquepath);
    if (!h) {
      log_error("reload %s failed: %s (new feature, not loaded)", name,
                Platform_GetLoadError());
      file_remove_path(uniquepath);
      free(uniquepath);
      UNLOCK();
      return false;
    }
    void (*init)(void) =
        (void (*)(void))Platform_GetSymbol(h, "skald_feature_init");
    if (!init)
      init = (void (*)(void))Platform_GetSymbol(h, "SkaldFeatureReload");
    if (!init) {
      log_error("reload %s: no init symbol, new feature not loaded", name);
      purge_failed_library(h, uniquepath);
      Platform_UnloadLibrary(h);
      file_remove_path(uniquepath);
      free(uniquepath);
      UNLOCK();
      return false;
    }
    const char *libext = Platform_LibExtension();
    char dll[512];
    snprintf(dll, sizeof(dll), "build/features/%.255s%.8s", name,
             libext ? libext : "");
    LoadedFeature *f = &features[feature_count++];
    memset(f, 0, sizeof(*f));
    owner_strfree(&f->name);
    f->name = owner_strdup(name);
    owner_strfree(&f->path);
    f->path = owner_strdup(dll);
    owner_strfree(&f->uniq);
    f->uniq = owner_strdup(uniquepath);
    f->handle = h;
    feature_init_count++;
    if (!file_copy_path(uniquepath, dll))
      log_warn("reload %s: could not persist %s (will rebuild next start)",
               name, dll);
    free(uniquepath);
    UNLOCK();
    deferred_init_run_all();
    LOCK();
    log_info("reloaded %s (new feature)", name);
    UNLOCK();
    return true;
  }
  char file[512];
  snprintf(file, sizeof(file), "%s%s", name, Platform_LibExtension());
  bool ok = load_single("build/features", file);
  UNLOCK();
  log_info("feature_loader_reload completed");
  return ok;
}

void feature_loader_reload_all(void) {
  LOCK();
  int kept = 0;
  for (int i = 0; i < feature_count; i++) {
    if (features[i].lua) {
      char *name = owner_strdup(features[i].name);
      UNLOCK();
      slua_reload(name);
      free(name);
      LOCK();
      continue;
    }
    if (features[i].handle) {
      char *loaded = owner_strdup((features[i].uniq && features[i].uniq[0])
                                      ? features[i].uniq
                                      : features[i].path);
      if (!retire_old_lib(features[i].handle, features[i].uniq, loaded)) {
        log_error("reload_all: cannot retire '%s', keeping old library",
                  loaded ? loaded : features[i].name);
        owner_strfree(&loaded);
        features[kept++] = features[i];
        continue;
      }
      call_feature_shutdown(features[i].handle);
      owner_strfree(&loaded);
      features[i].handle = 0;
    }
    owner_strfree(&features[i].name);
    owner_strfree(&features[i].path);
    owner_strfree(&features[i].uniq);
  }
  feature_count = kept;
  sweep_retired_libs();
  UNLOCK();
  widget_bump_generation();
  feature_loader_init();
  LOCK();
  sweep_retired_libs();
  UNLOCK();

  log_info("feature_loader_reload_all completed");
}

void feature_loader_lock(void) { LOCK(); }

void feature_loader_sweep_retired(void) {
  feature_mutex_init();
  if (!Platform_MutexTryLock(feature_mutex))
    return;
  sweep_retired_libs();
  Platform_MutexUnlock(feature_mutex);
}

bool feature_loader_try_lock(void) {
  feature_mutex_init();
  return Platform_MutexTryLock(feature_mutex);
}

void feature_loader_unlock(void) { UNLOCK(); }

void feature_loader_reload(const char *name) {
  (void)feature_loader_reload_from(name, NULL);
}
