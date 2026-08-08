#ifndef FEATURE_DATA_H
#define FEATURE_DATA_H

#include "core/types.h"
#include "core/hashmap.h"

struct feature_data* GetFeatureData(const char* name);
void InitFeatureDataSystem(void);
void CleanupFeatureDataSystem(void);
void AddSelectionToData(char* selection, const char* feature_name, struct feature_data* data);
struct feature_data* GetCurrentFeatureData(void);
void SetCurrentFeatureName(const char* name);

#endif
