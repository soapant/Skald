#include <string.h>
#include <stdbool.h>

#include "config.h"
#include "platform/platform.h"
#include "config_values.h"

typedef struct {
  const char **var;
  DefaultAppKind kind;
  const char *name;
  bool auto_resolved;
} AutoApp;

#define AUTO_APP_SLOTS 3

static AutoApp auto_apps[AUTO_APP_SLOTS];
static bool auto_apps_ready = false;

static void auto_apps_init(void) {
  if (auto_apps_ready)
    return;
  auto_apps_ready = true;
  auto_apps[0].var = &UsedBrowser;
  auto_apps[0].kind = DEFAULT_BROWSER;
  auto_apps[0].name = "UsedBrowser";
  auto_apps[0].auto_resolved = false;
  auto_apps[1].var = &UsedTerminal;
  auto_apps[1].kind = DEFAULT_TERMINAL;
  auto_apps[1].name = "UsedTerminal";
  auto_apps[1].auto_resolved = false;
  auto_apps[2].var = &UsedShell;
  auto_apps[2].kind = DEFAULT_SHELL;
  auto_apps[2].name = "UsedShell";
  auto_apps[2].auto_resolved = false;
}

static void config_resolve_one(AutoApp *app) {
  if (!app || !app->var)
    return;
  app->auto_resolved = false;
  if ((*app->var) && (*app->var)[0])
    return;
  const char *id = Platform_GetDefaultApp(app->kind);
  if (!id || !id[0])
    return;
  size_t n = strlen(id);
  const char *copy = NULL;
  if (n > 8 && strcmp(id + n - 8, ".desktop") == 0) {
    char exe[512];
    if (!Platform_ResolveDesktopExec(id, exe, sizeof(exe)) || !exe[0])
      return;
    copy = strdup(exe);
  } else {
    copy = strdup(id);
  }
  if (!copy)
    return;
  *app->var = copy;
  app->auto_resolved = true;
}

void config_resolve_default_apps(void) {
  auto_apps_init();
  for (size_t i = 0; i < ARRAY_LEN(auto_apps); i++) {
    config_resolve_one(&auto_apps[i]);
  }
}

bool config_is_auto_resolved_app(const char *name) {
  auto_apps_init();
  if (!name)
    return false;
  for (size_t i = 0; i < ARRAY_LEN(auto_apps); i++) {
    if (!auto_apps[i].name)
      continue;
    if (strcmp(auto_apps[i].name, name) == 0)
      return auto_apps[i].auto_resolved;
  }
  return false;
}
