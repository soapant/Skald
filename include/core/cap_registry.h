#ifndef CORE_CAP_REGISTRY_H
#define CORE_CAP_REGISTRY_H

#include <stdbool.h>
#include <stdint.h>

#define CAP_ABI_V1 1u

#define CAP_MAX 640
#define CAP_KEY_LEN 96

void Cap_RegisterAbi(const char *key, unsigned abi, void *fn);
void *Cap_ResolveAbi(const char *key, unsigned abi);

void Cap_BindDeclare(const char *key, void *consumer);
void Cap_PurgeOwner(const char *path_a, const char *path_b);

unsigned Cap_Generation(void);

#endif
