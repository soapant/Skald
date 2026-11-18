#ifndef SETTINGS_REGISTRY_H
#define SETTINGS_REGISTRY_H

typedef struct {
  const char *name;
  const char *icon;
  void (*render)(void *ud);
  void *ud;
} SettingsEntrySpec;

void settings_registry_purge_owner(const char *path_a, const char *path_b);

#endif
