#ifndef FEATURE_LOADER_H
#define FEATURE_LOADER_H

#include <stdbool.h>
#include <stddef.h>

bool FeatureLoader_Init(void);
void FeatureLoader_Shutdown(void);
void FeatureLoader_ManifestAddProvides(const char* owner_path, const char* key);
void FeatureLoader_ManifestAddDepends(const char* owner_path, const char* dep_name);
void FeatureLoader_OwnerFeatureName(const char* owner_path, char* out, size_t n);
void FeatureLoader_Reload(const char* name);
void FeatureLoader_ReloadAll(void);
bool FeatureLoader_LoadAllFromDir(const char* dir);
void FeatureLoader_Unload(const char* name);

void FeatureLoader_Lock(void);
void FeatureLoader_Unlock(void);

#endif