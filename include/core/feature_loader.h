#ifndef FEATURE_LOADER_H
#define FEATURE_LOADER_H

#include <stdbool.h>

bool FeatureLoader_Init(void);
void FeatureLoader_Shutdown(void);
void FeatureLoader_Reload(const char* name);
void FeatureLoader_ReloadAll(void);
bool FeatureLoader_LoadAllFromDir(const char* dir);

#endif