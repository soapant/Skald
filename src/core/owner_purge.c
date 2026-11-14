#include "core/owner_purge.h"

#include "core/cap_registry.h"
#include "core/cc_registry.h"
#include "core/hover_widget.h"
#include "core/snippet_manager.h"
#include "core/widget_registry.h"
#include "core/window_manager.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_input_state.h"
#include "helpers/helpers_key.h"
#include "platform/platform.h"

void PurgeOwnerSet(const char *path_a, const char *path_b,
                   unsigned widget_gen) {
  Cap_PurgeOwner(path_a, path_b);
  Hotkey_PurgeOwner(path_a, path_b);
  RunEvery_PurgeOwner(path_a, path_b);
  Snippet_PurgeOwner(path_a, path_b);
  Widget_PurgeOwner(path_a, path_b, widget_gen);
  CcRegistry_PurgeOwner(path_a, path_b);
  FeatureList_PurgeOwner(path_a, path_b);
  KeyCallback_PurgeOwner(path_a, path_b);
  Key_PurgeInterceptorOwner(path_a, path_b);
  EventBus_PurgeOwner(path_a, path_b);
  HoverWidget_PurgeOwner(path_a, path_b);
  Platform_MessagePopup_PurgeOwner(path_a, path_b);
}
