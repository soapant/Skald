#ifndef THREAD_REGISTRY_H
#define THREAD_REGISTRY_H

#include <stdbool.h>

void thread_registry_add(void *ctx, const char *owner_path);
void thread_registry_finish(const void *ctx);
bool thread_registry_owner_active(const char *owner_path);

#endif
