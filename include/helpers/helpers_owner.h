#ifndef HELPERS_OWNER_H
#define HELPERS_OWNER_H

#include <stdbool.h>
#include <stddef.h>

#define OWNER_PATH_MAX 512

char *owner_strdup(const char *src);
void owner_strfree(char **src);

char *owner_module_basename_alloc(const char *path);
void owner_module_basename(const char *path, char *out, size_t n);
bool owner_module_matches(const char *owner, const char *path_a,
                          const char *path_b);
char *owner_resolve_for_func_alloc(void *fn);
void owner_resolve_for_func(void *fn, char *out, size_t n);
bool owner_purge_inactive(const char *path_a, const char *path_b);

#endif
