#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "core/config_section.h"
#include "core/context.h"
#include "core/hotkey_manager.h"
#include "core/cc_registry.h"
#include "core/window_manager.h"
#include "core/owner_registry.h"
#include "helpers/helpers_fs.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_misc.h"
#include "helpers/helpers_text.h"
#include "helpers/helpers_ui.h"
#include "helpers/helpers_owner.h"
#include "config_values.h"
#include "deps/stb_ds.h"
#include "core/config_toml.h"
#include "core/toml_common.h"
#include "deps/log.h"
#include "platform/platform.h"

#define CONFIG_SECTION_HEADER_MAX 96
#define CONFIG_SECTION_NAME_MAX (CONFIG_SECTION_HEADER_MAX - 3)

static ConfigTomlStatus section_fail(ConfigTomlError *err,
                                     ConfigTomlStatus status,
                                     const char *message) {
  if (err) {
    err->status = status;
    err->line = 0;
    snprintf(err->message, sizeof(err->message), "%s", message ? message : "");
  }
  return status;
}

static bool section_encode_key(char *out, size_t cap, const char *name) {
  if (toml_is_bare_key(name) && strlen(name) < 384) {
    if (strlen(name) >= cap)
      return false;
    memcpy(out, name, strlen(name) + 1);
    return true;
  }
  return config_toml_encode_string(name, out, cap);
}

static char *section_trim(const char *s, size_t len) {
  s = toml_trim_left(s);
  len = toml_trim_right_len(s, len);
  char *out = (char *)malloc(len + 1);
  if (!out)
    return NULL;
  memcpy(out, s, len);
  out[len] = '\0';
  return out;
}

static int section_parse_entry_line(const char *line, size_t line_len,
                                    char **key_out, char **value_out) {
  *key_out = NULL;
  *value_out = NULL;
  char key[384];
  char raw[1024];
  TomlKeyValueStatus st =
      toml_parse_key_value(line, line_len, key, sizeof(key), raw, sizeof(raw));
  if (st == TOML_KV_EMPTY)
    return 0;
  if (st != TOML_KV_OK)
    return -1;
  char value[1024];
  if (!toml_decode_string(raw, value, sizeof(value)))
    return -1;
  *key_out = strdup(key);
  *value_out = strdup(value);
  if (!*key_out || !*value_out) {
    free(*key_out);
    free(*value_out);
    *key_out = NULL;
    *value_out = NULL;
    return -1;
  }
  return 1;
}

typedef struct {
  size_t count;
  size_t offset;
} ConfigSectionLine;

static void section_append_entry(char *out, size_t *out_len,
                                 const char *enc_key, const char *enc_val) {
  memcpy(out + *out_len, enc_key, strlen(enc_key));
  *out_len += strlen(enc_key);
  memcpy(out + *out_len, " = ", 3);
  *out_len += 3;
  memcpy(out + *out_len, enc_val, strlen(enc_val));
  *out_len += strlen(enc_val);
  out[(*out_len)++] = '\n';
}

static bool section_apply(const char *path, const char *section,
                          const char *name, const char *value, bool is_clear) {
  if (!toml_is_bare_key(section) ||
      strlen(section) >= CONFIG_SECTION_NAME_MAX) {
    log_error("config-section: invalid or too long section name '%s'", section);
    return false;
  }

  char *orig = NULL;
  long fsize = 0;
  FILE *f = fopen(path, "rb");
  if (f) {
    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize < 0 || fsize > 1048576) {
      fclose(f);
      log_error("config-section: %s size bad", path);
      return false;
    }
    orig = malloc((size_t)fsize + 1);
    if (!orig) {
      fclose(f);
      return false;
    }
    size_t got = fread(orig, 1, (size_t)fsize, f);
    fclose(f);
    if (got != (size_t)fsize) {
      free(orig);
      log_error("config-section: %s read short", path);
      return false;
    }
    orig[got] = '\0';
  } else {
    if (is_clear)
      return true;
    fsize = 0;
  }

  size_t line_cap = 64, line_count = 0;
  ConfigSectionLine *lines = malloc(line_cap * sizeof(*lines));
  if (!lines) {
    free(orig);
    return false;
  }
  if (orig) {
    size_t pos = 0;
    while (pos < (size_t)fsize) {
      size_t end = pos;
      while (end < (size_t)fsize && orig[end] != '\n')
        end++;
      if (line_count == line_cap) {
        size_t ncap = line_cap * 2;
        ConfigSectionLine *grown = realloc(lines, ncap * sizeof(*grown));
        if (!grown) {
          free(lines);
          free(orig);
          return false;
        }
        lines = grown;
        line_cap = ncap;
      }
      lines[line_count].count = end - pos;
      lines[line_count].offset = pos;
      line_count++;
      pos = end + 1;
    }
  }

  char header[CONFIG_SECTION_HEADER_MAX];
  snprintf(header, sizeof(header), "[%s]", section);

  size_t sec_header = SIZE_MAX;
  for (size_t i = 0; i < line_count; i++) {
    const char *s = orig + lines[i].offset;
    size_t len = lines[i].count;
    char *trimmed = section_trim(s, len);
    bool is_match = trimmed && strcmp(trimmed, header) == 0;
    free(trimmed);
    if (is_match) {
      if (sec_header != SIZE_MAX) {
        free(lines);
        free(orig);
        log_error(
            "config-section: multiple [%s] sections in %s, not touching file",
            section, path);
        return false;
      }
      sec_header = i;
    }
  }

  size_t sec_end = line_count;
  if (sec_header != SIZE_MAX) {
    for (size_t i = sec_header + 1; i < line_count; i++) {
      const char *s = orig + lines[i].offset;
      size_t len = lines[i].count;
      char *trimmed = section_trim(s, len);
      bool is_header =
          trimmed && trimmed[0] == '[' && trimmed[strlen(trimmed) - 1] == ']';
      free(trimmed);
      if (is_header) {
        sec_end = i;
        break;
      }
    }
  }

  char enc_key[512] = {0}, enc_val[1100] = {0};
  if (!is_clear) {
    if (!section_encode_key(enc_key, sizeof(enc_key), name)) {
      free(lines);
      free(orig);
      log_error("config-section: key '%s' too long or invalid", name);
      return false;
    }
    if (!config_toml_encode_string(value, enc_val, sizeof(enc_val)) ||
        strlen(enc_val) >= 1024 || strlen(enc_key) >= 384) {
      free(lines);
      free(orig);
      log_error("config-section: value too long for '%s'", name);
      return false;
    }
  }

  size_t replace_idx = SIZE_MAX;
  if (sec_header != SIZE_MAX) {
    for (size_t i = sec_header + 1; i < sec_end; i++) {
      char *key = NULL, *entry_value = NULL;
      int pr = section_parse_entry_line(orig + lines[i].offset, lines[i].count,
                                        &key, &entry_value);
      if (pr < 0) {
        free(lines);
        free(orig);
        log_error("config-section: malformed line %zu in [%s] of %s, not "
                  "touching file",
                  i + 1, section, path);
        return false;
      }
      if (pr == 0)
        continue;
      bool match = strcmp(key, name) == 0;
      free(key);
      free(entry_value);
      if (match) {
        replace_idx = i;
        break;
      }
    }
  }

  bool found = sec_header != SIZE_MAX;
  size_t entry_len = strlen(enc_key) + 3 + strlen(enc_val) + 1;

  size_t cap = 0;
  for (size_t i = 0; i < line_count; i++) {
    if (is_clear && i == replace_idx)
      continue;
    if (!is_clear && i == replace_idx) {
      cap += entry_len;
      continue;
    }
    if (!is_clear && sec_header != SIZE_MAX && i == sec_end &&
        replace_idx == SIZE_MAX)
      cap += entry_len;
    cap += lines[i].count;
    if (lines[i].count == 0 ||
        orig[lines[i].offset + lines[i].count - 1] != '\n')
      cap += 1;
  }
  if (!found && !is_clear) {
    cap += 1;
    cap += 1 + strlen(header) + 1;
    cap += entry_len;
  } else if (!is_clear && replace_idx == SIZE_MAX && sec_end == line_count) {
    cap += entry_len;
  }
  char *out = malloc(cap + 1);
  if (!out) {
    free(lines);
    free(orig);
    return false;
  }
  size_t out_len = 0;

  for (size_t i = 0; i < line_count; i++) {
    if (is_clear && i == replace_idx)
      continue;
    if (!is_clear && i == replace_idx) {
      section_append_entry(out, &out_len, enc_key, enc_val);
      continue;
    }
    if (!is_clear && sec_header != SIZE_MAX && i == sec_end &&
        replace_idx == SIZE_MAX) {
      section_append_entry(out, &out_len, enc_key, enc_val);
    }
    memcpy(out + out_len, orig + lines[i].offset, lines[i].count);
    out_len += lines[i].count;
    if (lines[i].count == 0 ||
        orig[lines[i].offset + lines[i].count - 1] != '\n')
      out[out_len++] = '\n';
  }

  if (!found && !is_clear) {
    if (out_len > 0 && out[out_len - 1] != '\n')
      out[out_len++] = '\n';
    memcpy(out + out_len, "\n", 1);
    out_len += 1;
    memcpy(out + out_len, header, strlen(header));
    out_len += strlen(header);
    out[out_len++] = '\n';
    section_append_entry(out, &out_len, enc_key, enc_val);
  } else if (!is_clear && replace_idx == SIZE_MAX && sec_end == line_count) {
    section_append_entry(out, &out_len, enc_key, enc_val);
  }

  char tmp[512];
  snprintf(tmp, sizeof(tmp), "%s.tmp.%d", path, (int)getpid());
  FILE *outf = fopen(tmp, "wb");
  bool ok = outf != NULL;
  if (ok) {
    if (fwrite(out, 1, out_len, outf) != out_len)
      ok = false;
    if (fclose(outf) != 0)
      ok = false;
  }
  free(out);
  free(lines);
  free(orig);
  if (!ok) {
    remove(tmp);
    log_error("config-section: write to %s failed", tmp);
    return false;
  }
  if (!Platform_AtomicRename(tmp, path)) {
    remove(tmp);
    log_error("config-section: rename %s failed", tmp);
    return false;
  }
  return true;
}

ConfigTomlStatus config_section_set_ex(const char *path, const char *section,
                                       const char *name, const char *value,
                                       ConfigTomlError *err) {
  config_toml_error_clear(err);
  if (!path || !path[0] || !section || !section[0] || !name || !name[0] ||
      !value || !value[0])
    return section_fail(err, CONFIG_TOML_ERR_ARG,
                        "missing path/section/name/value");
  if (!toml_is_bare_key(section) || strlen(section) >= CONFIG_SECTION_NAME_MAX)
    return section_fail(err, CONFIG_TOML_ERR_PARSE, "invalid section name");
  if (!section_apply(path, section, name, value, false))
    return section_fail(err, CONFIG_TOML_ERR_IO,
                        "could not update config section");
  return CONFIG_TOML_OK;
}

ConfigTomlStatus config_section_clear_ex(const char *path, const char *section,
                                         const char *name,
                                         ConfigTomlError *err) {
  config_toml_error_clear(err);
  if (!path || !path[0] || !section || !section[0] || !name || !name[0])
    return section_fail(err, CONFIG_TOML_ERR_ARG, "missing path/section/name");
  if (!toml_is_bare_key(section) || strlen(section) >= CONFIG_SECTION_NAME_MAX)
    return section_fail(err, CONFIG_TOML_ERR_PARSE, "invalid section name");
  if (!section_apply(path, section, name, NULL, true))
    return section_fail(err, CONFIG_TOML_ERR_IO,
                        "could not clear config section");
  return CONFIG_TOML_OK;
}

bool config_section_set(const char *path, const char *section, const char *name,
                        const char *value) {
  return config_section_set_ex(path, section, name, value, NULL) ==
         CONFIG_TOML_OK;
}

bool config_section_clear(const char *path, const char *section,
                          const char *name) {
  return config_section_clear_ex(path, section, name, NULL) == CONFIG_TOML_OK;
}

#define CONFIG_SETTING_KEY_MAX 384
#define CONFIG_SETTING_VALUE_MAX 1024
#define CONFIG_NAME_MAX 384
#define CONFIG_HOTKEY_MAX 64
#define CONFIG_ALIAS_MAX 128
#define CONFIG_TARGET_MAX 256
#define CONFIG_QUICKLINK_TARGET_MAX 1024
#define CONFIG_FEATURE_SHORTCUT_TARGET_MAX 1024

#if defined(_WIN32)
#define CONFIG_PATH_SEP '\\'
#else
#define CONFIG_PATH_SEP '/'
#endif

typedef struct {
  char *key;
  char *value;
} ConfigSettingEntry;

typedef struct {
  char name[CONFIG_NAME_MAX];
  char hotkey[CONFIG_HOTKEY_MAX];
} ConfigHotkeyEntry;

typedef struct {
  char target[CONFIG_TARGET_MAX];
  char alias[CONFIG_ALIAS_MAX];
} ConfigAliasEntry;

typedef struct {
  char name[CONFIG_NAME_MAX];
  char target[CONFIG_QUICKLINK_TARGET_MAX];
} ConfigQuicklinkEntry;

typedef struct {
  char name[CONFIG_NAME_MAX];
  char target[CONFIG_FEATURE_SHORTCUT_TARGET_MAX];
} ConfigFeatureShortcutEntry;

typedef struct {
  char *display_name;
  char *key;
  char *default_hotkey;
  char **whitelist;
  char **blacklist;
  void (*cb)(void);
  HotkeyCallbackUd cb_ud;
  void *ud;
  bool (*should_swallow)(void);
  OwnerTag tag;
} ConfigHotkeyBinding;

typedef struct {
  char *alias;
  char *target;
} ConfigAliasStateEntry;

static ConfigSettingEntry *config_setting_entries;
#define config_setting_entry_count arrlenu(config_setting_entries)
static ConfigHotkeyEntry *hotkey_entries;
#define hotkey_entry_count arrlenu(hotkey_entries)
static ConfigAliasEntry *alias_entries;
#define alias_entry_count arrlenu(alias_entries)
static ConfigQuicklinkEntry *quicklink_entries;
#define quicklink_entry_count arrlenu(quicklink_entries)
static ConfigFeatureShortcutEntry *feature_shortcut_entries;
#define feature_shortcut_entry_count arrlenu(feature_shortcut_entries)
static ConfigHotkeyBinding *config_hotkey_bindings;
#define config_hotkey_binding_count arrlenu(config_hotkey_bindings)
static ConfigAliasStateEntry *alias_state_entries;
#define alias_state_count arrlenu(alias_state_entries)

static void *config_mutex = NULL;
static char config_path[1024];
static bool config_loaded = false;

static void config_hotkey_register_bound(const char *name, const char *hotkey,
                                         void (*cb)(void *), void *ud);

void config_lock(void) {
  lazy_mutex_get(&config_mutex);
  if (config_mutex)
    Platform_MutexLock(config_mutex);
}

void config_unlock(void) {
  if (config_mutex)
    Platform_MutexUnlock(config_mutex);
}

const char *config_setting_lookup_locked(const char *key) {
  for (size_t i = 0; i < config_setting_entry_count; i++)
    if (strcmp(config_setting_entries[i].key, key) == 0)
      return config_setting_entries[i].value;
  return NULL;
}

void config_setting_entries_set(const char *key, const char *value) {
  for (size_t i = 0; i < config_setting_entry_count; i++) {
    if (strcmp(config_setting_entries[i].key, key) == 0) {
      free(config_setting_entries[i].value);
      config_setting_entries[i].value = strdup(value ? value : "");
      return;
    }
  }
  ConfigSettingEntry *e = arraddnptr(config_setting_entries, 1);
  e->key = strdup(key);
  e->value = strdup(value ? value : "");
}

static int config_count_unlocked(ConfigEntryKind kind) {
  switch (kind) {
  case CONFIG_ENTRY_HOTKEY:
    return (int)hotkey_entry_count;
  case CONFIG_ENTRY_ALIAS:
    return (int)alias_entry_count;
  case CONFIG_ENTRY_QUICKLINK:
    return (int)quicklink_entry_count;
  case CONFIG_ENTRY_FEATURE_SHORTCUT:
    return (int)feature_shortcut_entry_count;
  case CONFIG_ENTRY_SETTING:
    return (int)config_setting_entry_count;
  }
  return 0;
}

static char *config_dup_unlocked(ConfigEntryKind kind, int index) {
  if (index < 0)
    return NULL;
  size_t i = (size_t)index;
  switch (kind) {
  case CONFIG_ENTRY_HOTKEY:
    return i < hotkey_entry_count ? strdup(hotkey_entries[i].hotkey) : NULL;
  case CONFIG_ENTRY_ALIAS:
    return i < alias_entry_count ? strdup(alias_entries[i].alias) : NULL;
  case CONFIG_ENTRY_QUICKLINK:
    return i < quicklink_entry_count ? strdup(quicklink_entries[i].target)
                                     : NULL;
  case CONFIG_ENTRY_FEATURE_SHORTCUT:
    return i < feature_shortcut_entry_count
               ? strdup(feature_shortcut_entries[i].target)
               : NULL;
  case CONFIG_ENTRY_SETTING:
    return i < config_setting_entry_count
               ? strdup(config_setting_entries[i].value)
               : NULL;
  }
  return NULL;
}

static const char *config_ptr_unlocked(ConfigEntryKind kind, int index) {
  if (index < 0)
    return NULL;
  size_t i = (size_t)index;
  switch (kind) {
  case CONFIG_ENTRY_HOTKEY:
    return i < hotkey_entry_count ? hotkey_entries[i].name : NULL;
  case CONFIG_ENTRY_ALIAS:
    return i < alias_entry_count ? alias_entries[i].target : NULL;
  case CONFIG_ENTRY_QUICKLINK:
    return i < quicklink_entry_count ? quicklink_entries[i].name : NULL;
  case CONFIG_ENTRY_FEATURE_SHORTCUT:
    return i < feature_shortcut_entry_count ? feature_shortcut_entries[i].name
                                            : NULL;
  case CONFIG_ENTRY_SETTING:
    return i < config_setting_entry_count ? config_setting_entries[i].key
                                          : NULL;
  }
  return NULL;
}

int config_entry_count(ConfigEntryKind kind) {
  config_lock();
  config_init_locked();
  int n = config_count_unlocked(kind);
  config_unlock();
  return n;
}

char *config_entry_value_at(ConfigEntryKind kind, int index) {
  config_lock();
  config_init_locked();
  char *out = config_dup_unlocked(kind, index);
  config_unlock();
  return out;
}

char *config_entry_dup_key_at(ConfigEntryKind kind, int index) {
  config_lock();
  config_init_locked();
  const char *key = config_ptr_unlocked(kind, index);
  char *out = key ? strdup(key) : NULL;
  config_unlock();
  return out;
}

const char *config_entry_key_at(ConfigEntryKind kind, int index) {
  config_lock();
  config_init_locked();
  const char *out = config_ptr_unlocked(kind, index);
  config_unlock();
  return out;
}

void config_parse_bool_value(const char *text, void *out) {
  setting_parse_bool(text, (bool *)out);
}

void config_parse_int_value(const char *text, void *out) {
  setting_parse_int(text, (int *)out);
}

void config_parse_float_value(const char *text, void *out) {
  setting_parse_float(text, (float *)out);
}

bool config_setting_get_value(const char *key,
                              void (*parse)(const char *, void *), void *out) {
  if (!key)
    return false;
  config_lock();
  config_init_locked();
  parse(config_setting_lookup_locked(key), out);
  config_unlock();
  return true;
}

bool config_setting_set(const char *key, const char *value) {
  if (!key || !key[0] || !value)
    return false;
  if (strlen(key) >= CONFIG_SETTING_KEY_MAX ||
      strlen(value) >= CONFIG_SETTING_VALUE_MAX) {
    log_error("sk_config: setting '%.80s' too long", key);
    return false;
  }
  config_lock();
  config_init_locked();
  config_setting_entries_set(key, value);
  if (!config_path[0] ||
      !config_section_set(config_path, "settings", key, value))
    log_error("sk_config: failed to save %s", config_path);
  config_unlock();
  return true;
}

char *config_setting_get(const char *key) {
  if (!key || !key[0])
    return NULL;
  config_lock();
  config_init_locked();
  const char *p = config_setting_lookup_locked(key);
  char *out = p ? strdup(p) : NULL;
  config_unlock();
  return out;
}

bool config_setting_has(const char *key) {
  config_lock();
  config_init_locked();
  bool out = key && config_setting_lookup_locked(key) != NULL;
  config_unlock();
  return out;
}

bool alias_register(const char *alias, const char *target) {
  if (!alias || !target)
    return false;
  for (size_t i = 0; i < alias_state_count; i++) {
    if (strcmp(alias_state_entries[i].target, target) == 0) {
      free(alias_state_entries[i].alias);
      alias_state_entries[i].alias = strdup(alias);
      return true;
    }
  }
  ConfigAliasStateEntry *e = arraddnptr(alias_state_entries, 1);
  e->alias = strdup(alias);
  e->target = strdup(target);
  return true;
}

bool alias_remove(const char *alias) {
  if (!alias)
    return false;
  for (size_t i = 0; i < alias_state_count;) {
    if (strcmp(alias_state_entries[i].alias, alias) == 0) {
      free(alias_state_entries[i].alias);
      free(alias_state_entries[i].target);
      arrdel(alias_state_entries, i);
      return true;
    }
    i++;
  }
  return false;
}

const char *alias_for(const char *target) {
  if (!target)
    return NULL;
  for (size_t i = 0; i < alias_state_count; i++) {
    if (strcmp(alias_state_entries[i].target, target) == 0)
      return alias_state_entries[i].alias;
  }
  return NULL;
}

static int hotkey_index_of(const char *name) {
  return CONFIG_INDEX_OF(hotkey_entries, hotkey_entry_count, ConfigHotkeyEntry,
                         name, name);
}

static int hotkey_index_by_key(const char *hotkey) {
  return CONFIG_INDEX_OF(hotkey_entries, hotkey_entry_count, ConfigHotkeyEntry,
                         hotkey, hotkey);
}

static int alias_index_of(const char *target) {
  return CONFIG_INDEX_OF(alias_entries, alias_entry_count, ConfigAliasEntry,
                         target, target);
}

static int quicklink_index_of(const char *name) {
  return CONFIG_INDEX_OF(quicklink_entries, quicklink_entry_count,
                         ConfigQuicklinkEntry, name, name);
}

static int feature_shortcut_index_of(const char *name) {
  return CONFIG_INDEX_OF(feature_shortcut_entries, feature_shortcut_entry_count,
                         ConfigFeatureShortcutEntry, name, name);
}

#define CONFIG_ENTRIES_SET(entries, count, type, find, key_field, val_field,   \
                           key, val)                                           \
  do {                                                                         \
    int _i = (find)(key);                                                      \
    if (_i >= 0) {                                                             \
      safe_copy((entries)[_i].val_field, sizeof((entries)[_i].val_field),      \
                (val));                                                        \
      return;                                                                  \
    }                                                                          \
    type *_e = arraddnptr((entries), 1);                                       \
    safe_copy(_e->key_field, sizeof(_e->key_field), (key));                    \
    safe_copy(_e->val_field, sizeof(_e->val_field), (val));                    \
  } while (0)

static void hotkey_entries_remove(int i) { arrdel(hotkey_entries, i); }
static void quicklink_entries_remove(int i) { arrdel(quicklink_entries, i); }
static void feature_shortcut_entries_remove(int i) {
  arrdel(feature_shortcut_entries, i);
}
static void alias_entries_remove(int i) { arrdel(alias_entries, i); }

static void hotkey_entries_set(const char *name, const char *hotkey) {
  CONFIG_ENTRIES_SET(hotkey_entries, hotkey_entry_count, ConfigHotkeyEntry,
                     hotkey_index_of, name, hotkey, name, hotkey);
}

static void quicklink_entries_set(const char *name, const char *target) {
  CONFIG_ENTRIES_SET(quicklink_entries, quicklink_entry_count,
                     ConfigQuicklinkEntry, quicklink_index_of, name, target,
                     name, target);
}

static void feature_shortcut_entries_set(const char *name, const char *target) {
  CONFIG_ENTRIES_SET(feature_shortcut_entries, feature_shortcut_entry_count,
                     ConfigFeatureShortcutEntry, feature_shortcut_index_of,
                     name, target, name, target);
}

static void alias_entries_set(const char *target, const char *alias) {
  CONFIG_ENTRIES_SET(alias_entries, alias_entry_count, ConfigAliasEntry,
                     alias_index_of, target, alias, target, alias);
}

static bool config_parse_path(void) {
  char dir[512];
  if (!data_dir(dir, sizeof(dir)))
    return false;
  Platform_MkdirIfNotExists(dir);
  char core_dir[512];
  snprintf(core_dir, sizeof(core_dir), "%s%ccore", dir, CONFIG_PATH_SEP);
  Platform_MkdirIfNotExists(core_dir);
  snprintf(config_path, sizeof(config_path), "%s%ccore%cskald.toml", dir,
           CONFIG_PATH_SEP, CONFIG_PATH_SEP);
  return true;
}

static bool config_ready(void) {
  return config_path[0] != '\0' || config_parse_path();
}

static bool config_apply_entry(const ConfigTomlEntry *e, void *user) {
  (void)user;
  char body[1024];
  if (e->type != CONFIG_TOML_STRING) {
    log_error("sk_config: skipping non-string value for '%s' in [%s]", e->key,
              e->section);
    return true;
  }
  if (!config_toml_decode_string(e->raw, body, sizeof(body))) {
    log_error("sk_config: bad string for '%s' in [%s]", e->key, e->section);
    return true;
  }
  if (strcmp(e->section, "hotkeys") == 0) {
    HotkeyKey key;
    if (!hotkey_string_parse(body, &key)) {
      log_error("sk_config: unparsable hotkey '%s' for '%s'", body, e->key);
      return true;
    }
    hotkey_entries_set(e->key, body);
  } else if (strcmp(e->section, "aliases") == 0) {
    alias_entries_set(e->key, body);
  } else if (strcmp(e->section, "quicklinks") == 0) {
    quicklink_entries_set(e->key, body);
  } else if (strcmp(e->section, "feature_shortcuts") == 0) {
    log_info("feature_shortcut: config '%s' = '%s'", e->key, body);
    feature_shortcut_entries_set(e->key, body);
  } else if (strcmp(e->section, "settings") == 0) {
    config_setting_entries_set(e->key, body);
  } else {
    log_warn("sk_config: unknown section [%s], ignoring", e->section);
  }
  return true;
}

void config_init_locked(void) {
  if (config_loaded)
    return;
  config_loaded = true;
  char dir[512];
  if (!data_dir(dir, sizeof(dir)))
    dir[0] = '\0';
  if (!config_parse_path()) {
    log_error("sk_config: data dir unavailable, config persistence disabled");
    return;
  }
  ConfigTomlError err;
  ConfigTomlStatus st =
      config_toml_parse_ex(config_path, config_apply_entry, NULL, &err);
  if (st == CONFIG_TOML_ERR_IO)
    log_info("sk_config: %s not found, starting with empty tables",
             config_path);
  else if (st != CONFIG_TOML_OK)
    log_error("sk_config: %s failed at line %d (%s): %s, keeping defaults",
              config_path, err.line, config_toml_status_name(st),
              err.message[0] ? err.message : "invalid config");

  for (size_t i = 0; i < alias_entry_count; i++)
    alias_register(alias_entries[i].alias, alias_entries[i].target);
}

static void unregister_hotkey_string(const char *hotkey) {
  if (!hotkey)
    return;
  HotkeyBinding binding = hotkey_convert_into_binding((char *)hotkey, NULL,
                                                      NULL, NULL, false, NULL);
  if (binding.valid)
    hotkey_unregister_internal(binding.vk, binding.modifiers);
}

static void hotkey_bind_live(const char *name, const char *hotkey,
                             void (*cb)(void *), void *ud) {
  if (!name || !hotkey || !cb)
    return;
  unregister_hotkey_string(hotkey);
  HotkeyBinding binding =
      hotkey_convert_into_binding((char *)hotkey, NULL, NULL, NULL, false, ud);
  if (binding.valid) {
    binding.hotkey_callback_ud = (HotkeyCallbackUd)cb;
    binding.hold = false;
    binding.should_swallow = NULL;
    hotkey_register_internal(binding);
  }
  config_hotkey_register_bound(name, hotkey, cb, ud);
}

bool config_hotkey_set(const char *name, const char *hotkey, void (*cb)(void *),
                       void *ud) {
  if (!name || !name[0])
    return false;
  if (strlen(name) >= CONFIG_NAME_MAX)
    return false;
  if (!hotkey || !hotkey[0])
    return false;
  if (strlen(hotkey) >= CONFIG_HOTKEY_MAX)
    return false;
  HotkeyKey key;
  if (!hotkey_string_parse(hotkey, &key))
    return false;

  config_lock();
  config_init_locked();
  int displaced = hotkey_index_by_key(hotkey);
  int self = hotkey_index_of(name);
  if (displaced >= 0 && displaced != self) {
    log_warn("sk_config: '%s' overwrites hotkey '%s' previously bound to "
             "'%s'",
             name, hotkey, hotkey_entries[displaced].name);
    hotkey_entries_remove(displaced);
    if (displaced < self)
      self--;
  }

  char old_hotkey[CONFIG_HOTKEY_MAX] = "";
  int i = hotkey_index_of(name);
  if (i >= 0)
    safe_copy(old_hotkey, sizeof(old_hotkey), hotkey_entries[i].hotkey);

  hotkey_entries_set(name, hotkey);
  if (!config_ready() ||
      !config_section_set(config_path, "hotkeys", name, hotkey))
    log_error("sk_config: failed to save %s", config_path);

  if (old_hotkey[0] && !string_equals_ci(old_hotkey, hotkey))
    unregister_hotkey_string(old_hotkey);
  if (cb)
    hotkey_bind_live(name, hotkey, cb, ud);
  config_unlock();
  return true;
}

char *config_hotkey_get(const char *name) {
  if (!name)
    return NULL;
  config_lock();
  config_init_locked();
  int i = hotkey_index_of(name);
  char *out = i >= 0 ? strdup(hotkey_entries[i].hotkey) : NULL;
  config_unlock();
  return out;
}

bool config_hotkey_clear(const char *name) {
  if (!name)
    return false;
  config_lock();
  config_init_locked();
  int i = hotkey_index_of(name);
  if (i < 0) {
    config_unlock();
    return false;
  }
  char hk[CONFIG_HOTKEY_MAX];
  safe_copy(hk, sizeof(hk), hotkey_entries[i].hotkey);
  hotkey_entries_remove(i);
  if (config_ready())
    config_section_clear(config_path, "hotkeys", name);
  unregister_hotkey_string(hk);
  config_unlock();
  return true;
}

bool config_hotkey_bind(const char *name, void (*cb)(void *), void *ud) {
  if (!name || !cb)
    return false;
  config_lock();
  config_init_locked();
  int i = hotkey_index_of(name);
  if (i < 0) {
    config_unlock();
    return false;
  }
  char hk[CONFIG_HOTKEY_MAX];
  safe_copy(hk, sizeof(hk), hotkey_entries[i].hotkey);
  hotkey_bind_live(name, hk, cb, ud);
  config_unlock();
  return true;
}

char *config_hotkey_name_at(int index) {
  return config_entry_value_at(CONFIG_ENTRY_HOTKEY, index);
}

static int config_hotkey_binding_index(const char *key) {
  if (!key)
    return -1;
  for (size_t i = 0; i < config_hotkey_binding_count; i++)
    if (string_equals_ci(config_hotkey_bindings[i].key, key))
      return (int)i;
  return -1;
}

char *config_hotkey_display_name_at(int index) {
  char *key = config_hotkey_name_at(index);
  if (!key)
    return NULL;
  int binding = config_hotkey_binding_index(key);
  free(key);
  if (binding < 0)
    return NULL;
  return strdup(config_hotkey_bindings[binding].display_name);
}

static void config_hotkey_strings_free(char **strings) {
  for (size_t i = 0; i < arrlenu(strings); i++)
    free(strings[i]);
  arrfree(strings);
}

static void config_hotkey_strings_copy(char ***out, const char *const *src) {
  arrfree(*out);
  *out = NULL;
  if (!src)
    return;
  for (int i = 0; src[i]; i++)
    arrput(*out, strdup(src[i]));
}

static void config_hotkey_binding_free(void *ptr) {
  ConfigHotkeyBinding *b = (ConfigHotkeyBinding *)ptr;
  if (!b)
    return;
  free(b->display_name);
  free(b->key);
  free(b->default_hotkey);
  owner_tag_clear(&b->tag);
  config_hotkey_strings_free(b->whitelist);
  config_hotkey_strings_free(b->blacklist);
}

static void config_hotkey_register_bound(const char *name, const char *hotkey,
                                         void (*cb)(void *), void *ud) {
  if (!name || !hotkey || !cb)
    return;
  int existing = config_hotkey_binding_index(name);
  if (existing >= 0) {
    ConfigHotkeyBinding *b = &config_hotkey_bindings[existing];
    b->cb = NULL;
    b->cb_ud = (HotkeyCallbackUd)cb;
    b->ud = ud;
    return;
  }

  ConfigHotkeyBinding b = {0};
  b.display_name = strdup(name);
  b.key = strdup(name);
  b.default_hotkey = strdup(hotkey);
  b.cb_ud = (HotkeyCallbackUd)cb;
  b.ud = ud;
  owner_tag_set_for_func(&b.tag, (void *)(uintptr_t)cb, 0);
  arrput(config_hotkey_bindings, b);
}

static const char **config_hotkey_string_view(char **strings) {
  const char **out = NULL;
  for (size_t i = 0; i < arrlenu(strings); i++)
    arrput(out, strings[i]);
  return out;
}

static bool config_hotkey_binding_bind(ConfigHotkeyBinding *b) {
  if (!b)
    return false;
  char *hotkey = config_hotkey_get(b->key);
  if (!hotkey)
    return false;
  unregister_hotkey_string(hotkey);
  const char **wl = config_hotkey_string_view(b->whitelist);
  const char **bl = config_hotkey_string_view(b->blacklist);
  const char *const *windows = NULL;
  bool whitelist = false;
  resolve_window_list(wl, bl, &windows, &whitelist);
  HotkeyBinding binding = hotkey_convert_into_binding(
      hotkey, b->cb, NULL, windows, whitelist, b->ud);
  if (!binding.valid) {
    arrfree(wl);
    arrfree(bl);
    free(hotkey);
    return false;
  }
  binding.hotkey_callback_ud = b->cb_ud;
  binding.hold = false;
  binding.should_swallow = b->should_swallow;
  bool ok = hotkey_register_internal(binding);
  arrfree(wl);
  arrfree(bl);
  free(hotkey);
  return ok;
}

bool config_hotkey_register(const ConfigHotkeyRegisterSpec *spec) {
  if (!spec || !spec->name || !spec->name[0] || !spec->key || !spec->key[0] ||
      !spec->default_hotkey || !spec->default_hotkey[0] ||
      (!spec->cb && !spec->cb_ud))
    return false;
  HotkeyKey parsed;
  if (!hotkey_string_parse(spec->default_hotkey, &parsed))
    return false;

  int existing = config_hotkey_binding_index(spec->key);
  if (existing >= 0) {
    config_hotkey_binding_free(&config_hotkey_bindings[existing]);
    arrdel(config_hotkey_bindings, existing);
  }

  ConfigHotkeyBinding b = {0};
  b.display_name = strdup(spec->name);
  b.key = strdup(spec->key);
  b.default_hotkey = strdup(spec->default_hotkey);
  config_hotkey_strings_copy(&b.whitelist,
                             (const char *const *)spec->whitelist);
  config_hotkey_strings_copy(&b.blacklist,
                             (const char *const *)spec->blacklist);
  b.cb = spec->cb;
  b.cb_ud = (HotkeyCallbackUd)spec->cb_ud;
  b.ud = spec->ud;
  b.should_swallow = spec->should_swallow;

  void *anchor = (void *)(uintptr_t)spec->cb_ud;
  if (spec->cb)
    anchor = (void *)(uintptr_t)spec->cb;
  else if (spec->cb_ud)
    anchor = (void *)(uintptr_t)spec->cb_ud;
  owner_tag_set_for_func(&b.tag, anchor, 0);

  arrput(config_hotkey_bindings, b);
  int index = (int)config_hotkey_binding_count - 1;

  char *stored = config_hotkey_get(spec->key);
  if (!stored)
    config_hotkey_set(spec->key, spec->default_hotkey, NULL, NULL);
  else
    free(stored);
  return config_hotkey_binding_bind(&config_hotkey_bindings[index]);
}

void config_hotkey_bind_all(void) {
  for (size_t i = 0; i < config_hotkey_binding_count; i++)
    config_hotkey_binding_bind(&config_hotkey_bindings[i]);
}

void config_hotkey_registry_purge_owner(const char *path_a,
                                        const char *path_b) {
  OWNER_PURGE_STRUCT(config_hotkey_bindings, ConfigHotkeyBinding, path_a,
                     path_b, 0, config_hotkey_binding_free);
}

bool config_alias_set(const char *target, const char *alias) {
  if (!target || !target[0])
    return false;
  if (strlen(target) >= CONFIG_TARGET_MAX)
    return false;
  bool clearing = !alias || !alias[0];
  if (!clearing && strlen(alias) >= CONFIG_ALIAS_MAX)
    return false;

  config_lock();
  config_init_locked();
  if (clearing) {
    const char *cur = alias_for(target);
    if (cur)
      alias_remove(cur);
    int i = alias_index_of(target);
    if (i >= 0) {
      alias_entries_remove(i);
      if (config_ready())
        config_section_clear(config_path, "aliases", target);
    }
    config_unlock();
    return true;
  }

  alias_entries_set(target, alias);
  if (!config_ready() ||
      !config_section_set(config_path, "aliases", target, alias))
    log_error("sk_config: failed to save %s", config_path);
  alias_register(alias, target);
  config_unlock();
  return true;
}

char *config_alias_get(const char *target) {
  if (!target)
    return NULL;
  config_lock();
  config_init_locked();
  int i = alias_index_of(target);
  char *out = i >= 0 ? strdup(alias_entries[i].alias) : NULL;
  config_unlock();
  return out;
}

bool config_quicklink_set(const char *name, const char *target) {
  if (!name || !name[0])
    return false;
  config_lock();
  config_init_locked();
  int i = quicklink_index_of(name);
  if (!target || !target[0]) {
    if (i >= 0) {
      quicklink_entries_remove(i);
      if (config_ready())
        config_section_clear(config_path, "quicklinks", name);
    }
    config_unlock();
    return true;
  }
  if (strlen(name) >= CONFIG_NAME_MAX ||
      strlen(target) >= CONFIG_QUICKLINK_TARGET_MAX) {
    config_unlock();
    return false;
  }
  quicklink_entries_set(name, target);
  if (!config_ready() ||
      !config_section_set(config_path, "quicklinks", name, target))
    log_error("sk_config: failed to save %s", config_path);
  config_unlock();
  return true;
}

bool config_feature_shortcut_set(const char *name, const char *target) {
  if (!name || !name[0])
    return false;
  config_lock();
  config_init_locked();
  int i = feature_shortcut_index_of(name);
  if (!target || !target[0]) {
    if (i >= 0) {
      feature_shortcut_entries_remove(i);
      if (config_ready())
        config_section_clear(config_path, "feature_shortcuts", name);
    }
    config_unlock();
    return true;
  }
  if (strlen(name) >= CONFIG_NAME_MAX ||
      strlen(target) >= CONFIG_FEATURE_SHORTCUT_TARGET_MAX) {
    config_unlock();
    return false;
  }
  char feature[128];
  char filter[CONFIG_FEATURE_SHORTCUT_TARGET_MAX];
  if (!feature_shortcut_parse(target, feature, sizeof(feature), filter,
                              sizeof(filter))) {
    config_unlock();
    return false;
  }
  feature_shortcut_entries_set(name, target);
  if (!config_ready() ||
      !config_section_set(config_path, "feature_shortcuts", name, target))
    log_error("sk_config: failed to save %s", config_path);
  config_unlock();
  return true;
}

bool feature_shortcut_run(const char *name) {
  bool should_close = false;
  log_info("feature_shortcut: dispatch start '%s'", name ? name : "(null)");
  int index = feature_shortcut_index_of(name);
  if (index < 0) {
    log_error("feature_shortcut: unknown name '%s'", name ? name : "(null)");
    goto done;
  }
  char feature[128];
  char filter[CONFIG_FEATURE_SHORTCUT_TARGET_MAX];
  if (!feature_shortcut_parse(feature_shortcut_entries[index].target, feature,
                              sizeof(feature), filter, sizeof(filter))) {
    log_error("feature_shortcut: bad target '%s'",
              feature_shortcut_entries[index].target);
    goto done;
  }
  log_info("feature_shortcut: parsed feature='%s' filter='%s'", feature,
           filter);
  const CcEntrySpec *target = NULL;
  for (int i = 0; i < cc_registry_count(); i++) {
    const CcEntrySpec *entry = cc_registry_at(i);
    if (entry && entry->display_name &&
        strcmp(entry->display_name, feature) == 0) {
      target = entry;
      break;
    }
  }
  if (!target || !target->on_action) {
    log_error("feature_shortcut: feature '%s' not found or has no action",
              feature);
    goto done;
  }
  log_info("feature_shortcut: found feature '%s'", feature);

  SkaldContext *ctx = SkaldContext_Get();
  PromptCallbacks base_callbacks =
      ctx ? ctx->window.prompt_callbacks : (PromptCallbacks){0};

  HeadlessDispatch *headless = ctx ? &ctx->window.headless : NULL;
  if (!headless)
    goto done;
  arrfree(headless->filter);
  headless->filter = NULL;
  headless->active = true;
  headless->capturing = true;
  headless->spec_valid = false;
  headless->state = NULL;
  target->on_action();
  headless->capturing = false;
  if (!headless->spec_valid) {
    log_error("feature_shortcut: feature '%s' did not call sk_window", feature);
    goto done;
  }
  log_info("feature_shortcut: captured sk_window spec");

  PromptSpec spec = headless->spec;
  PromptState *live = prompt_popup_get_top_prompt_state();
  log_info("feature_shortcut: live prompt %s", live ? "found" : "missing");
  if (!live || !live->eb) {
    for (const char *p = filter; *p; p++)
      arrput(headless->filter, *p);
    arrput(headless->filter, '\0');
    ctx->window.initial_text_pending = headless->filter;
    spec.subwindow = false;
    headless->active = false;
    headless->capturing = false;
    headless->spec_valid = false;
    headless->state = NULL;
    ctx->window.auto_enter_pending = true;
    WindowManager_ShowPrompt(WindowManager_Get(), &spec);
    ctx->window.auto_enter_pending = false;
    goto done;
  }
  PromptCallbacks cb = feature_shortcut_make_callbacks(&spec, &base_callbacks);
  if (live->callbacks)
    *live->callbacks = cb;
  live->render_data = spec.render_data;
  live->window_title = (char *)spec.title;
  safe_copy(live->eb->filter, sizeof(live->eb->filter), filter);
  live->eb->cursor = strlen(live->eb->filter);
  ctx->window.prompt_callbacks = cb;
  ctx->window.current_feature = (char *)feature;
  ctx->window.title = spec.title;
  headless->state = live;
  if (cb.refresh)
    cb.refresh(live);
  feature_shortcut_select(live->lb, filter);
  log_info("feature_shortcut: live listbox count=%d selected=%d",
           live->lb ? live->lb->item_count : -1,
           live->lb ? live->lb->selected_index : -1);
  if (cb.enter)
    should_close = cb.enter(live);
  log_info("feature_shortcut: enter done close=%d filter='%s'", should_close,
           live->eb->filter);
  headless->state = NULL;
  UI_RequestPopupRepaint();
  log_info("feature_shortcut: installed callbacks into live prompt");
  if (should_close) {
    ctx->window.selection_complete = true;
    if (spec.on_close)
      spec.on_close();
  }

done:
  log_info("feature_shortcut: dispatch end close=%d", should_close);
  if (ctx) {
    arrfree(ctx->window.headless.filter);
    ctx->window.headless.filter = NULL;
    ctx->window.headless.active = false;
    ctx->window.headless.spec_valid = false;
    ctx->window.headless.state = NULL;
  }
  return should_close;
}
