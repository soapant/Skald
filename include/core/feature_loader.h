#ifndef FEATURE_LOADER_H
#define FEATURE_LOADER_H

#include <stdbool.h>

bool FeatureLoader_Init(void);
void FeatureLoader_Shutdown(void);
void FeatureLoader_Reload(const char* name);
void FeatureLoader_ReloadAll(void);
bool FeatureLoader_LoadAllFromDir(const char* dir);
void FeatureLoader_Unload(const char* name);

bool Config_Load(void);
bool Config_Reload(void);
void Config_Unload(void);

#endif