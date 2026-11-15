#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

#include "core/types/window.h"
#include "platform/platform.h"
#include "platform/lib.h"
#include "config.h"
#include "core/theme.h"
#include "deps/log.h"
#include "config_values.h"
#include "core/types/color.h"
#include "core/window_manager.h"
#include "helpers/helpers_text.h"
#include "config_defs.h"
#include "core/reload_lib.h"

#define CONFISRC "config/config.c"
#define CONFISTART_MARK "// <skald-config-generated>"
#define CONFIEND_MARK "// </skald-config-generated>"

static const char *confiout_path(void) {
  static char path[256];
  snprintf(path, sizeof(path), "build/config%s", Platform_LibExtension());
  return path;
}

typedef struct {
  LibHandle handle;
  char uniq[256];
} ConfigLib;

static ConfigLib cflibs[2];
static int cfcount = 0;
static int cfseq = 0;
static bool cfculled = false;

static void cfcull_stale_libs(void) {
  if (cfculled)
    return;
  cfculled = true;
  const char *lib_ext = Platform_LibExtension();
  int nremoved = reload_lib_sweep_stale_hot_libs("build", lib_ext);
  if (nremoved > 0)
    log_info("config: removed %d stale hot-reload libs", nremoved);
}

static char *heap_dup(const char *s) {
  if (!s)
    return NULL;
  size_t n = strlen(s) + 1;
  char *d = (char *)malloc(n);
  if (d)
    memcpy(d, s, n);
  return d;
}
static char *heap_used_font = NULL, *heap_used_terminal = NULL,
            *heap_used_shell = NULL;
static char *heap_used_browser = NULL;
static char *heap_used_search = NULL, *heap_feature_path = NULL,
            *heap_icon_path = NULL;
static char *heap_control_hotkey = NULL, *heap_reload_features_hotkey = NULL,
            *heap_actions_hotkey = NULL, *heap_rehotk = NULL,
            *heap_realias_hotk = NULL, *heap_theme = NULL,
            *heap_background_image = NULL;

static void heapify_config(void) {
  static struct {
    const char **global;
    char **heap;
  } slots[] = {
      {&UsedFont, &heap_used_font},
      {&UsedTerminal, &heap_used_terminal},
      {&UsedShell, &heap_used_shell},
      {&UsedBrowser, &heap_used_browser},
      {&UsedSearchEngine, &heap_used_search},
      {&FeaturePath, &heap_feature_path},
      {&IconPath, &heap_icon_path},
      {&ControlHotkey, &heap_control_hotkey},
      {&ReloadFeaturesHotkey, &heap_reload_features_hotkey},
      {&ActionsHotkey, &heap_actions_hotkey},
      {&RegHotkey, &heap_rehotk},
      {&RegAliasHotkey, &heap_realias_hotk},
      {&Theme, &heap_theme},
      {&BackgroundImage, &heap_background_image},
  };
  for (size_t i = 0; i < sizeof(slots) / sizeof(slots[0]); i++) {
    char *fresh = heap_dup(*slots[i].global);
    if (!fresh)
      continue;
    char *old = *slots[i].heap;
    *slots[i].global = fresh;
    *slots[i].heap = fresh;
    free(old);
  }
}
static bool confiload_copy(const char *uniq, LibHandle *out) {
  LibHandle h = Platform_LoadLibrary(uniq);
  if (!h) {
    log_error("config dlopen %s failed", uniq);
    return false;
  }
  typedef void (*ApplyFn)(void);
  ApplyFn apply = (ApplyFn)Platform_GetSymbol(h, "config_apply");
  if (!apply) {
    log_error("config %s has no config_apply, keeping current", uniq);
    Platform_UnloadLibrary(h);
    return false;
  }
  apply();
  theme_apply();
  heapify_config();
  *out = h;
  return true;
}

static int config_build_load_store(void) {

  const char *out = confiout_path();
  int rebuilt = 0;
  if (reload_lib_is_fresh(CONFISRC, out)) {
    log_info("config .so is newer than source, skipping compile");
  } else {
    bool ok = reload_lib_compile(CONFISRC, out, "config build");
    if (!ok) {
      log_error("config compile failed, keeping current");
      return -1;
    }
    rebuilt = 1;
  }
  const char *lib_ext = Platform_LibExtension();
  if (!lib_ext) {
    log_error("Platform_LibExtension() returned NULL!");
    return -1;
  }
  char *uniq = reload_lib_unique_copy(out, "build/config", ++cfseq, lib_ext);
  if (!uniq) {
    log_error("config copy failed, keeping current");
    return -1;
  }
  LibHandle h = 0;
  bool ok = confiload_copy(uniq, &h);
  if (!ok) {
    remove(uniq);
    free(uniq);
    return -1;
  }
  if (cfcount >= 2) {
    Platform_UnloadLibrary(cflibs[0].handle);
    remove(cflibs[0].uniq);
    cflibs[0] = cflibs[1];
    cfcount = 1;
  }
  cflibs[cfcount].handle = h;
  safe_copy(cflibs[cfcount].uniq, sizeof(cflibs[cfcount].uniq), uniq);
  cfcount++;
  free(uniq);
  return rebuilt;
}

bool config_load(void) {
  cfcull_stale_libs();
  int r = config_build_load_store();

  if (r < 0) {
    log_error("config load failed, running on built-in defaults");
    return false;
  }
  return true;
}

static long long cflast_reload_ms = 0;
static void *cfreload_mutex = NULL;

static void cfreload_lock(void) {
  if (!cfreload_mutex)
    cfreload_mutex = Platform_MutexCreate();
  if (cfreload_mutex)
    Platform_MutexLock(cfreload_mutex);
}

static void cfreload_unlock(void) {
  if (cfreload_mutex)
    Platform_MutexUnlock(cfreload_mutex);
}

bool config_reload(void) {
  WindowManager *_wm2 = WindowManager_Get();
  if (_wm2 && _wm2->popup_count > 0) {
    log_warn("cfg reload blocked: %d popup(s) open", _wm2->popup_count);
    return false;
  }
  cfreload_lock();
  long long now = Platform_GetWallClockMs();
  long long debounce = (long long)1000;
  if (cflast_reload_ms && now - cflast_reload_ms < debounce) {
    log_warn("cfg reload blocked: debounce, %lldms since last",
             (long long)(now - cflast_reload_ms));
    cfreload_unlock();
    return false;
  }
  int r = config_build_load_store();
  if (r < 0) {
    log_warn("cfg reload blocked: build/load failed, keeping old values");
    cfreload_unlock();
    return false;
  }
  if (r == 0) {
    log_info("config unchanged, reload no-op");
    cfreload_unlock();
    return false;
  }
  cflast_reload_ms = Platform_GetWallClockMs();
  Platform_ResetUIScale();
  cfreload_unlock();
  return true;
}

void config_unload(void) {
  for (int i = 0; i < cfcount; i++) {
    if (cflibs[i].handle)
      Platform_UnloadLibrary(cflibs[i].handle);
    if (cflibs[i].uniq[0])
      remove(cflibs[i].uniq);
    cflibs[i].handle = 0;
    cflibs[i].uniq[0] = '\0';
  }
  cfcount = 0;
}

typedef struct {
  char *data;
  size_t len;
  size_t cap;
} ConfigBuf;

static void cfbuf_free(ConfigBuf *b) {
  free(b->data);
  b->data = NULL;
  b->len = 0;
  b->cap = 0;
}

static void cfbuf_put(ConfigBuf *b, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  va_list aq;
  va_copy(aq, ap);
  int n = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  if (n < 0) {
    va_end(aq);
    return;
  }
  size_t need = b->len + (size_t)n + 1;
  if (need > b->cap) {
    size_t cap = b->cap ? b->cap : 512;
    while (cap < need)
      cap *= 2;
    char *grown = (char *)realloc(b->data, cap);
    if (!grown) {
      va_end(aq);
      return;
    }
    b->data = grown;
    b->cap = cap;
  }
  vsnprintf(b->data + b->len, b->cap - b->len, fmt, aq);
  va_end(aq);
  b->len += (size_t)n;
}

static void cfput_bool_line(ConfigBuf *b, const char *name, bool v) {
  cfbuf_put(b, "  %s = %s;\n", name, v ? "true" : "false");
}

static void cfput_int_line(ConfigBuf *b, const char *name, int v) {
  if (strcmp(name, "FontSize") == 0) {
    cfbuf_put(b, "  FontSize = Platform_GetUnscaledFontSize();\n");
    return;
  }
  cfbuf_put(b, "  %s = %d;\n", name, v);
}

static void cfput_float_line(ConfigBuf *b, const char *name, float v) {
  char tmp[64];
  snprintf(tmp, sizeof(tmp), "%.6g", (double)v);
  if (!strchr(tmp, '.') && !strchr(tmp, 'e') && !strchr(tmp, 'E') &&
      !strchr(tmp, 'n') && !strchr(tmp, 'N'))
    cfbuf_put(b, "  %s = %s.0f;\n", name, tmp);
  else
    cfbuf_put(b, "  %s = %sf;\n", name, tmp);
}

static void cfput_escaped(ConfigBuf *b, const char *s) {
  cfbuf_put(b, "\"");
  for (const char *p = s ? s : ""; *p; p++) {
    if (*p == '\\' || *p == '"')
      cfbuf_put(b, "\\%c", *p);
    else if (*p == '\n')
      cfbuf_put(b, "\\n");
    else if (*p == '\r')
      cfbuf_put(b, "\\r");
    else
      cfbuf_put(b, "%c", *p);
  }
  cfbuf_put(b, "\"");
}

static void cfput_str_line(ConfigBuf *b, const char *name, const char *value) {
  if (config_is_auto_resolved_app(name))
    value = "";
  cfbuf_put(b, "  %s = ", name);
  cfput_escaped(b, value);
  cfbuf_put(b, ";\n");
}

static void cfput_color_line(ConfigBuf *b, const char *name, ColorRef v) {
  cfbuf_put(b, "  %s = RGB(%u, %u, %u);\n", name, v & 0xFFu, (v >> 8) & 0xFFu,
            (v >> 16) & 0xFFu);
}

static ConfigBuf confiemit_region(void) {
  ConfigBuf b = {0};
  cfbuf_put(&b, "void config_apply(void) {\n");
#define EMIT_LIVE(type, name, def)                                             \
  _Generic((type){0},                                                          \
      bool: cfput_bool_line,                                                   \
      int: cfput_int_line,                                                     \
      float: cfput_float_line,                                                 \
      const char *: cfput_str_line,                                            \
      ColorRef: cfput_color_line)(&b, #name, name);
  SKALD_CONFIG_VALUES(EMIT_LIVE)
#undef EMIT_LIVE
  cfbuf_put(&b, "}\n");
  return b;
}

static long long cfsaved_mtime = 0;
static long long cfsaved_size = 0;
static int cfsaved_valid = 0;

void config_save(void) {
  ConfigBuf region = confiemit_region();
  if (!region.data) {
    log_error("config emit failed");
    return;
  }
  size_t n = region.len;
  const char *region_data = region.data;
  FILE *f = fopen(CONFISRC, "rb");
  if (!f) {
    log_error("config open %s failed", CONFISRC);
    goto out;
  }
  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (fsize < 0 || fsize > 1048576) {
    fclose(f);
    log_error("config size bad");
    goto out;
  }
  char *orig = malloc((size_t)fsize + 1);
  if (!orig) {
    log_error("config save: malloc %lld failed", (long long)fsize + 1);
    fclose(f);
    goto out;
  }
  size_t got = fread(orig, 1, (size_t)fsize, f);
  fclose(f);
  if (got != (size_t)fsize) {
    free(orig);
    log_error("config read short (%zu/%lld), not touching file", got,
              (long long)fsize);
    goto out;
  }
  orig[got] = '\0';
  char *start = strstr(orig, CONFISTART_MARK);
  char *end = start ? strstr(start, CONFIEND_MARK) : NULL;
  if (!start || !end || end < start) {
    free(orig);
    log_error("config markers missing, not touching file");
    goto out;
  }
  char *start_eol = strchr(start, '\n');
  if (!start_eol) {
    log_error("config marker end without newline, not rewriting");
    free(orig);
    goto out;
  }
  start_eol++;
  size_t head = (size_t)(start_eol - orig);
  size_t tail = got - (size_t)(end - orig);
  size_t body_len = (size_t)(end - start_eol);
  if (body_len == n && memcmp(orig + head, region_data, n) == 0) {
    free(orig);
    log_info("config unchanged, not rewriting");
    goto out;
  }
  char tmp[512];
  snprintf(tmp, sizeof(tmp), "%s.tmp.%d", CONFISRC, (int)getpid());
  FILE *out = fopen(tmp, "wb");
  if (!out) {
    free(orig);
    log_error("config save: fopen %s failed (%s)", tmp, strerror(errno));
    goto out;
  }
  bool ok = fwrite(orig, 1, head, out) == head;
  if (ok)
    ok = fwrite(region_data, 1, n, out) == n;
  if (ok)
    ok = fwrite(end, 1, tail, out) == tail;
  if (fclose(out) != 0)
    ok = false;
  free(orig);
  if (!ok) {
    remove(tmp);
    log_error("config write failed");
    goto out;
  }
  if (!Platform_AtomicRename(tmp, CONFISRC)) {
    remove(tmp);
    log_error("config rename failed");
    goto out;
  }
  struct stat st;
  if (stat(CONFISRC, &st) == 0) {
    cfsaved_mtime = (long long)st.st_mtime;
    cfsaved_size = (long long)st.st_size;
    cfsaved_valid = 1;
  }
  log_info("config saved");
out:
  cfbuf_free(&region);
}
