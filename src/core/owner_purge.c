#include "core/owner_purge.h"

#include "core/cap_registry.h"
#include "core/cc_registry.h"
#include "core/settings_registry.h"
#include "core/hover_widget.h"
#include "core/snippet_manager.h"
#include "core/widget_registry.h"
#include "core/window_manager.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_input_state.h"
#include "helpers/helpers_key.h"
#include "platform/platform.h"

void purge_owner_set(const char *path_a, const char *path_b,
                     unsigned widget_gen) {
  cap_purge_owner(path_a, path_b);
  hotkey_purge_owner(path_a, path_b);
  run_every_purge_owner(path_a, path_b);
  snippet_purge_owner(path_a, path_b);
  widget_purge_owner(path_a, path_b, widget_gen);
  cc_registry_purge_owner(path_a, path_b);
  settings_registry_purge_owner(path_a, path_b);
  feature_list_purge_owner(path_a, path_b);
  key_callback_purge_owner(path_a, path_b);
  key_purge_interceptor_owner(path_a, path_b);
  event_bus_purge_owner(path_a, path_b);
  hover_widget_purge_owner(path_a, path_b);
  Platform_MessagePopup_PurgeOwner(path_a, path_b);
}
