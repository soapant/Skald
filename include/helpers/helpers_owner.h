#ifndef HELPERS_OWNER_H
#define HELPERS_OWNER_H

#include <stdbool.h>
#include <stddef.h>

#define OWNER_PATH_MAX 512

void owner_module_basename(const char *path, char *out, size_t n);
bool owner_module_matches(const char *owner, const char *path_a,
                         const char *path_b);
void owner_resolve_for_func(void *fn, char *out, size_t n);
bool owner_purge_inactive(const char *path_a, const char *path_b);

#endif
