#ifndef FEATURE_LOADER_H
#define FEATURE_LOADER_H

#include <stdbool.h>

bool FeatureLoader_Init(void);
void FeatureLoader_Shutdown(void);
bool FeatureLoader_Reload(const char* name);
bool FeatureLoader_ReloadAll(void);
// control for you to tinker - call to trigger reload of single/all
bool FeatureLoader_LoadAllFromDir(const char* dir);

#endif
