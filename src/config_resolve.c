#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "platform/platform.h"

typedef struct {
    const char** var;
    DefaultAppKind kind;
    const char* name;
    bool auto_resolved;
} AutoApp;

#define AUTO_APP_SLOTS 3

static AutoApp g_auto_apps[AUTO_APP_SLOTS];
static bool g_auto_apps_ready = false;

static void auto_apps_init(void) {
    if (g_auto_apps_ready) return;
    g_auto_apps_ready = true;
    g_auto_apps[0].var = &UsedBrowser;      g_auto_apps[0].kind = DEFAULT_BROWSER;      g_auto_apps[0].name = "UsedBrowser";      g_auto_apps[0].auto_resolved = false;
    g_auto_apps[1].var = &UsedTerminal;     g_auto_apps[1].kind = DEFAULT_TERMINAL;     g_auto_apps[1].name = "UsedTerminal";     g_auto_apps[1].auto_resolved = false;
    g_auto_apps[2].var = &UsedShell;        g_auto_apps[2].kind = DEFAULT_SHELL;        g_auto_apps[2].name = "UsedShell";        g_auto_apps[2].auto_resolved = false;
}

static void config_resolve_one(AutoApp* app) {
    if (!app || !app->var) return;
    app->auto_resolved = false;
    if ((*app->var) && (*app->var)[0]) return;
    const char* id = Platform_GetDefaultApp(app->kind);
    if (!id || !id[0]) return;
    size_t n = strlen(id);
    char* copy = NULL;
    if (n > 8 && strcmp(id + n - 8, ".desktop") == 0) {
        char exe[512];
        if (!Platform_ResolveDesktopExec(id, exe, sizeof(exe)) || !exe[0]) return;
        copy = strdup(exe);
    } else {
        copy = strdup(id);
    }
    if (!copy) return;
    *app->var = copy;
    app->auto_resolved = true;
}

void Config_ResolveDefaultApps(void) {
    auto_apps_init();
    for (size_t i = 0; i < ARRAY_LEN(g_auto_apps); i++){
      config_resolve_one(&g_auto_apps[i]);
    }
}

bool Config_IsAutoResolvedApp(const char* name) {
    auto_apps_init();
    if (!name) return false;
    for (size_t i = 0; i < ARRAY_LEN(g_auto_apps); i++) {
        if (!g_auto_apps[i].name) continue;
        if (strcmp(g_auto_apps[i].name, name) == 0) return g_auto_apps[i].auto_resolved;
    }
    return false;
}
