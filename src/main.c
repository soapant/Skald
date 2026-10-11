#define STB_DS_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "core/context.h"
#include "deps/log.h"
#include "config.h"
#include "platform/platform.h"
#include "core/feature_data.h"
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
  PROFILE_START(t_start);
  srand((unsigned)time(NULL));
  if (!Platform_TryAcquireSingleInstance()) {
      fprintf(stderr, "Flint is already running, exiting.\n");
      return 1;
  }
  Platform_InitDataFolder();
  Platform_InitMainThreadContext();
  Platform_FixWorkingDirectory();
  Platform_InitDisplaySafe();

  PROFILE_START(t_cfg);
  Config_Load();
  Config_ResolveDefaultApps();
  PROFILE_END_VAR(t_cfg, "startup:config_load+resolve");

  PROFILE_START(t_svc);
  Platform_StartUniversalTimer();
  Platform_InitializeTrayIcon();
  FlintContext_Get()->feature_history = NULL;
  sh_new_strdup(FlintContext_Get()->feature_history);
  InitFeatureDataSystem();
  History_Init();
  Platform_InitUI();
  PROFILE_END_VAR(t_svc, "startup:services_init");

  PROFILE_START(t_feat);
  FeatureLoader_Init();
  PROFILE_END_VAR(t_feat, "startup:FeatureLoader_Init");

  PROFILE_START(t_wm);
  WindowManager* wm = WindowManager_Create();
  PROFILE_END_VAR(t_wm, "startup:WindowManager_Create");
  if (!wm) {
      log_error("Failed to create WindowManager");
      return 1;
  }

  PROFILE_END_VAR(t_start, "startup:total_before_main_loop");
  log_info("entering main loop");

  WindowManager_Run(wm);
  WindowManager_Destroy(wm);

  FeatureLoader_Shutdown();
  Config_Save();
  Config_Unload();
  Platform_ShutdownUI();
  Platform_CleanupDisplaySafe();

  Platform_CleanupPlatformResources();
  CleanupFeatureDataSystem();
  History_Shutdown();

  FlintContext* _ctx = FlintContext_Get();
  if (_ctx->feature_history) {
    for (ptrdiff_t i = 0; i < shlen(_ctx->feature_history); i++) free_history_list(_ctx->feature_history[i].value);
    shfree(_ctx->feature_history);
    _ctx->feature_history = NULL;
  }
  if (_ctx->history_indices) {
    for (ptrdiff_t i = 0; i < shlen(_ctx->history_indices); i++) free(_ctx->history_indices[i].value);
    shfree(_ctx->history_indices);
    _ctx->history_indices = NULL;
  }
  if (_ctx->item_index_map) {
    shfree(_ctx->item_index_map);
    _ctx->item_index_map = NULL;
  }

  return 0;
}
