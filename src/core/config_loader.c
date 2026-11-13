#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
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
#if defined(_WIN32)
#include <windows.h>
#endif
#include "core/window_manager.h"
#include "helpers/helpers_fs.h"
#include "helpers/helpers_text.h"

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
  DIR *d = opendir("build");
  if (!d)
    return;
  const char *lib_ext = Platform_LibExtension();
  struct dirent *e;
  int nremoved = 0;
  while ((e = readdir(d)) != NULL) {
    const char *name = e->d_name;
    if (strncmp(name, "config.hot", 10) != 0)
      continue;
    const char *p = name + 10;
    while (*p >= '0' && *p <= '9')
      p++;
    if (*p == '\0')
      continue;
    if (p == name + 10)
      continue;
    if (!lib_ext || strcmp(p, lib_ext) != 0)
      continue;
    char junk[512];
    snprintf(junk, sizeof(junk), "build/%s", name);
    if (remove(junk) == 0)
      nremoved++;
  }
  closedir(d);
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
  char *n_font = heap_dup(UsedFont), *n_term = heap_dup(UsedTerminal),
       *n_shell = heap_dup(UsedShell);
  char *n_search = heap_dup(UsedSearchEngine), *n_feat = heap_dup(FeaturePath),
       *n_browser = heap_dup(UsedBrowser), *n_icon = heap_dup(IconPath);
  char *n_hotk = heap_dup(ControlHotkey),
       *n_reload_hotk = heap_dup(ReloadFeaturesHotkey),
       *n_actions_hotk = heap_dup(ActionsHotkey),
       *n_rehotk = heap_dup(RegHotkey),
       *n_realias_hotk = heap_dup(RegAliasHotkey), *n_theme = heap_dup(Theme),
       *n_bgimg = heap_dup(BackgroundImage);
  char *old_font = heap_used_font, *old_term = heap_used_terminal,
       *old_shell = heap_used_shell, *old_browser = heap_used_browser,
       *old_search = heap_used_search, *old_feat = heap_feature_path,
       *old_icon = heap_icon_path, *old_hotk = heap_control_hotkey,
       *old_reload_hotk = heap_reload_features_hotkey,
       *old_actions_hotk = heap_actions_hotkey, *old_rehotk = heap_rehotk,
       *old_realias_hotk = heap_realias_hotk, *old_theme = heap_theme,
       *old_bgimg = heap_background_image;
  if (n_font) {
    UsedFont = n_font;
    heap_used_font = n_font;
  } else
    heap_used_font = old_font, old_font = NULL;
  if (n_term) {
    UsedTerminal = n_term;
    heap_used_terminal = n_term;
  } else
    heap_used_terminal = old_term, old_term = NULL;
  if (n_shell) {
    UsedShell = n_shell;
    heap_used_shell = n_shell;
  } else
    heap_used_shell = old_shell, old_shell = NULL;
  if (n_browser) {
    UsedBrowser = n_browser;
    heap_used_browser = n_browser;
  } else
    heap_used_browser = old_browser, old_browser = NULL;
  if (n_search) {
    UsedSearchEngine = n_search;
    heap_used_search = n_search;
  } else
    heap_used_search = old_search, old_search = NULL;
  if (n_feat) {
    FeaturePath = n_feat;
    heap_feature_path = n_feat;
  } else
    heap_feature_path = old_feat, old_feat = NULL;
  if (n_icon) {
    IconPath = n_icon;
    heap_icon_path = n_icon;
  } else
    heap_icon_path = old_icon, old_icon = NULL;
  if (n_hotk) {
    ControlHotkey = n_hotk;
    heap_control_hotkey = n_hotk;
  } else
    heap_control_hotkey = old_hotk, old_hotk = NULL;
  if (n_reload_hotk) {
    ReloadFeaturesHotkey = n_reload_hotk;
    heap_reload_features_hotkey = n_reload_hotk;
  } else
    heap_reload_features_hotkey = old_reload_hotk, old_reload_hotk = NULL;
  if (n_actions_hotk) {
    ActionsHotkey = n_actions_hotk;
    heap_actions_hotkey = n_actions_hotk;
  } else
    heap_actions_hotkey = old_actions_hotk, old_actions_hotk = NULL;
  if (n_rehotk) {
    RegHotkey = n_rehotk;
    heap_rehotk = n_rehotk;
  } else
    heap_rehotk = old_rehotk, old_rehotk = NULL;
  if (n_realias_hotk) {
    RegAliasHotkey = n_realias_hotk;
    heap_realias_hotk = n_realias_hotk;
  } else
    heap_realias_hotk = old_realias_hotk, old_realias_hotk = NULL;
  if (n_theme) {
    Theme = n_theme;
    heap_theme = n_theme;
  } else
    heap_theme = old_theme, old_theme = NULL;
  if (n_bgimg) {
    BackgroundImage = n_bgimg;
    heap_background_image = n_bgimg;
  } else
    heap_background_image = old_bgimg, old_bgimg = NULL;
  free(old_font);
  free(old_term);
  free(old_shell);
  free(old_browser);
  free(old_search);
  free(old_feat);
  free(old_icon);
  free(old_hotk);
  free(old_reload_hotk);
  free(old_actions_hotk);
  free(old_rehotk);
  free(old_realias_hotk);
  free(old_theme);
  free(old_bgimg);
}
static bool confiload_copy(const char *uniq, LibHandle *out) {
  LibHandle h = Platform_LoadLibrary(uniq);
  if (!h) {
    log_error("config dlopen %s failed", uniq);
    return false;
  }
  typedef void (*ApplyFn)(void);
  ApplyFn apply = (ApplyFn)Platform_GetSymbol(h, "ConfigApply");
  if (!apply) {
    log_error("config %s has no ConfigApply, keeping current", uniq);
    Platform_UnloadLibrary(h);
    return false;
  }
  apply();
  Theme_Apply();
  heapify_config();
  *out = h;
  return true;
}

static bool confioutput_is_fresh(const char *out) {
  struct stat src_st, out_st;
  if (stat(CONFISRC, &src_st) != 0)
    return false;
  if (stat(out, &out_st) != 0)
    return false;
  return out_st.st_mtime >= src_st.st_mtime;
}

static bool confibuild_feature(const char *src, const char *out) {
  char tmp[1024];
  snprintf(tmp, sizeof(tmp), "%s.tmp", out);
  const char *argv[PLATFORM_BUILD_ARGV_MAX];
  if (!Platform_GetFeatureBuildArgv(src, tmp, argv, PLATFORM_BUILD_ARGV_MAX)) {
    log_error("config build: no feature build argv for %s", src);
    return false;
  }
  int status = -1;
  char *outStr = Platform_RunArgvCapture(argv, NULL, &status);
  bool ok = outStr != NULL && status == 0;
  if (outStr) {
    if (outStr[0])
      log_info("%s", outStr);
    free(outStr);
  } else
    log_error("config build: could not run compiler for %s", src);
  if (!ok) {
    unlink(tmp);
    return false;
  }
  if (!Platform_AtomicRename(tmp, out)) {
    unlink(tmp);
    return false;
  }
  return true;
}

static int config_build_load_store(void) {

  const char *out = confiout_path();
  int rebuilt = 0;
  if (confioutput_is_fresh(out)) {
    log_info("config .so is newer than source, skipping compile");
  } else {
    bool ok = confibuild_feature(CONFISRC, out);
    if (!ok) {
      log_error("config compile failed, keeping current");
      return -1;
    }
    rebuilt = 1;
  }
  char uniq[256];
  const char *lib_ext = Platform_LibExtension();
  if (!lib_ext) {
    log_error("Platform_LibExtension() returned NULL!");
    return -1;
  }
  snprintf(uniq, sizeof(uniq), "build/config.hot%d%s", ++cfseq, lib_ext);
  {

    bool ok = File_CopyPath(out, uniq);

    if (!ok) {
      log_error("config copy failed, keeping current");
      return -1;
    }
  }
  LibHandle h = 0;
  {

    bool ok = confiload_copy(uniq, &h);

    if (!ok) {
      remove(uniq);
      return -1;
    }
  }
  if (cfcount >= 2) {
    Platform_UnloadLibrary(cflibs[0].handle);
    remove(cflibs[0].uniq);
    cflibs[0] = cflibs[1];
    cfcount = 1;
  }
  cflibs[cfcount].handle = h;
  SafeCopy(cflibs[cfcount].uniq, sizeof(cflibs[cfcount].uniq), uniq);
  cfcount++;
  return rebuilt;
}

bool Config_Load(void) {
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

bool Config_Reload(void) {
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

void Config_Unload(void) {
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

static void cfput(char *buf, size_t cap, size_t *off, const char *fmt, ...) {
  if (*off >= cap)
    return;
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf + *off, cap - *off, fmt, ap);
  va_end(ap);
  if (n > 0)
    *off += (size_t)n;
}

static void cfput_escaped(char *buf, size_t cap, size_t *off, const char *s) {
  cfput(buf, cap, off, "\"");
  for (const char *p = s ? s : ""; *p; p++) {
    if (*p == '\\' || *p == '"')
      cfput(buf, cap, off, "\\%c", *p);
    else if (*p == '\n')
      cfput(buf, cap, off, "\\n");
    else if (*p == '\r')
      cfput(buf, cap, off, "\\r");
    else
      cfput(buf, cap, off, "%c", *p);
  }
  cfput(buf, cap, off, "\"");
}

static void cfput_float(char *buf, size_t cap, size_t *off, const char *name,
                        float v) {
  char tmp[64];
  snprintf(tmp, sizeof(tmp), "%.6g", (double)v);
  if (!strchr(tmp, '.') && !strchr(tmp, 'e') && !strchr(tmp, 'E') &&
      !strchr(tmp, 'n') && !strchr(tmp, 'N')) {
    cfput(buf, cap, off, "    %s = %s.0f;\n", name, tmp);
  } else {
    cfput(buf, cap, off, "    %s = %sf;\n", name, tmp);
  }
}

static void cfput_str_var(char *buf, size_t cap, size_t *off, const char *name,
                          const char *value) {
  if (Config_IsAutoResolvedApp(name))
    value = "";
  cfput(buf, cap, off, "    %s = ", name);
  cfput_escaped(buf, cap, off, value);
  cfput(buf, cap, off, ";\n");
}

static size_t confiemit_region(char *buf, size_t cap) {
  size_t off = 0;
  cfput(buf, cap, &off, "void ConfigApply(void) {\n");
  cfput(buf, cap, &off, "    Tray = %s;\n", Tray ? "true" : "false");
  cfput(buf, cap, &off, "    EnableAutoPairing = %s;\n",
        EnableAutoPairing ? "true" : "false");
  cfput(buf, cap, &off, "    RememberLastIndex = %s;\n",
        RememberLastIndex ? "true" : "false");
  cfput(buf, cap, &off, "    MessageAfterEachHotkey = %s;\n",
        MessageAfterEachHotkey ? "true" : "false");
  cfput(buf, cap, &off, "    TimeOutForMessages = %d;\n", TimeOutForMessages);
  cfput(buf, cap, &off, "    DebugMode = %s;\n", DebugMode ? "true" : "false");
  cfput(buf, cap, &off, "    RoundedCorners = %s;\n",
        RoundedCorners ? "true" : "false");
  cfput(buf, cap, &off, "    EnableBorders = %s;\n",
        EnableBorders ? "true" : "false");
  cfput(buf, cap, &off, "    CornerRadius = %d;\n", CornerRadius);
  cfput(buf, cap, &off, "    HotkeyRepeatDelayMs = %d;\n", HotkeyRepeatDelayMs);
  cfput_float(buf, cap, &off, "WindowOpacity", WindowOpacity);
  cfput(buf, cap, &off, "    UsedFont = ");
  cfput_escaped(buf, cap, &off, UsedFont);
  cfput(buf, cap, &off, ";\n");
  cfput(buf, cap, &off, "    FontSize = %d;\n", Platform_GetUnscaledFontSize());
  cfput(buf, cap, &off, "    IconSize = %d;\n", IconSize);
  cfput(buf, cap, &off, "    WidgetGap = %d;\n", WidgetGap);
  cfput(buf, cap, &off, "    PromptWidth = %d;\n", PromptWidth);
  cfput(buf, cap, &off, "    PromptHeight = %d;\n", PromptHeight);
  cfput_str_var(buf, cap, &off, "UsedTerminal", UsedTerminal);
  cfput_str_var(buf, cap, &off, "UsedShell", UsedShell);
  cfput_str_var(buf, cap, &off, "UsedBrowser", UsedBrowser);
  cfput(buf, cap, &off, "    UsedSearchEngine = ");
  cfput_escaped(buf, cap, &off, UsedSearchEngine);
  cfput(buf, cap, &off, ";\n");
  cfput(buf, cap, &off, "    FeaturePath = ");
  cfput_escaped(buf, cap, &off, FeaturePath);
  cfput(buf, cap, &off, ";\n");
  cfput(buf, cap, &off, "    IconPath = ");
  cfput_escaped(buf, cap, &off, IconPath);
  cfput(buf, cap, &off, ";\n");
  cfput(buf, cap, &off, "    ControlHotkey = ");
  cfput_escaped(buf, cap, &off, ControlHotkey);
  cfput(buf, cap, &off, ";\n");
  cfput(buf, cap, &off, "    ReloadFeaturesHotkey = ");
  cfput_escaped(buf, cap, &off, ReloadFeaturesHotkey);
  cfput(buf, cap, &off, ";\n");
  cfput(buf, cap, &off, "    ActionsHotkey = ");
  cfput_escaped(buf, cap, &off, ActionsHotkey);
  cfput(buf, cap, &off, ";\n");
  cfput(buf, cap, &off, "    RegHotkey = ");
  cfput_escaped(buf, cap, &off, RegHotkey);
  cfput(buf, cap, &off, ";\n");
  cfput(buf, cap, &off, "    RegAliasHotkey = ");
  cfput_escaped(buf, cap, &off, RegAliasHotkey);
  cfput(buf, cap, &off, ";\n");
  cfput(buf, cap, &off, "    MessagesEnabled = %s;\n",
        MessagesEnabled ? "true" : "false");
  cfput(buf, cap, &off, "    RainbowText = %s;\n",
        RainbowText ? "true" : "false");
  cfput(buf, cap, &off, "    WindowTitleText = %s;\n",
        WindowTitleText ? "true" : "false");
  cfput(buf, cap, &off, "    CloseOnLossOfFocus = %s;\n",
        CloseOnLossOfFocus ? "true" : "false");
  cfput(buf, cap, &off, "    Theme = ");
  cfput_escaped(buf, cap, &off, Theme);
  cfput(buf, cap, &off, ";\n");
  cfput(buf, cap, &off, "    BackgroundImage = ");
  cfput_escaped(buf, cap, &off, BackgroundImage ? BackgroundImage : "");
  cfput(buf, cap, &off, ";\n");
  ColorRef cols[] = {COLOR_BG, COLOR_TEXT, COLOR_SELECTED_TEXT, COLOR_CURSOR,
                     COLOR_BORDER};
  const char *names[] = {"COLOR_BG", "COLOR_TEXT", "COLOR_SELECTED_TEXT",
                         "COLOR_CURSOR", "COLOR_BORDER"};
  for (size_t i = 0; i < sizeof(cols) / sizeof(cols[0]); i++) {
    cfput(buf, cap, &off, "    %s = RGB(%u, %u, %u);\n", names[i],
          cols[i] & 0xFFu, (cols[i] >> 8) & 0xFFu, (cols[i] >> 16) & 0xFFu);
  }
  cfput(buf, cap, &off, "}\n");
  return off;
}

static long long cfsaved_mtime = 0;
static long long cfsaved_size = 0;
static int cfsaved_valid = 0;

void Config_Save(void) {
  static char region[65536];
  size_t n = confiemit_region(region, sizeof(region));
  if (n == 0 || n >= sizeof(region)) {
    log_error("config emit failed");
    return;
  }
  FILE *f = fopen(CONFISRC, "rb");
  if (!f) {
    log_error("config open %s failed", CONFISRC);
    return;
  }
  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (fsize < 0 || fsize > 1048576) {
    fclose(f);
    log_error("config size bad");
    return;
  }
  char *orig = malloc((size_t)fsize + 1);
  if (!orig) {
    log_error("config save: malloc %lld failed", (long long)fsize + 1);
    fclose(f);
    return;
  }
  size_t got = fread(orig, 1, (size_t)fsize, f);
  fclose(f);
  if (got != (size_t)fsize) {
    free(orig);
    log_error("config read short (%zu/%lld), not touching file", got,
              (long long)fsize);
    return;
  }
  orig[got] = '\0';
  char *start = strstr(orig, CONFISTART_MARK);
  char *end = start ? strstr(start, CONFIEND_MARK) : NULL;
  if (!start || !end || end < start) {
    free(orig);
    log_error("config markers missing, not touching file");
    return;
  }
  char *start_eol = strchr(start, '\n');
  if (!start_eol) {
    log_error("config marker end without newline, not rewriting");
    free(orig);
    return;
  }
  start_eol++;
  size_t head = (size_t)(start_eol - orig);
  size_t tail = got - (size_t)(end - orig);
  size_t body_len = (size_t)(end - start_eol);
  if (body_len == n && memcmp(orig + head, region, n) == 0) {
    free(orig);
    log_info("config unchanged, not rewriting");
    return;
  }
  char tmp[512];
  snprintf(tmp, sizeof(tmp), "%s.tmp.%d", CONFISRC, (int)getpid());
  FILE *out = fopen(tmp, "wb");
  if (!out) {
    free(orig);
    log_error("config save: fopen %s failed (%s)", tmp, strerror(errno));
    return;
  }
  bool ok = fwrite(orig, 1, head, out) == head;
  if (ok)
    ok = fwrite(region, 1, n, out) == n;
  if (ok)
    ok = fwrite(end, 1, tail, out) == tail;
  if (fclose(out) != 0)
    ok = false;
  free(orig);
  if (!ok) {
    remove(tmp);
    log_error("config write failed");
    return;
  }
  if (!Platform_AtomicRename(tmp, CONFISRC)) {
    remove(tmp);
    log_error("config rename failed");
    return;
  }
  struct stat st;
  if (stat(CONFISRC, &st) == 0) {
    cfsaved_mtime = (long long)st.st_mtime;
    cfsaved_size = (long long)st.st_size;
    cfsaved_valid = 1;
  }
  log_info("config saved");
}
