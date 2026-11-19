#ifndef SETTINGS_REGISTRY_H
#define SETTINGS_REGISTRY_H

#include <stdbool.h>

typedef struct SettingsEntry SettingsEntry;

typedef struct {
  const char *name;
  const char *icon;
  const char *desc;
  void (*render)(SettingsEntry *entry, void *ud);
  void *ud;
} SettingsEntrySpec;

void settings_registry_purge_owner(const char *path_a, const char *path_b);

#endif
