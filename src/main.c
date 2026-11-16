#define STB_DS_IMPLEMENTATION
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wundef"
#include "deps/stb_ds.h"
#pragma GCC diagnostic pop
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stddef.h>
#include "deps/log.h"
#include "config.h"
#include "platform/platform.h"
#include "core/feature_loader.h"
#include "core/window_manager.h"

int main(void) {
  srand((unsigned)time(NULL));
  if (!Platform_TryAcquireSingleInstance()) {
    log_warn("Skald is already running, exiting");
    return 1;
  }
  Platform_InitDataFolder();
  Platform_InitMainThreadContext();
  Platform_FixWorkingDirectory();
  Platform_InitDisplaySafe();

  config_load();
  config_resolve_default_apps();

  Platform_InitializeTrayIcon();
  Platform_InitUI();
  Platform_PrewarmBackgroundPixmap();

  feature_loader_init();

  WindowManager *wm = WindowManager_Create();

  if (!wm) {
    log_error("Failed to create WindowManager");
    return 1;
  }

  log_info("entering main loop");

  WindowManager_Run(wm);
  WindowManager_Destroy(wm);

  feature_loader_shutdown();
  config_save();
  config_unload();
  Platform_ShutdownUI();
  Platform_CleanupDisplaySafe();

  Platform_CleanupPlatformResources();
  return 0;
}
