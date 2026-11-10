#include <stdbool.h>

#include "core/skapi.h"

#if defined(_WIN32)
#include <windows.h>
static void *host_sym(const char *name) {
  HMODULE h = GetModuleHandleA(NULL);
  if (!h)
    return NULL;
  return (void *)GetProcAddress(h, name);
}
#else
#include <dlfcn.h>
static void *host_sym(const char *name) { return dlsym(RTLD_DEFAULT, name); }
#endif

typedef void (*ski_cap_register_fn)(const char *key, void *fn);

static bool reload_probe_cap_fn(void) { return true; }

SKALD_FEATURE_INIT {
  ski_cap_register_fn reg = (ski_cap_register_fn)host_sym("ski_cap_register");
  if (reg)
    reg("test.reload_probe.cap", (void *)&reload_probe_cap_fn);
}
