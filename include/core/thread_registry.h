#ifndef THREAD_REGISTRY_H
#define THREAD_REGISTRY_H

#include <stdbool.h>

void ThreadRegistry_Add(void *ctx, const char *owner_path);
void ThreadRegistry_Finish(const void *ctx);
bool ThreadRegistry_OwnerActive(const char *owner_path);

#endif
