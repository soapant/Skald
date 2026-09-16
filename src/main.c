#include "core/context.h"
#include "deps/log.h"
#include "core/modal.h"
#include "core/types.h"
#include "config.h"
#include "core/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_flag.h"
#include "helpers/helpers_file.h"
#include "helpers/helpers_ui.h"
#include "core/feature_data.h"
#include "core/flapi.h"
#include "core/feature_loader.h"
#include "core/window_manager.h"
#include "core/history.h"
#include "core/timer.h"

static void free_history_list(void* v) {
  char** list = (char**)v;
  if (!list) return;
  for (int k = 0; list[k]; k++) free(list[k]);
  free(list);
}

int main() {
  if (!Platform_TryAcquireSingleInstance()) {
      return 1;
  }
  Platform_InitMainThreadContext();
  Platform_FixWorkingDirectory();
  Platform_InitDisplaySafe();

  Platform_StartUniversalTimer();
  Platform_InitializeTrayIcon();
  FlintContext_Get()->feature_history = HashMap_Create(32);
  InitFeatureDataSystem();
  History_Init();
  Platform_InitUI();

  FeatureLoader_Init();

  WindowManager* wm = WindowManager_Create();
  if (!wm) {
      log_error("Failed to create WindowManager");
      return 1;
  }

  WindowManager_Run(wm);
  WindowManager_Destroy(wm);

  FeatureLoader_Shutdown();
  Platform_ShutdownUI();
  Platform_CleanupDisplaySafe();

  Platform_CleanupPlatformResources();
  CleanupFeatureDataSystem();
  History_Shutdown();

  FlintContext* _ctx = FlintContext_Get();
  if (_ctx->feature_history) {
    HashMap_DestroyWith(_ctx->feature_history, free_history_list);
    _ctx->feature_history = NULL;
  }
  if (_ctx->history_indices) {
    HashMap_DestroyWith(_ctx->history_indices, free);
    _ctx->history_indices = NULL;
  }
  if (_ctx->g_item_index_map) {
    HashMap_Destroy(_ctx->g_item_index_map);
    _ctx->g_item_index_map = NULL;
  }

  return 0;
}
