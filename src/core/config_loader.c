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
#include "config.h"
#include "core/theme.h"
#include "deps/log.h"
#include "config_values.h"
#include "core/types/color.h"
#include "core/config_toml.h"
#include "config_defs.h"
#include "helpers/helpers_ui.h"

#define CONFISRC "config/config.toml"
#define CONFISTART_MARK "# <skald-config-generated>"
#define CONFIEND_MARK "# </skald-config-generated>"

typedef enum { CFG_BOOL, CFG_INT, CFG_FLOAT, CFG_STRING, CFG_COLORREF } CfgKind;

typedef struct {
  const char *name;
  CfgKind kind;
  void *slot;
} CfgEntry;

static const CfgEntry cfg_entries[] = {
    {"Tray", CFG_BOOL, &Tray},
    {"RememberLastIndex", CFG_BOOL, &RememberLastIndex},
    {"MessageAfterEachHotkey", CFG_BOOL, &MessageAfterEachHotkey},
    {"TimeOutForMessages", CFG_INT, &TimeOutForMessages},
    {"HotkeyRepeatDelayMs", CFG_INT, &HotkeyRepeatDelayMs},
    {"DebugMode", CFG_BOOL, &DebugMode},
    {"RoundedCorners", CFG_BOOL, &RoundedCorners},
    {"CornerRadius", CFG_INT, &CornerRadius},
    {"MessageScreenGap", CFG_INT, &MessageScreenGap},
    {"WindowOpacity", CFG_FLOAT, &WindowOpacity},
    {"UsedFont", CFG_STRING, &UsedFont},
    {"FontSize", CFG_INT, &FontSize},
    {"IconSize", CFG_INT, &IconSize},
    {"WidgetGap", CFG_INT, &WidgetGap},
    {"PromptWidth", CFG_INT, &PromptWidth},
    {"PromptHeight", CFG_INT, &PromptHeight},
    {"PromptX", CFG_INT, &PromptX},
    {"PromptY", CFG_INT, &PromptY},
    {"UsedTerminal", CFG_STRING, &UsedTerminal},
    {"UsedShell", CFG_STRING, &UsedShell},
    {"UsedBrowser", CFG_STRING, &UsedBrowser},
    {"UsedSearchEngine", CFG_STRING, &UsedSearchEngine},
    {"FeaturePath", CFG_STRING, &FeaturePath},
    {"IconPath", CFG_STRING, &IconPath},
    {"ControlHotkey", CFG_STRING, &ControlHotkey},
    {"ReloadFeaturesHotkey", CFG_STRING, &ReloadFeaturesHotkey},
    {"ActionsHotkey", CFG_STRING, &ActionsHotkey},
    {"RegHotkey", CFG_STRING, &RegHotkey},
    {"RegAliasHotkey", CFG_STRING, &RegAliasHotkey},
    {"Theme", CFG_STRING, &Theme},
    {"BackgroundImage", CFG_STRING, &BackgroundImage},
    {"MessagesEnabled", CFG_BOOL, &MessagesEnabled},
    {"EnableAutoPairing", CFG_BOOL, &EnableAutoPairing},
    {"EnableBorders", CFG_BOOL, &EnableBorders},
    {"RainbowText", CFG_BOOL, &RainbowText},
    {"WindowTitleText", CFG_BOOL, &WindowTitleText},
    {"CloseOnLossOfFocus", CFG_BOOL, &CloseOnLossOfFocus},
    {"COLOR_BG", CFG_COLORREF, &COLOR_BG},
    {"COLOR_TEXT", CFG_COLORREF, &COLOR_TEXT},
    {"COLOR_SELECTED_TEXT", CFG_COLORREF, &COLOR_SELECTED_TEXT},
    {"COLOR_CURSOR", CFG_COLORREF, &COLOR_CURSOR},
    {"COLOR_BORDER", CFG_COLORREF, &COLOR_BORDER},
};
#define CFG_ENTRY_COUNT ((int)(sizeof(cfg_entries) / sizeof(cfg_entries[0])))

static char *cfg_owned[CFG_ENTRY_COUNT];

static int cfg_find(const char *section, const char *key) {
  if (!key || !key[0])
    return -1;
  bool color_section = section && strcmp(section, "color") == 0;
  bool root_or_color = !section || !section[0] || color_section;
  if (!root_or_color)
    return -1;
  for (int i = 0; i < CFG_ENTRY_COUNT; i++) {
    const CfgEntry *e = &cfg_entries[i];
    if (color_section) {
      if (e->kind != CFG_COLORREF)
        continue;
      const char *short_name = e->name + 6;
      if (strcmp(short_name, key) == 0)
        return i;
    } else {
      if (e->kind == CFG_COLORREF)
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
} CfgAssign;

typedef struct {
  CfgAssign items[CFG_ENTRY_COUNT];
  size_t count;
} CfgStaging;

static bool cfg_visit(const CfgTomlEntry *e, void *user) {
  CfgStaging *st = (CfgStaging *)user;
  if (!st || st->count >= CFG_ENTRY_COUNT)
    return false;
  int idx = cfg_find(e->section, e->key);
  if (idx < 0) {
    log_error("config: unknown key '%s' in [%s]", e->key,
              e->section[0] ? e->section : "(root)");
    return false;
  }
  const CfgEntry *ent = &cfg_entries[idx];
  CfgAssign *a = &st->items[st->count];
  memset(a, 0, sizeof(*a));
  a->idx = idx;

  switch (ent->kind) {
  case CFG_BOOL:
    if (strcmp(e->raw, "true") == 0)
      a->b = true;
    else if (strcmp(e->raw, "false") == 0)
      a->b = false;
    else {
      log_error("config: bad bool for '%s'", ent->name);
      return false;
    }
    break;
  case CFG_INT: {
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
  case CFG_FLOAT: {
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
  case CFG_STRING: {
    char body[1024];
    if (!cfg_toml_decode_string(e->raw, body, sizeof(body))) {
      log_error("config: bad string for '%s'", ent->name);
      return false;
    }
    a->s = strdup(body);
    if (!a->s)
      return false;
    break;
  }
  case CFG_COLORREF: {
    char body[1024];
    if (!cfg_toml_decode_string(e->raw, body, sizeof(body)) ||
        !cfg_toml_color_ref(body, &a->cr)) {
      log_error("config: bad color for '%s'", ent->name);
      return false;
    }
    break;
  }
  }
  st->count++;
  return true;
}

static void cfg_staging_free(CfgStaging *st) {
  for (size_t i = 0; i < st->count; i++)
    free(st->items[i].s);
  st->count = 0;
}

static void cfg_staging_commit(CfgStaging *st) {
  for (size_t i = 0; i < st->count; i++) {
    CfgAssign *a = &st->items[i];
    const CfgEntry *ent = &cfg_entries[a->idx];
    switch (ent->kind) {
    case CFG_BOOL:
      *(bool *)ent->slot = a->b;
      break;
    case CFG_INT:
      *(int *)ent->slot = (int)a->i;
      break;
    case CFG_FLOAT:
      *(float *)ent->slot = (float)a->f;
      break;
    case CFG_STRING: {
      char *old = cfg_owned[a->idx];
      *(const char **)ent->slot = a->s;
      cfg_owned[a->idx] = a->s;
      free(old);
      break;
    }
    case CFG_COLORREF:
      *(ColorRef *)ent->slot = a->cr;
      break;
    }
  }
  st->count = 0;
}

static bool cfg_load_path(const char *path) {
  CfgStaging st;
  memset(&st, 0, sizeof(st));
  CfgTomlStatus status = cfg_toml_parse(path, cfg_visit, &st);
  if (status != CFG_TOML_OK) {
    cfg_staging_free(&st);
    if (status == CFG_TOML_ERR_IO)
      log_error("config open %s failed, running on defaults", path);
    else
      log_error("config parse failed for %s, keeping previous config", path);
    return false;
  }
  cfg_staging_commit(&st);
  theme_apply();
  log_info("config loaded from %s", path);
  return true;
}

bool config_load(void) { return cfg_load_path(CONFISRC); }

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
  bool ok = cfg_load_path(CONFISRC);
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
  for (int i = 0; i < CFG_ENTRY_COUNT; i++) {
    free(cfg_owned[i]);
    cfg_owned[i] = NULL;
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
      !strchr(tmp, 'n') && !strchr(tmp, 'N'))
    snprintf(tmp, sizeof(tmp), "%s.0", tmp);
  config_buf_put(b, "  %s = %s\n", name, tmp);
}

static void config_emit_str_line(ConfigBuf *b, const char *name,
                                 const char *value) {
  const char *out = value ? value : "";
  if (config_is_auto_resolved_app(name))
    out = "";
  char enc[1024];
  if (!cfg_toml_encode_string(out, enc, sizeof(enc)))
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
  for (int i = 0; i < CFG_ENTRY_COUNT; i++) {
    const CfgEntry *e = &cfg_entries[i];
    if (e->kind == CFG_COLORREF)
      continue;
    const char *name = e->name;
    switch (e->kind) {
    case CFG_BOOL:
      config_emit_bool_line(&b, name, *(const bool *)e->slot);
      break;
    case CFG_INT:
      config_emit_int_line(&b, name, *(const int *)e->slot);
      break;
    case CFG_FLOAT:
      config_emit_float_line(&b, name, *(const float *)e->slot);
      break;
    case CFG_STRING:
      config_emit_str_line(&b, name, *(const char *const *)e->slot);
      break;
    case CFG_COLORREF:
      break;
    }
  }
  config_buf_put(&b, "\n[color]\n");
  for (int i = 0; i < CFG_ENTRY_COUNT; i++) {
    const CfgEntry *e = &cfg_entries[i];
    if (e->kind != CFG_COLORREF)
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
  const char *start = strstr(orig, CONFISTART_MARK);
  const char *end =
      start ? strstr(start + strlen(CONFISTART_MARK), CONFIEND_MARK) : NULL;
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
    ok = fwrite(after_end, 1, tail, out) == tail;
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
  log_info("config saved");
out:
  config_buf_free(&region);
}
