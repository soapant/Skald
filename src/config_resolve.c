#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdatomic.h>

#include "config.h"
#include "platform/platform.h"
#include "config_values.h"
#include "helpers/helpers_misc.h"

typedef struct {
  _Atomic(const char *) *var;
  DefaultAppKind kind;
  const char *name;
  bool auto_resolved;
} AutoApp;

#define AUTO_APP_SLOTS 3

typedef struct {
  AutoApp apps[AUTO_APP_SLOTS];
  void *mutex;
} AutoAppRegistry;

static AutoAppRegistry g_auto_apps;

static void auto_apps_init(void) {
  void *mutex = lazy_mutex_get(&g_auto_apps.mutex);
  if (mutex)
    Platform_MutexLock(mutex);
  if (g_auto_apps.apps[0].var) {
    if (mutex)
      Platform_MutexUnlock(mutex);
    return;
  }
  AutoApp *auto_apps = g_auto_apps.apps;
  auto_apps[0].var = &UsedBrowser;
  auto_apps[0].kind = DEFAULT_BROWSER;
  auto_apps[0].name = CONFIG_NAME_STRING(UsedBrowser);
  auto_apps[0].auto_resolved = false;
  auto_apps[1].var = &UsedTerminal;
  auto_apps[1].kind = DEFAULT_TERMINAL;
  auto_apps[1].name = CONFIG_NAME_STRING(UsedTerminal);
  auto_apps[1].auto_resolved = false;
  auto_apps[2].var = &UsedShell;
  auto_apps[2].kind = DEFAULT_SHELL;
  auto_apps[2].name = CONFIG_NAME_STRING(UsedShell);
  auto_apps[2].auto_resolved = false;
  if (mutex)
    Platform_MutexUnlock(mutex);
}

static void config_resolve_one(AutoApp *app) {
  if (!app || !app->var)
    return;
  app->auto_resolved = false;
  const char *current = atomic_load_explicit(app->var, memory_order_acquire);
  if (current && current[0])
    return;
  const char *id = Platform_GetDefaultApp(app->kind);
  if (!id || !id[0])
    return;
  size_t n = strlen(id);
  char *copy = NULL;
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
  if (!config_own_string(app->name, copy)) {
    free(copy);
    return;
  }
  atomic_store_explicit(app->var, copy, memory_order_release);
  app->auto_resolved = true;
}

void config_resolve_default_apps(void) {
  auto_apps_init();
  for (size_t i = 0; i < ARRAY_LEN(g_auto_apps.apps); i++) {
    config_resolve_one(&g_auto_apps.apps[i]);
  }
}

bool config_is_auto_resolved_app(const char *name) {
  auto_apps_init();
  if (!name)
    return false;
  for (size_t i = 0; i < ARRAY_LEN(g_auto_apps.apps); i++) {
    if (!g_auto_apps.apps[i].name)
      continue;
    if (strcmp(g_auto_apps.apps[i].name, name) == 0)
      return g_auto_apps.apps[i].auto_resolved;
  }
  return false;
}
