#include "core/context.h"
#include "core/modal.h"
#include "core/feature_data.h"
#include <stdio.h>

static HashMap* g_feature_data = NULL;
static const char* g_current_feature_name = NULL;

void InitFeatureDataSystem(void) {
    if (!g_feature_data) {
        g_feature_data = HashMap_Create(32);
    }
}

static void free_feature_data(void* v) {
    struct feature_data* fd = (struct feature_data*)v;
    if (!fd) return;
    free(fd->name);
    for (int i = 0; i < fd->selection_count; i++) {
        free((void*)fd->last_selections[i]);
    }
    free(fd);
}

void CleanupFeatureDataSystem(void) {
    if (g_feature_data) {
        HashMap_DestroyWith(g_feature_data, free_feature_data);
        g_feature_data = NULL;
    }
    g_current_feature_name = NULL;
}

struct feature_data* GetFeatureData(const char* name) {
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

struct feature_data* GetCurrentFeatureData(void) {
    if (!g_current_feature_name) return NULL;
    return GetFeatureData(g_current_feature_name);
}

void SetCurrentFeatureName(const char* name) {
    g_current_feature_name = name;
}
