#ifndef CORE_CAP_REGISTRY_H
#define CORE_CAP_REGISTRY_H

#include <stdbool.h>
#include <stdint.h>

/* Cross-feature capabilities.
 *
 * A feature publishes a namespaced function pointer here during its
 * FLINT_FEATURE_INIT; consumers resolve lazily by key. This replaces the
 * "define a fl_ function in a feature .so and call it from another feature"
 * pattern, which cannot work: features are dlopen'd RTLD_LOCAL
 * (src/platform/x11/lib_x11.c:21), so a feature's symbols never enter the
 * global namespace and a consumer gets an undefined symbol at dlopen time.
 *
 * Entries carry the owning module path (derived from the function address via
 * dladdr) so they can be dropped when that module is hot-reloaded or
 * unloaded. Consumers must therefore re-resolve rather than cache forever.
 */

#define FL_CAP_ABI_V1 1u

#define FL_CAP_MAX 64
#define FL_CAP_KEY_LEN 96

void  fl_cap_register_abi(const char *key, unsigned abi, void *fn);
void *fl_cap_resolve_abi(const char *key, unsigned abi);

/* Common case: every capability currently speaks ABI v1, so you can omit it.
   Use the _abi forms only when a specific capability needs a version bump. */
void  fl_cap_register(const char *key, void *fn);
void *fl_cap_resolve(const char *key);
void  fl_cap_purge_owner(const char *path_a, const char *path_b);
void  fl_cap_dump(void);

/* Bumped on every register and every purge. A consumer that caches a resolved
 * pointer can compare against this to notice its provider was hot-reloaded or
 * unloaded, instead of calling into a freed .so. */
unsigned fl_cap_generation(void);

#endif