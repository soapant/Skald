#ifndef CC_REGISTRY_H
#define CC_REGISTRY_H

#include <stdbool.h>

typedef struct {
  const char *display_name;
  const char *internal_name;
  bool (*on_enter)(void);
  bool (*on_ctrl_enter)(void);
  bool (*on_shift_enter)(void);
  bool (*on_ctrl_shift_enter)(void);
  bool (*on_ctrl_alt_enter)(void);
  bool (*on_shift_alt_enter)(void);
  bool (*on_tab)(void);
  void (*on_action)(void);
  const char *icon;
  const char *type;
} CcEntrySpec;

void cc_registry_purge_owner(const char *path_a, const char *path_b);
void feature_list_purge_owner(const char *path_a, const char *path_b);
int feature_list_find(const char *name, void (*cb)(void));
bool feature_list_add(const char *name, void (*cb)(void), const char *icon,
                      const char *owner);
int feature_list_count(void);
bool feature_list_at(int index, const char **name, void (**cb)(void),
                     const char **icon);
int cc_registry_count(void);
const CcEntrySpec *cc_registry_at(int index);
bool cc_add_from_spec(const CcEntrySpec *spec, void *caller);
bool cc_add_dep(const char *dep_name, const char *owner_file);

#endif
