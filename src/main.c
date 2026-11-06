#define STB_DS_IMPLEMENTATION
#include "deps/stb_ds.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stddef.h>

#include "core/context.h"
#include "deps/log.h"
#include "config.h"
#include "platform/platform.h"
#include "core/feature_loader.h"
#include "core/window_manager.h"

int main() {
  srand((unsigned)time(NULL));
  if (!Platform_TryAcquireSingleInstance()) {
    fprintf(stderr, "Flint is already running, exiting.\n");
    return 1;
  }
  Platform_InitDataFolder();
  Platform_InitMainThreadContext();
  Platform_FixWorkingDirectory();
  Platform_InitDisplaySafe();

  Config_Load();
  Config_ResolveDefaultApps();

  Platform_StartUniversalTimer();
  Platform_InitializeTrayIcon();
  Platform_InitUI();
  Platform_PrewarmBackgroundPixmap();

  FeatureLoader_Init();

  WindowManager *wm = WindowManager_Create();

  if (!wm) {
    log_error("Failed to create WindowManager");
    return 1;
  }

  log_info("entering main loop");

  WindowManager_Run(wm);
  WindowManager_Destroy(wm);

  FeatureLoader_Shutdown();
  Config_Save();
  Config_Unload();
  Platform_ShutdownUI();
  Platform_CleanupDisplaySafe();

  Platform_CleanupPlatformResources();

  return 0;
}
