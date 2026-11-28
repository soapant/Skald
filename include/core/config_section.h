#ifndef CONFIG_SECTION_H
#define CONFIG_SECTION_H

#include <stdbool.h>
#include <stddef.h>

#include "core/config_toml.h"
#include "core/types.h"

typedef enum {
  CONFIG_ENTRY_HOTKEY = 0,
  CONFIG_ENTRY_ALIAS,
  CONFIG_ENTRY_QUICKLINK,
  CONFIG_ENTRY_FEATURE_SHORTCUT,
  CONFIG_ENTRY_SETTING
} ConfigEntryKind;

void config_lock(void);
void config_unlock(void);
void config_init_locked(void);

int config_entry_count(ConfigEntryKind kind);
char *config_entry_value_at(ConfigEntryKind kind, int index);
char *config_entry_dup_key_at(ConfigEntryKind kind, int index);
const char *config_entry_key_at(ConfigEntryKind kind, int index);

bool config_setting_set(const char *key, const char *value);
bool config_setting_remove(const char *key);
char *config_setting_get(const char *key);
bool config_setting_has(const char *key);
bool config_setting_get_value(const char *key,
                              void (*parse)(const char *, void *), void *out);
void config_parse_bool_value(const char *text, void *out);
void config_parse_int_value(const char *text, void *out);
void config_parse_float_value(const char *text, void *out);
void config_setting_entries_set(const char *key, const char *value);

bool alias_register(const char *alias, const char *target);
bool alias_remove(const char *alias);
const char *alias_for(const char *target);

bool config_hotkey_set(const char *name, const char *hotkey, void (*cb)(void *),
                       void *ud);
char *config_hotkey_get(const char *name);
bool config_hotkey_clear(const char *name);
bool config_hotkey_bind(const char *name, void (*cb)(void *), void *ud);
char *config_hotkey_name_at(int index);
char *config_hotkey_display_name_at(int index);
bool config_alias_set(const char *target, const char *alias);
char *config_alias_get(const char *target);
bool config_quicklink_set(const char *name, const char *target);
bool config_feature_shortcut_set(const char *name, const char *target);
bool feature_shortcut_run(const char *name);

typedef void (*ConfigHotkeyFn)(void *ud);

bool config_hotkey_register(const ConfigHotkeyRegister *spec);
void config_hotkey_bind_all(void);

bool config_section_set(const char *path, const char *section, const char *name,
                        const char *value);
bool config_section_clear(const char *path, const char *section,
                          const char *name);

typedef struct {
  const char *path;
  const char *section;
  const char *name;
  const char *value;
  ConfigTomlError *err;
} ConfigSectionSetEx;

ConfigTomlStatus config_section_set_ex(ConfigSectionSetEx *argspec);
ConfigTomlStatus config_section_clear_ex(const char *path, const char *section,
                                         const char *name,
                                         ConfigTomlError *err);

#endif
