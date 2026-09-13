#ifndef FEATURE_DATA_H
#define FEATURE_DATA_H

#include "core/types.h"
#include "core/hashmap.h"

struct feature_data* GetFeatureData(const char* name);
void InitFeatureDataSystem(void);
void CleanupFeatureDataSystem(void);
struct feature_data* GetCurrentFeatureData(void);
void SetCurrentFeatureName(const char* name);

#endif
