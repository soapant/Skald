#include "core/feature_loader.h"
#include "platform/lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "core/timer.h"
#ifndef _WIN32
#include <dlfcn.h>
#else
#include <windows.h>
#endif

#define MAX_FEATURES 64

typedef struct {
    char name[64];
    char path[512];
    LibHandle handle;
} LoadedFeature;

static LoadedFeature g_features[MAX_FEATURES];
static int g_feature_count = 0;

static bool ends_with(const char* str, const char* suffix) {
    if (!str || !suffix) return false;
    size_t sl = strlen(str), su = strlen(suffix);
    if (su > sl) return false;
    return strcmp(str + sl - su, suffix) == 0;
}

static bool load_single(const char* dir, const char* filename) {
    if (g_feature_count >= MAX_FEATURES) return false;
    char full[1024];
    snprintf(full, sizeof(full), "%s/%s", dir, filename);
    char name[64];
    strncpy(name, filename, sizeof(name)-1);
    name[sizeof(name)-1] = '\0';
    char* dot = strrchr(name, '.');
    if (dot) *dot = '\0';

    for (int i=0;i<g_feature_count;i++) if (strcmp(g_features[i].name, name)==0) return true;

    LibHandle h = Platform_LoadLibrary(full);
    if (!h) {
#ifndef _WIN32
        fprintf(stderr, "[FEATURE] dlopen %s failed: %s\n", full, dlerror());
#else
        fprintf(stderr, "[FEATURE] LoadLibrary %s failed\n", full);
#endif
        return false;
    }
    typedef void (*InitFn)(void);
    InitFn init = (InitFn)Platform_GetSymbol(h, "FlintFeatureInit");
    if (!init) init = (InitFn)Platform_GetSymbol(h, "FlintFeatureReload");
    if (init) init();

    LoadedFeature *f = &g_features[g_feature_count++];
    strncpy(f->name, name, sizeof(f->name)-1);
    strncpy(f->path, full, sizeof(f->path)-1);
    f->handle = h;
    printf("[FEATURE] loaded %s from %s\n", f->name, full);
    return true;
}

bool FeatureLoader_LoadAllFromDir(const char* dir) {
    const char* ext = Platform_LibExtension();
    DIR* d = opendir(dir);
    if (!d) return false;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0]=='.') continue;
        if (!ends_with(e->d_name, ext)) continue;
        load_single(dir, e->d_name);
    }
    closedir(d);
    return true;
}

bool FeatureLoader_Init(void) {
    g_feature_count = 0;
    // try build/features and ./features
    if (!FeatureLoader_LoadAllFromDir("build/features")) {
        FeatureLoader_LoadAllFromDir("features");
    } else {
        // also load any extra in features dir if exists
        FeatureLoader_LoadAllFromDir("features");
    }
    printf("[FEATURE] init done, %d features loaded (hot reload dir: build/features)\n", g_feature_count);
    return true;
}

void FeatureLoader_Shutdown(void) {
    for (int i=0;i<g_feature_count;i++) {
        if (g_features[i].handle) {
            // optional shutdown symbol
            typedef void (*ShutdownFn)(void);
            ShutdownFn shut = (ShutdownFn)Platform_GetSymbol(g_features[i].handle, "FlintFeatureShutdown");
            if (shut) shut();
            Platform_UnloadLibrary(g_features[i].handle);
        }
    }
    g_feature_count = 0;
}

void FeatureLoader_Reload(const char* name) {
    if (!name) return FeatureLoader_ReloadAll();
    for (int i=0;i<g_feature_count;i++) {
        if (strcmp(g_features[i].name, name)==0) {
            typedef void (*ShutdownFn)(void);
            ShutdownFn shut = (ShutdownFn)Platform_GetSymbol(g_features[i].handle, "FlintFeatureShutdown");
            if (shut) shut();
            Platform_UnloadLibrary(g_features[i].handle);
#ifdef _WIN32
            Sleep(100);
#endif
            LibHandle h = Platform_LoadLibrary(g_features[i].path);
            if (!h) return;
            typedef void (*InitFn)(void);
            InitFn init = (InitFn)Platform_GetSymbol(h, "FlintFeatureInit");
            if (!init) init = (InitFn)Platform_GetSymbol(h, "FlintFeatureReload");
            if (init) init();
            g_features[i].handle = h;
            printf("[FEATURE] reloaded %s\n", name);
            return;
        }
    }
    char file[128];
    snprintf(file, sizeof(file), "%s%s", name, Platform_LibExtension());
    load_single("build/features", file);
    return;
}

void FeatureLoader_ReloadAll(void) {
    WinTimer timer1 = TimerStart();

    FeatureLoader_Shutdown();
    FeatureLoader_Init();

    TimerEnd(timer1, "Hot Reload");
}

