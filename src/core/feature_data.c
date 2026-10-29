#include "core/context.h"
#include "core/feature_data.h"

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

void SetCurrentFeatureName(const char* name) {
    g_current_feature_name = name;
}
