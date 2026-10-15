#ifndef FEATURE_DATA_H
#define FEATURE_DATA_H

#include "core/types.h"

struct feature_data* GetFeatureData(const char* name);
void InitFeatureDataSystem(void);
void CleanupFeatureDataSystem(void);
void SetCurrentFeatureName(const char* name);

#endif
