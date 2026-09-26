#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "platform/platform.h"

static void config_resolve_one(const char** var, DefaultAppKind kind) {
    if (!var || (*var && (*var)[0])) return;
    const char* id = Platform_GetDefaultApp(kind);
    if (!id || !id[0]) return;
    size_t n = strlen(id);
    if (n > 8 && strcmp(id + n - 8, ".desktop") == 0) {
        char exe[512];
        if (!Platform_ResolveDesktopExec(id, exe, sizeof(exe)) || !exe[0]) return;
        char* copy = strdup(exe);
        if (copy) *var = copy;
    } else {
        char* copy = strdup(id);
        if (copy) *var = copy;
    }
}

void Config_ResolveDefaultApps(void) {
    config_resolve_one(&UsedBrowser, DEFAULT_BROWSER);
    printf("%s \n",UsedBrowser);
    config_resolve_one(&UsedTerminal, DEFAULT_TERMINAL);
    printf("%s \n",UsedTerminal);
    config_resolve_one(&UsedShell, DEFAULT_SHELL);
    printf("%s \n",UsedShell);
    config_resolve_one(&UsedImageViewer, DEFAULT_IMAGE_VIEWER);
    printf("%s \n",UsedImageViewer);
    config_resolve_one(&UsedVideoPlayer, DEFAULT_VIDEO_PLAYER);
    printf("%s \n",UsedVideoPlayer);
}
