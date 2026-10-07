#ifndef HELPERS_OWNER_H
#define HELPERS_OWNER_H

#include <stdbool.h>
#include <stddef.h>

#define OWNER_PATH_MAX 512

void Owner_ModuleBasename(const char* path, char* out, size_t n);
bool Owner_ModuleMatches(const char* owner, const char* path_a, const char* path_b);
void Owner_ResolveForFunc(void* fn, char* out, size_t n);

#endif