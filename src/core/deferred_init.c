#include "core/deferred_init.h"

#include <stddef.h>

#define DEFERRED_INIT_MAX 32
static void (*g_deferred_inits[DEFERRED_INIT_MAX])(void);
static int g_deferred_count = 0;

void DeferredInit_Register(void (*cb)(void)) {
  if (!cb || g_deferred_count >= DEFERRED_INIT_MAX)
    return;
  g_deferred_inits[g_deferred_count++] = cb;
}

void DeferredInit_RunAll(void) {
  for (int i = 0; i < g_deferred_count; i++) {
    void (*cb)(void) = g_deferred_inits[i];
    g_deferred_inits[i] = NULL;
    cb();
  }
  g_deferred_count = 0;
}
