#ifndef OWNER_PURGE_H
#define OWNER_PURGE_H

#include <stdbool.h>

typedef void (*OwnerPurgeHook)(const char *path_a, const char *path_b,
                               unsigned widget_gen);

void run_every_purge_owner(const char *path_a, const char *path_b);
void config_hotkey_registry_purge_owner(const char *path_a, const char *path_b);
void purge_owner_set(const char *path_a, const char *path_b,
                     unsigned widget_gen);

bool owner_purge_register(OwnerPurgeHook hook);

#define OWNER_PURGE_REGISTER2(fn)                                              \
  static void op2_##fn(const char *a, const char *b, unsigned g) {             \
    (void)g;                                                                   \
    fn(a, b);                                                                  \
  }                                                                            \
  __attribute__((constructor)) static void opreg_##fn(void) {                  \
    owner_purge_register(op2_##fn);                                            \
  }

#define OWNER_PURGE_REGISTER3(fn)                                              \
  __attribute__((constructor)) static void opreg_##fn(void) {                  \
    owner_purge_register(fn);                                                  \
  }

#endif
