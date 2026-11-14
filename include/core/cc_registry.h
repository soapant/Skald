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

#endif
