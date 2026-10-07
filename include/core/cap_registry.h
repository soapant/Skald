#ifndef CORE_CAP_REGISTRY_H
#define CORE_CAP_REGISTRY_H

#include <stdbool.h>
#include <stdint.h>

#define FL_CAP_ABI_V1 1u

#define FL_CAP_MAX 64
#define FL_CAP_KEY_LEN 96

void  fl_cap_register_abi(const char *key, unsigned abi, void *fn);
void *fl_cap_resolve_abi(const char *key, unsigned abi);

void  fl_cap_register(const char *key, void *fn);
void *fl_cap_resolve(const char *key);
void  fl_cap_purge_owner(const char *path_a, const char *path_b);
void  fl_cap_dump(void);

unsigned fl_cap_generation(void);

#endif
