#ifndef CAP_REGISTRY_H
#define CAP_REGISTRY_H

#include <stdbool.h>
#include <stdint.h>

#define CAP_ABI_V1 1u

#define CAP_MAX 640
#define CAP_KEY_LEN 96

void cap_register_abi(const char *key, unsigned abi, void *fn);
void *cap_resolve_abi(const char *key, unsigned abi);

void cap_bind_declare(const char *key, void *consumer);
void cap_purge_owner(const char *path_a, const char *path_b);

unsigned cap_generation(void);

#endif
