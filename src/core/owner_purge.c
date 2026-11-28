#include "core/owner_purge.h"

#include "platform/platform.h"
#include "helpers/helpers_owner.h"

#define OWNER_PURGE_MAX_HOOKS 64
static OwnerPurgeHook owner_purge_hooks[OWNER_PURGE_MAX_HOOKS];
static int owner_purge_hook_count;

bool owner_purge_register(OwnerPurgeHook hook) {
  if (!hook || owner_purge_hook_count >= OWNER_PURGE_MAX_HOOKS)
    return false;
  owner_purge_hooks[owner_purge_hook_count++] = hook;
  return true;
}

void purge_owner_set(const char *path_a, const char *path_b,
                     unsigned widget_gen) {
  if (owner_purge_inactive(path_a, path_b))
    return;
  for (int i = 0; i < owner_purge_hook_count; i++)
    owner_purge_hooks[i](path_a, path_b, widget_gen);
  Platform_MessagePopup_PurgeOwner(path_a, path_b);
}
