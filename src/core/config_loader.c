#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "platform/platform.h"

static void config_cache_info(const char *path);
#include "config.h"
#include "core/theme.h"
#include "deps/log.h"
#include "config_values.h"
#include "core/types/color.h"
#include "core/config_toml.h"
#include "helpers/helpers_ui.h"

#define CONFIG_SRC "config/config.toml"
#define CONFIG_START_MARK "# <skald-config-generated>"
#define CONFIG_END_MARK "# </skald-config-generated>"

typedef enum {
  CONFIG_BOOL,
  CONFIG_INT,
  CONFIG_FLOAT,
  CONFIG_STRING,
  CONFIG_COLORREF
} ConfigKind;

typedef struct {
  const char *name;
  ConfigKind kind;
  void *slot;
} ConfigEntry;

static const ConfigEntry config_entries[] = {
    {"Tray", CONFIG_BOOL, &Tray},
    {"RememberLastIndex", CONFIG_BOOL, &RememberLastIndex},
    {"MessageAfterEachHotkey", CONFIG_BOOL, &MessageAfterEachHotkey},
    {"TimeOutForMessages", CONFIG_INT, &TimeOutForMessages},
    {"HotkeyRepeatDelayMs", CONFIG_INT, &HotkeyRepeatDelayMs},
    {"DebugMode", CONFIG_BOOL, &DebugMode},
    {"RoundedCorners", CONFIG_BOOL, &RoundedCorners},
    {"CornerRadius", CONFIG_INT, &CornerRadius},
    {"MessageScreenGap", CONFIG_INT, &MessageScreenGap},
    {"WindowOpacity", CONFIG_FLOAT, &WindowOpacity},
    {"UsedFont", CONFIG_STRING, &UsedFont},
    {"FontSize", CONFIG_INT, &FontSize},
    {"IconSize", CONFIG_INT, &IconSize},
    {"WidgetGap", CONFIG_INT, &WidgetGap},
    {"PromptWidth", CONFIG_INT, &PromptWidth},
    {"PromptHeight", CONFIG_INT, &PromptHeight},
    {"PromptX", CONFIG_INT, &PromptX},
    {"PromptY", CONFIG_INT, &PromptY},
    {"UsedTerminal", CONFIG_STRING, &UsedTerminal},
    {"UsedShell", CONFIG_STRING, &UsedShell},
    {"UsedBrowser", CONFIG_STRING, &UsedBrowser},
    {"UsedSearchEngine", CONFIG_STRING, &UsedSearchEngine},
    {"FeaturePath", CONFIG_STRING, &FeaturePath},
    {"IconPath", CONFIG_STRING, &IconPath},
    {"ControlHotkey", CONFIG_STRING, &ControlHotkey},
    {"SettingsHotkey", CONFIG_STRING, &SettingsHotkey},
    {"ReloadFeaturesHotkey", CONFIG_STRING, &ReloadFeaturesHotkey},
    {"ActionsHotkey", CONFIG_STRING, &ActionsHotkey},
    {"RegHotkey", CONFIG_STRING, &RegHotkey},
    {"RegAliasHotkey", CONFIG_STRING, &RegAliasHotkey},
    {"Theme", CONFIG_STRING, &Theme},
    {"BackgroundImage", CONFIG_STRING, &BackgroundImage},
    {"MessagesEnabled", CONFIG_BOOL, &MessagesEnabled},
    {"EnableAutoPairing", CONFIG_BOOL, &EnableAutoPairing},
    {"EnableBorders", CONFIG_BOOL, &EnableBorders},
    {"RainbowText", CONFIG_BOOL, &RainbowText},
    {"WindowTitleText", CONFIG_BOOL, &WindowTitleText},
    {"CloseOnLossOfFocus", CONFIG_BOOL, &CloseOnLossOfFocus},
    {"COLOR_BG", CONFIG_COLORREF, &COLOR_BG},
    {"COLOR_TEXT", CONFIG_COLORREF, &COLOR_TEXT},
    {"COLOR_SELECTED_TEXT", CONFIG_COLORREF, &COLOR_SELECTED_TEXT},
    {"COLOR_CURSOR", CONFIG_COLORREF, &COLOR_CURSOR},
    {"COLOR_BORDER", CONFIG_COLORREF, &COLOR_BORDER},
};
#define CONFIG_ENTRY_COUNT                                                     \
  ((int)(sizeof(config_entries) / sizeof(config_entries[0])))

static char *config_owned[CONFIG_ENTRY_COUNT];

static int config_find(const char *section, const char *key) {
  if (!key || !key[0])
    return -1;
  bool color_section = section && strcmp(section, "color") == 0;
  bool root_or_color = !section || !section[0] || color_section;
  if (!root_or_color)
    return -1;
  for (int i = 0; i < CONFIG_ENTRY_COUNT; i++) {
    const ConfigEntry *e = &config_entries[i];
    if (color_section) {
      if (e->kind != CONFIG_COLORREF)
        continue;
      const char *short_name = e->name + 6;
      if (strcmp(short_name, key) == 0)
        return i;
    } else {
      if (e->kind == CONFIG_COLORREF)
        continue;
      if (strcmp(e->name, key) == 0)
        return i;
    }
  }
  return -1;
}

typedef struct {
  int idx;
  bool b;
  long i;
  double f;
  char *s;
  ColorRef cr;
} ConfigAssign;

typedef struct {
  ConfigAssign items[CONFIG_ENTRY_COUNT];
  size_t count;
} ConfigStaging;

static bool config_visit(const ConfigTomlEntry *e, void *user) {
  ConfigStaging *st = (ConfigStaging *)user;
  if (!st)
    return false;
  if (st->count >= CONFIG_ENTRY_COUNT)
    return false;
  int idx = config_find(e->section, e->key);
  if (idx < 0) {
    log_error("config: unknown key '%s' in [%s]", e->key,
              e->section[0] ? e->section : "(root)");
    return false;
  }
  const ConfigEntry *ent = &config_entries[idx];
  ConfigAssign *a = &st->items[st->count];
  memset(a, 0, sizeof(*a));
  a->idx = idx;

  switch (ent->kind) {
  case CONFIG_BOOL:
    if (strcmp(e->raw, "true") == 0)
      a->b = true;
    else if (strcmp(e->raw, "false") == 0)
      a->b = false;
    else {
      log_error("config: bad bool for '%s'", ent->name);
      return false;
    }
    break;
  case CONFIG_INT: {
    errno = 0;
    char *end = NULL;
    long v = strtol(e->raw, &end, 10);
    if (errno || !end || end[0] != '\0' || v < INT32_MIN || v > INT32_MAX) {
      log_error("config: bad integer for '%s'", ent->name);
      return false;
    }
    a->i = v;
    break;
  }
  case CONFIG_FLOAT: {
    errno = 0;
    char *end = NULL;
    double v = strtod(e->raw, &end);
    if (errno || !end || end[0] != '\0' || !isfinite(v)) {
      log_error("config: bad float for '%s'", ent->name);
      return false;
    }
    a->f = v;
    break;
  }
  case CONFIG_STRING: {
    char body[1024];
    if (!config_toml_decode_string(e->raw, body, sizeof(body))) {
      log_error("config: bad string for '%s'", ent->name);
      return false;
    }
    a->s = strdup(body);
    if (!a->s)
      return false;
    break;
  }
  case CONFIG_COLORREF: {
    char body[1024];
    if (!config_toml_decode_string(e->raw, body, sizeof(body)) ||
        !config_toml_color_ref(body, &a->cr)) {
      log_error("config: bad color for '%s'", ent->name);
      return false;
    }
    break;
  }
  }
  st->count++;
  return true;
}

static void config_staging_free(ConfigStaging *st) {
  for (size_t i = 0; i < st->count; i++)
    free(st->items[i].s);
  st->count = 0;
}

static void config_staging_commit(ConfigStaging *st) {
  for (size_t i = 0; i < st->count; i++) {
    ConfigAssign *a = &st->items[i];
    const ConfigEntry *ent = &config_entries[a->idx];
    switch (ent->kind) {
    case CONFIG_BOOL:
      *(bool *)ent->slot = a->b;
      break;
    case CONFIG_INT:
      *(int *)ent->slot = (int)a->i;
      break;
    case CONFIG_FLOAT:
      *(float *)ent->slot = (float)a->f;
      break;
    case CONFIG_STRING: {
      char *old = config_owned[a->idx];
      *(const char **)ent->slot = a->s;
      config_owned[a->idx] = a->s;
      free(old);
      break;
    }
    case CONFIG_COLORREF:
      *(ColorRef *)ent->slot = a->cr;
      break;
    }
  }
  st->count = 0;
}

static long long config_cached_mtime = 0;
static long long config_cached_size = -1;

static void config_cache_info(const char *path) {
  long long mtime = 0, size = 0;
  if (Platform_FileInfo(path, &mtime, &size)) {
    config_cached_mtime = mtime;
    config_cached_size = size;
  }
}

static bool config_file_unchanged(void) {
  long long mtime = 0, size = 0;
  if (!Platform_FileInfo(CONFIG_SRC, &mtime, &size))
    return false;
  return mtime == config_cached_mtime && size == config_cached_size;
}

static bool config_load_path(const char *path) {
  ConfigStaging st;
  memset(&st, 0, sizeof(st));
  ConfigTomlStatus status = config_toml_parse(path, config_visit, &st);
  if (status != CONFIG_TOML_OK) {
    config_staging_free(&st);
    if (status == CONFIG_TOML_ERR_IO)
      log_error("config open %s failed, running on defaults", path);
    else
      log_error("config parse failed for %s, keeping previous config", path);
    return false;
  }
  config_staging_commit(&st);
  theme_apply();
  config_cache_info(path);
  log_info("config loaded from %s", path);
  return true;
}

bool config_load(void) { return config_load_path(CONFIG_SRC); }

static long long config_last_reload_ms = 0;
static void *config_reload_mutex = NULL;

static void config_reload_lock(void) {
  if (!config_reload_mutex)
    config_reload_mutex = Platform_MutexCreate();
  if (config_reload_mutex)
    Platform_MutexLock(config_reload_mutex);
}

static void config_reload_unlock(void) {
  if (config_reload_mutex)
    Platform_MutexUnlock(config_reload_mutex);
}

bool config_reload(void) {
  config_reload_lock();
  long long now = Platform_GetWallClockMs();
  long long debounce = (long long)1000;
  if (config_last_reload_ms && now - config_last_reload_ms < debounce) {
    log_warn("cfg reload blocked: debounce, %lldms since last",
             (long long)(now - config_last_reload_ms));
    config_reload_unlock();
    return false;
  }
  if (config_file_unchanged()) {
    log_info("config reload skipped: %s unchanged", CONFIG_SRC);
    config_reload_unlock();
    return false;
  }
  bool ok = config_load_path(CONFIG_SRC);
  if (!ok) {
    log_warn("cfg reload blocked: load failed, keeping old values");
    config_reload_unlock();
    return false;
  }
  config_last_reload_ms = Platform_GetWallClockMs();
  Platform_ResetUIScale();
  Platform_PrewarmBackgroundPixmap();
  UI_RequestPopupRepaint();
  config_reload_unlock();
  return true;
}

void config_unload(void) {
  for (int i = 0; i < CONFIG_ENTRY_COUNT; i++) {
    free(config_owned[i]);
    config_owned[i] = NULL;
  }
}

typedef struct {
  char *data;
  size_t len;
  size_t cap;
} ConfigBuf;

static void config_buf_free(ConfigBuf *b) {
  free(b->data);
  b->data = NULL;
  b->len = 0;
  b->cap = 0;
}

static void config_buf_put(ConfigBuf *b, const char *fmt, ...) {
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

static void config_emit_bool_line(ConfigBuf *b, const char *name, bool v) {
  config_buf_put(b, "  %s = %s\n", name, v ? "true" : "false");
}

static void config_emit_int_line(ConfigBuf *b, const char *name, int v) {
  config_buf_put(b, "  %s = %d\n", name, v);
}

static void config_emit_float_line(ConfigBuf *b, const char *name, float v) {
  char tmp[64];
  snprintf(tmp, sizeof(tmp), "%.6g", (double)v);
  if (!strchr(tmp, '.') && !strchr(tmp, 'e') && !strchr(tmp, 'E') &&
      !strchr(tmp, 'n') && !strchr(tmp, 'N')) {
    size_t n = strlen(tmp);
    snprintf(tmp + n, sizeof(tmp) - n, ".0");
  }
  config_buf_put(b, "  %s = %s\n", name, tmp);
}

static void config_emit_str_line(ConfigBuf *b, const char *name,
                                 const char *value) {
  const char *out = value ? value : "";
  if (config_is_auto_resolved_app(name))
    out = "";
  char enc[1024];
  if (!config_toml_encode_string(out, enc, sizeof(enc)))
    return;
  config_buf_put(b, "  %s = %s\n", name, enc);
}

static void config_emit_color_line(ConfigBuf *b, const char *name, ColorRef v) {
  unsigned r = v & 0xFFu, g = (v >> 8) & 0xFFu, b_ = (v >> 16) & 0xFFu;
  config_buf_put(b, "  %s = \"#%02X%02X%02X\"\n", name, r, g, b_);
}

static ConfigBuf config_emit_region(void) {
  ConfigBuf b = {0};
  config_buf_put(&b, "# <skald-config-generated>\n");
  for (int i = 0; i < CONFIG_ENTRY_COUNT; i++) {
    const ConfigEntry *e = &config_entries[i];
    if (e->kind == CONFIG_COLORREF)
      continue;
    const char *name = e->name;
    switch (e->kind) {
    case CONFIG_BOOL:
      config_emit_bool_line(&b, name, *(const bool *)e->slot);
      break;
    case CONFIG_INT:
      config_emit_int_line(&b, name, *(const int *)e->slot);
      break;
    case CONFIG_FLOAT:
      config_emit_float_line(&b, name, *(const float *)e->slot);
      break;
    case CONFIG_STRING:
      config_emit_str_line(&b, name, *(const char *const *)e->slot);
      break;
    case CONFIG_COLORREF:
      break;
    }
  }
  config_buf_put(&b, "\n[color]\n");
  for (int i = 0; i < CONFIG_ENTRY_COUNT; i++) {
    const ConfigEntry *e = &config_entries[i];
    if (e->kind != CONFIG_COLORREF)
      continue;
    const char *short_name = e->name + 6;
    config_emit_color_line(&b, short_name, *(const ColorRef *)e->slot);
  }
  config_buf_put(&b, "# </skald-config-generated>\n");
  return b;
}

void config_save(void) {
  ConfigBuf region = config_emit_region();
  if (!region.data) {
    log_error("config emit failed");
    return;
  }
  size_t n = region.len;
  const char *region_data = region.data;
  FILE *f = fopen(CONFIG_SRC, "rb");
  if (!f) {
    log_error("config open %s failed", CONFIG_SRC);
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
  const char *start = strstr(orig, CONFIG_START_MARK);
  const char *end =
      start ? strstr(start + strlen(CONFIG_START_MARK), CONFIG_END_MARK) : NULL;
  if (!start || !end || end < start) {
    free(orig);
    log_error("config markers missing, not touching file");
    goto out;
  }
  const char *after_end = end;
  while (*after_end && *after_end != '\n')
    after_end++;
  if (*after_end == '\n')
    after_end++;
  size_t head = (size_t)(start - orig);
  size_t tail_off = (size_t)(after_end - orig);
  size_t tail = got - tail_off;
  char tmp[512];
  snprintf(tmp, sizeof(tmp), "%s.tmp.%d", CONFIG_SRC, (int)getpid());
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
    ok = fwrite(after_end, 1, tail, out) == tail;
  if (fclose(out) != 0)
    ok = false;
  free(orig);
  if (!ok) {
    remove(tmp);
    log_error("config write failed");
    goto out;
  }
  if (!Platform_AtomicRename(tmp, CONFIG_SRC)) {
    remove(tmp);
    log_error("config rename failed");
    goto out;
  }
  log_info("config saved");
out:
  config_buf_free(&region);
}
