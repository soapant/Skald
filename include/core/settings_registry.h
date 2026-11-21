#ifndef SETTINGS_REGISTRY_H
#define SETTINGS_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>

typedef struct SettingsEntry SettingsEntry;

typedef struct {
  const char *name;
  const char *icon;
  const char *desc;
  const char *owner;
  void (*render)(SettingsEntry *entry, void *ud);
  void *ud;
} SettingsEntrySpec;

typedef void (*SettingsEditboxChangeFn)(const char *name, const char *value);

typedef struct {
  const char *name;
  const char *label;
  char *buffer;
  size_t buffer_size;
  int width;
} SettingsEditboxField;

typedef struct {
  const char *title;
  const char *sub;
  const SettingsEditboxField *fields;
  int field_count;
  int x_offset;
  SettingsEditboxChangeFn on_change;
} SettingsEditboxSpec;

typedef struct {
  int entry;
  int block;
} SettingsEditboxGroup;

#define SETTINGS_EDITBOX_INVALID ((SettingsEditboxGroup){-1, -1})
#define SETTINGS_EDITBOX_X_OFFSET_DEFAULT 70

typedef int (*SettingsDropdownOptionsFn)(const char ***out_options);
typedef void (*SettingsDropdownSelectFn)(const char *option, int index,
                                         void *ud);

typedef struct {
  const char *title;
  const char *sub;
  SettingsDropdownOptionsFn refresh_options;
  int initial;
  SettingsDropdownSelectFn on_select;
  void (*on_hover)(void);
  void *ud;
} SettingsDropdownSpec;

typedef struct {
  const char *title;
  const char *sub;
  bool initial;
  void (*on_click)(void);
  void (*on_hover)(void);
} SettingsCheckboxSpec;

typedef struct {
  const char *title;
  const char *sub;
  const char *label;
  void (*on_click)(void);
  void (*on_hover)(void);
} SettingsButtonSpec;

typedef struct {
  const char *title;
  const char *sub;
  float min_value;
  float max_value;
  float initial;
  void (*on_change)(void);
  void (*on_hover)(void);
} SettingsSliderSpec;

SettingsEntry *settings_entry_add(const SettingsEntrySpec *spec);
int settings_entry_add_checkbox(SettingsEntry *entry,
                                const SettingsCheckboxSpec *spec);
int settings_entry_add_button(SettingsEntry *entry,
                              const SettingsButtonSpec *spec);
int settings_entry_add_slider(SettingsEntry *entry,
                              const SettingsSliderSpec *spec);
int settings_entry_add_dropdown(SettingsEntry *entry,
                                const SettingsDropdownSpec *spec);
SettingsEditboxGroup settings_entry_add_editbox(SettingsEntry *entry,
                                                const SettingsEditboxSpec *spec);
const char *settings_editbox_get(SettingsEditboxGroup group, const char *name);
bool settings_editbox_set(SettingsEditboxGroup group, const char *name,
                          const char *value);
int settings_editbox_right_offset(int pane_w, int width);
int settings_entry_count(void);
void settings_scroll_to_block(const char *entry_name, int block_index);
const SettingsEntrySpec *settings_entry_at(int index);

void settings_config_set_notify(void (*notify)(const char *key,
                                               const char *value));
void settings_registry_purge_owner(const char *path_a, const char *path_b);
bool settings_focus_ctx_has(const char *name);
void settings_dispatch_widget_click(void);
void settings_dispatch_slider_change(float value);
void settings_dispatch_focus_click(void);
void settings_dispatch_focus_change(float value);

#endif
