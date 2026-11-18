#include "core/feature_loader.h"
#include "core/reload_lib.h"
#include "core/manifest.h"
#include "helpers/helpers_key.h"
#include "helpers/helpers_text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
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
#include "helpers/helpers_ui.h"

#include "core/cc_registry.h"
#include "core/widget_registry.h"
#include "core/hover_widget.h"
#include "core/cap_registry.h"
#include "core/snippet_manager.h"
#include "helpers/helpers_fs.h"
#include "helpers/helpers_input_state.h"

#define DEFERRED_INIT_MAX 32
static void (*deferred_inits[DEFERRED_INIT_MAX])(void);
static int deferred_count = 0;

void deferred_init_register(void (*cb)(void)) {
  if (!cb)
    return;
  if (deferred_count >= DEFERRED_INIT_MAX) {
    log_error("deferred init queue full (%d), dropping registration",
              DEFERRED_INIT_MAX);
    return;
  }
  deferred_inits[deferred_count++] = cb;
}

void deferred_init_run_all(void) {
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
  safe_copy(dst, dst_size, src);
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
  r.owner_gen = widget_current_generation();
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

int feature_loader_pending_unload_count(void) { return defer_unload_count; }

void feature_loader_drain_retired_unloads(void) {
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
    safe_copy(u->uniq, sizeof(u->uniq), uniq);
  if (owner && owner[0])
    safe_copy(u->owner, sizeof(u->owner), owner);
}

static void sweep_retired_libs(void) {
  const WindowManager *wm = WindowManager_Get();
  checkbox_reset_all();
  slider_reset_all();
  int n = 0;
  for (int i = 0; i < retired_count; i++) {
    bool alive = false;
    if (retired[i].owner[0] && widget_owner_active(retired[i].owner))
      alive = true;
    else if (retired[i].owner[0] &&
             thread_registry_owner_active(retired[i].owner))
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
      purge_owner_set(retired[i].owner, NULL, retired[i].owner_gen);
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

typedef struct {
  bool found_section;
  bool found_entry;
  size_t insert_at;
} ManifestSectionScan;

static void manifest_scan_section(const char *buf, size_t len,
                                  const char *section, const char *entry,
                                  ManifestSectionScan *out) {
  memset(out, 0, sizeof(*out));
  bool in_section = false;
  size_t off = 0;
  while (off < len) {
    const char *nl = memchr(buf + off, '\n', len - off);
    size_t line_len = nl ? (size_t)(nl - (buf + off)) : (len - off);
    size_t line_end = off + line_len + (nl ? 1 : 0);
    char line[512];
    size_t cl = line_len < sizeof(line) - 1 ? line_len : sizeof(line) - 1;
    memcpy(line, buf + off, cl);
    line[cl] = '\0';

    char work[512];
    safe_copy(work, sizeof(work), line);
    manifest_trim(work);

    if (work[0] == '[') {
      in_section = strstr(work, section) != NULL;
      if (in_section && !out->found_section) {
        out->found_section = true;
        out->insert_at = line_end;
      }
      off = line_end;
      continue;
    }

    if (in_section && work[0] != '\0' && work[0] != '#') {
      char e[512];
      safe_copy(e, sizeof(e), work);
      manifest_unquote(e);
      if (strcmp(e, entry) == 0)
        out->found_entry = true;
    }
    off = line_end;
  }
}

static bool manifest_read_or_create(const char *name, char *buf, size_t cap,
                                    size_t *out_len) {
  char path[512];
  if (!manifest_path(name, path, sizeof(path)))
    return false;
  FILE *f = fopen(path, "rb");
  if (!f) {
    manifest_write_baseline(name);
    f = fopen(path, "rb");
    if (!f) {
      log_error("manifest add: cannot open %s even after baseline create",
                path);
      return false;
    }
  }
  size_t len = 0;
  if (fseek(f, 0, SEEK_END) == 0) {
    long sz = ftell(f);
    if (sz >= 0 && (size_t)sz >= cap) {
      log_error("manifest '%s' is %ld bytes, too large to rewrite (%zu)", path,
                sz, cap - 1);
      fclose(f);
      return false;
    }
    rewind(f);
  }
  len = fread(buf, 1, cap - 1, f);
  fclose(f);
  buf[len] = '\0';
  *out_len = len;
  return true;
}

static void manifest_add_entry(const char *name, const char *section,
                               const char *entry) {
  char buf[16384];
  size_t len = 0;
  if (!manifest_read_or_create(name, buf, sizeof(buf), &len))
    return;

  ManifestSectionScan scan;
  manifest_scan_section(buf, len, section, entry, &scan);
  if (scan.found_entry)
    return;

  char new_line[128];
  int nl_len = snprintf(new_line, sizeof(new_line), "%s\n", entry);
  if (nl_len <= 0)
    return;
  size_t add = (size_t)nl_len;

  if (!scan.found_section) {
    char header[64];
    int hn = snprintf(header, sizeof(header), "\n[%s]\n", section);
    if (hn <= 0 || len + (size_t)hn + add >= sizeof(buf))
      return;
    memcpy(buf + len, header, (size_t)hn);
    len += (size_t)hn;
    memcpy(buf + len, new_line, add);
    len += add;
  } else {
    if (len + add > sizeof(buf))
      return;
    memmove(buf + scan.insert_at + add, buf + scan.insert_at,
            len - scan.insert_at);
    memcpy(buf + scan.insert_at, new_line, add);
    len += add;
  }

  char path[512];
  if (!manifest_path(name, path, sizeof(path)))
    return;
  FILE *f = fopen(path, "wb");
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
void feature_loader_manifest_add_provides(const char *owner_path,
                                          const char *key) {
  if (!owner_path || !owner_path[0] || !key || !key[0])
    return;
  char name[64];
  feature_loader_owner_feature_name(owner_path, name, sizeof(name));
  if (!name[0])
    return;
  manifest_add_entry(name, "provides", key);
}

void feature_loader_manifest_add_depends(const char *owner_path,
                                         const char *dep_name) {
  if (!owner_path || !owner_path[0] || !dep_name || !dep_name[0])
    return;
  char name[64];
  feature_loader_owner_feature_name(owner_path, name, sizeof(name));
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
  InitFn init = (InitFn)Platform_GetSymbol(h, "skald_feature_init");
  if (!init)
    init = (InitFn)Platform_GetSymbol(h, "SkaldFeatureReload");
  if (init) {
    feature_init_count++;
  } else {
    log_warn("feature %s has no skald_feature_init/reload symbol", name);
  }

  LoadedFeature *f = &features[feature_count++];
  memset(f, 0, sizeof(*f));
  safe_copy(f->name, sizeof(f->name), name);
  safe_copy(f->path, sizeof(f->path), full);
  f->handle = h;
  return true;
}

static bool load_named(const char *dir, const char *ext, const char *name) {
  char filename[128];
  snprintf(filename, sizeof(filename), "%s%s", name, ext);
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
    if (!manifest_parse_file(mpath, names[i], &mans[i]) || !mans[i].valid) {
      log_error("feature '%s': manifest %s is malformed, skipping", names[i],
                mpath);
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
  log_info("feature_loader_load_all_from_dir completed");
  return true;
}

static struct {
  char dir[256];
  time_t src_mtime;
} s_failed[64];
static int s_failed_count = 0;

static void feature_mark_failed(const char *dir, time_t mtime) {
  if (s_failed_count < 64) {
    safe_copy(s_failed[s_failed_count].dir,
              sizeof(s_failed[s_failed_count].dir), dir);
    s_failed[s_failed_count].src_mtime = mtime;
    s_failed_count++;
  }
}

#define FEATURE_BUILD_Q_MAX 32

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
static FeatureBuildJob build_queue[FEATURE_BUILD_Q_MAX];
static int build_queue_count;
static FeatureBuildDone build_done[FEATURE_BUILD_Q_MAX];
static int build_done_count;
static int builds_in_flight;
static bool reload_session_active;
static int reload_session_ok;
static int reload_session_fail;
static unsigned build_seq = 0;
static void *build_worker;
static bool build_worker_failed;

static void build_mutex_init(void) {
  if (!build_mutex)
    build_mutex = Platform_MutexCreate();
}
#define B_LOCK()                                                               \
  do {                                                                         \
    build_mutex_init();                                                        \
    Platform_MutexLock(build_mutex);                                           \
  } while (0)
#define B_UNLOCK() Platform_MutexUnlock(build_mutex)

static void note_feature_built(const char *ent_name, const char *dll,
                               long long src_mtime) {
  for (int fi = 0; fi < s_failed_count; fi++) {
    if (strncmp(s_failed[fi].dir, ent_name, sizeof(s_failed[fi].dir)) == 0)
      s_failed[fi].src_mtime = 0;
  }
  struct utimbuf times;
  times.actime = (time_t)src_mtime;
  times.modtime = (time_t)src_mtime;
  utime(dll, &times);
}

static int finish_hot_build(const char *ent_name, const char *dll,
                            const char *out, long long src_mtime, bool ok) {
  if (ok)
    ok = feature_loader_reload_from(ent_name, out);
  remove(out);
  if (!ok) {
    feature_mark_failed(ent_name, (time_t)src_mtime);
    return 0;
  }
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
      memmove(build_queue, build_queue + 1,
              sizeof(build_queue[0]) * (size_t)(build_queue_count - 1));
      build_queue_count--;
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
    if (build_done_count < FEATURE_BUILD_Q_MAX) {
      build_done[build_done_count].job = job;
      build_done[build_done_count].ok = ok;
      build_done_count++;
    } else {
      remove(job.out);
      log_warn("feature build result for %s dropped, queue full", job.ent_name);
    }
    B_UNLOCK();
  }
  return NULL;
}

static bool feature_build_enqueue(const FeatureBuildJob *job) {
  B_LOCK();
  if (build_queue_count >= FEATURE_BUILD_Q_MAX ||
      builds_in_flight >= FEATURE_BUILD_Q_MAX) {
    B_UNLOCK();
    return false;
  }
  build_queue[build_queue_count] = *job;
  build_queue_count++;
  builds_in_flight++;
  if (!build_worker && !build_worker_failed) {
    build_worker = Platform_ThreadCreate(feature_build_worker_main, NULL);
    if (!build_worker) {
      builds_in_flight--;
      build_queue_count--;
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
  FeatureBuildDone done[FEATURE_BUILD_Q_MAX];
  int n = 0;
  B_LOCK();
  n = build_done_count;
  if (n > 0) {
    memcpy(done, build_done, sizeof(build_done[0]) * (size_t)n);
    build_done_count = 0;
  }
  B_UNLOCK();
  int ok_n = 0, fail_n = 0;
  for (int i = 0; i < n; i++) {
    if (finish_hot_build(done[i].job.ent_name, done[i].job.dll, done[i].job.out,
                         done[i].job.src_mtime, done[i].ok))
      ok_n++;
    else
      fail_n++;
  }
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
    char src[1024], dll[1024];
    snprintf(src, sizeof(src), "%.255s/%.255s.c", subpath, ent_name);
    snprintf(dll, sizeof(dll), "build/features/%.255s%.8s", ent_name, ext);
    long long src_mtime = 0;
    if (!Platform_FileInfo(src, &src_mtime, NULL))
      continue;
    if (reload_lib_is_fresh(src, dll))
      continue;
    bool known_bad = false;
    for (int fi = 0; fi < s_failed_count; fi++) {
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

static void call_feature_shutdown(LibHandle handle) {
  typedef void (*ShutdownFn)(void);
  ShutdownFn shut =
      (ShutdownFn)Platform_GetSymbol(handle, "skald_feature_shutdown");
  if (shut)
    shut();
}

void feature_loader_shutdown(void) {
  LOCK();
  for (int i = 0; i < feature_count; i++) {
    if (features[i].handle) {
      call_feature_shutdown(features[i].handle);
      purge_owner_set(NULL, features[i].path, widget_bump_generation());
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
    if (strcmp(features[i].name, name) == 0) {
      char path_copy[512];
      fl_copy(path_copy, sizeof(path_copy), features[i].path);
      const char *src = (src_path && src_path[0]) ? src_path : path_copy;
      LibHandle old = features[i].handle;
      char *uniquepath =
          reload_lib_unique_copy(src, path_copy, ++reload_seq, NULL);
      if (!uniquepath) {
        log_error("reload %s: copy failed (keeping old)", name);
        UNLOCK();
        return false;
      }
      LibHandle h = Platform_LoadLibrary(uniquepath);
      if (!h) {
        log_error("reload %s failed: %s (keeping old)", name,
                  Platform_GetLoadError());
        remove(uniquepath);
        free(uniquepath);
        UNLOCK();
        return false;
      }
      typedef void (*InitFn)(void);
      InitFn init = (InitFn)Platform_GetSymbol(h, "skald_feature_init");
      if (!init)
        init = (InitFn)Platform_GetSymbol(h, "SkaldFeatureReload");
      if (!init) {
        log_error("reload %s: no init symbol, keeping old", name);
        Platform_UnloadLibrary(h);
        remove(uniquepath);
        free(uniquepath);
        UNLOCK();
        return false;
      }
      typedef void (*ShutdownFn)(void);
      ShutdownFn shut =
          (ShutdownFn)Platform_GetSymbol(old, "skald_feature_shutdown");
      if (shut)
        shut();
      checkbox_reset_all();
  slider_reset_all();
      unsigned retire_gen = widget_current_generation();
      char retire_owner[512];
      fl_copy(retire_owner, sizeof(retire_owner),
              features[i].uniq[0] ? features[i].uniq : features[i].path);
      retire_old_lib(old, features[i].uniq, retire_owner);
      widget_bump_generation();
      log_info("feature_reload: %s (constructor ran at dlopen)", name);
      features[i].handle = h;
      fl_copy(features[i].path, sizeof(features[i].path), uniquepath);
      fl_copy(features[i].uniq, sizeof(features[i].uniq), uniquepath);
      free(uniquepath);
      purge_owner_set(retire_owner, path_copy, retire_gen);
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
    if (!h) {
      log_error("reload %s failed: %s (new feature, not loaded)", name,
                Platform_GetLoadError());
      remove(uniquepath);
      free(uniquepath);
      UNLOCK();
      return false;
    }
    typedef void (*InitFn)(void);
    InitFn init = (InitFn)Platform_GetSymbol(h, "skald_feature_init");
    if (!init)
      init = (InitFn)Platform_GetSymbol(h, "SkaldFeatureReload");
    if (!init) {
      log_error("reload %s: no init symbol, new feature not loaded", name);
      Platform_UnloadLibrary(h);
      remove(uniquepath);
      free(uniquepath);
      UNLOCK();
      return false;
    }
    LoadedFeature *f = &features[feature_count++];
    memset(f, 0, sizeof(*f));
    safe_copy(f->name, sizeof(f->name), name);
    safe_copy(f->path, sizeof(f->path), uniquepath);
    safe_copy(f->uniq, sizeof(f->uniq), uniquepath);
    f->handle = h;
    feature_init_count++;
    {
      const char *ext = Platform_LibExtension();
      char dll[512];
      snprintf(dll, sizeof(dll), "build/features/%.255s%.8s", name,
               ext ? ext : "");
      if (!file_copy_path(uniquepath, dll))
        log_warn("reload %s: could not persist %s (will rebuild next start)",
                 name, dll);
    }
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
