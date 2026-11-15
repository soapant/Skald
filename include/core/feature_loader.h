#ifndef FEATURE_LOADER_H
#define FEATURE_LOADER_H

#include <stdbool.h>
#include <stddef.h>

bool feature_loader_init(void);
void feature_loader_shutdown(void);
void feature_loader_manifest_add_provides(const char *owner_path,
                                          const char *key);
void feature_loader_manifest_add_depends(const char *owner_path,
                                         const char *dep_name);
void feature_loader_owner_feature_name(const char *owner_path, char *out,
                                       size_t n);
void feature_loader_reload(const char *name);
bool feature_loader_reload_from(const char *name, const char *src_path);
void feature_loader_reload_all(void);
bool feature_loader_load_all_from_dir(const char *dir);

void feature_loader_lock(void);
bool feature_loader_try_lock(void);
void feature_loader_unlock(void);
void feature_loader_sweep_retired(void);
void feature_loader_drain_retired_unloads(void);
int feature_loader_pending_unload_count(void);
int feature_loader_build_stale_sources(bool hot_reload);
void deferred_init_register(void (*cb)(void));
void deferred_init_run_all(void);

#endif
