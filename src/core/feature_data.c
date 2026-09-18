#include "core/context.h"
#include "core/modal.h"
#include "core/feature_data.h"
#include <stdio.h>

static FlintMap* g_feature_data = NULL;
static const char* g_current_feature_name = NULL;

void InitFeatureDataSystem(void) {
    if (!g_feature_data) {
        g_feature_data = NULL;
        sh_new_strdup(g_feature_data);
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
        for (ptrdiff_t i = 0; i < shlen(g_feature_data); i++) free_feature_data(g_feature_data[i].value);
        shfree(g_feature_data);
        g_feature_data = NULL;
    }
    g_current_feature_name = NULL;
}

struct feature_data* GetFeatureData(const char* name) {
    if (!g_feature_data) {
        InitFeatureDataSystem();
    }
    ptrdiff_t idx = shgeti(g_feature_data, (char*)name);
    struct feature_data* data = idx >= 0 ? (struct feature_data*)g_feature_data[idx].value : NULL;
    if (!data) {
        data = malloc(sizeof(struct feature_data));
        if (data) {
            memset(data, 0, sizeof(struct feature_data));
            data->name = strdup(name);
            shput(g_feature_data, (char*)name, data);
        }
    }
    return data;
}

void SetCurrentFeatureName(const char* name) {
    g_current_feature_name = name;
}
