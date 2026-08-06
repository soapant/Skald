#ifndef FEATURE_DATA_H
#define FEATURE_DATA_H

#include "types.h"
#include "hashmap.h"
#include <stdio.h>

static HashMap* g_feature_data = NULL;
static const char* g_current_feature_name = NULL;

static inline void InitFeatureDataSystem(void) {
    if (!g_feature_data) {
        g_feature_data = HashMap_Create(32);
    }
}

static inline void CleanupFeatureDataSystem(void) {
    if (g_feature_data) {
        HashMap_Destroy(g_feature_data);
        g_feature_data = NULL;
    }
    g_current_feature_name = NULL;
}

static inline struct feature_data* GetFeatureData(const char* name) {
    if (!g_feature_data) {
        InitFeatureDataSystem();
    }

    struct feature_data* data = (struct feature_data*)HashMap_Get(g_feature_data, name);

    if (!data) {
        data = malloc(sizeof(struct feature_data));
        if (data) {
            memset(data, 0, sizeof(struct feature_data));
            data->name = _strdup(name);
            HashMap_Put(g_feature_data, name, data);
        }
    }
    return data;
}

static inline void AddSelectionToData(char* selection, const char* feature_name, struct feature_data* data) {
    if (!data) return;
    printf("ADD SELECTION TO DATA OF SELECTION %s FEATURE %s\n", selection, feature_name);
    data->last_selections[data->selection_count] = selection;
    data->selection_count++;
    for (int i = 0; i < data->selection_count; i++) {
        printf("ADDED STRING %s TO SELECTIONDATA\n", data->last_selections[i]);
    }
}

static inline struct feature_data* GetCurrentFeatureData(void) {
    if (!g_current_feature_name) return NULL;
    return GetFeatureData(g_current_feature_name);
}

static inline const char* GetCurrentFeatureName(void) {
    return g_current_feature_name;
}

static inline void SetCurrentFeatureName(const char* name) {
    g_current_feature_name = name;
}

#endif
