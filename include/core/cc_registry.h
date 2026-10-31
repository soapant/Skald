#ifndef CC_REGISTRY_H
#define CC_REGISTRY_H

#include <stdbool.h>

/**
 * @brief Control center entry specification.
 *
 * Fields:
 * - `display_name` — Display name.
 * - `internal_name` — Internal unique name.
 * - `icon` — Icon path.
 */
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

bool add_dep(const char *dep_name, const char *owner_file);
void cc_registry_purge_owner(const char *path_a, const char *path_b);

#endif
