#include "core/feature_loader.h"
#include "deps/log.h"
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

#define MAX_FEATURES 130

typedef struct {
    char name[64];
    char path[512];
    LibHandle handle;
} LoadedFeature;

static LoadedFeature g_features[MAX_FEATURES];
static int g_feature_count = 0;

#if !defined(_WIN32)
#include <pthread.h>
static pthread_mutex_t g_fl_mutex = PTHREAD_MUTEX_INITIALIZER;
#define FL_LOCK() pthread_mutex_lock(&g_fl_mutex)
#define FL_UNLOCK() pthread_mutex_unlock(&g_fl_mutex)
#else
static CRITICAL_SECTION g_fl_cs;
static bool g_fl_cs_inited = false;
static void fl_lock_init(void) { if (!g_fl_cs_inited) { InitializeCriticalSection(&g_fl_cs); g_fl_cs_inited = true; } }
#define FL_LOCK() do { fl_lock_init(); EnterCriticalSection(&g_fl_cs); } while(0)
#define FL_UNLOCK() LeaveCriticalSection(&g_fl_cs)
#endif

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
        log_error("dlopen %s failed: %s", full, dlerror());
#else
        log_error("LoadLibrary %s failed", full);
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
    return true;
}

bool FeatureLoader_LoadAllFromDir(const char* dir) {
    FL_LOCK();
    const char* ext = Platform_LibExtension();
    DIR* d = opendir(dir);
    if (!d) { FL_UNLOCK(); return false; }
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0]=='.') continue;
        if (!ends_with(e->d_name, ext)) continue;
        load_single(dir, e->d_name);
    }
    closedir(d);
    FL_UNLOCK();
log_info("FeatureLoader_LoadAllFromDir completed");
    return true;
}

bool FeatureLoader_Init(void) {
    FL_LOCK();
    g_feature_count = 0;
    FL_UNLOCK();
    bool has_build = FeatureLoader_LoadAllFromDir("build/features");
    if (!has_build) {
        FeatureLoader_LoadAllFromDir("features");
    } else {
        FeatureLoader_LoadAllFromDir("features");
    }
    FL_LOCK();
    int cnt = g_feature_count;
    FL_UNLOCK();
    log_info("init done, %d features loaded (hot reload dir: build/features)", cnt);
log_info("FeatureLoader_Init completed");
    return true;
}

void FeatureLoader_Shutdown(void) {
    FL_LOCK();
    for (int i=0;i<g_feature_count;i++) {
        if (g_features[i].handle) {
            typedef void (*ShutdownFn)(void);
            ShutdownFn shut = (ShutdownFn)Platform_GetSymbol(g_features[i].handle, "FlintFeatureShutdown");
            if (shut) shut();
            Platform_UnloadLibrary(g_features[i].handle);
        }
    }
    g_feature_count = 0;
    FL_UNLOCK();
log_info("FeatureLoader_Shutdown completed");
}

void FeatureLoader_Reload(const char* name) {
    if (!name) return FeatureLoader_ReloadAll();
    FL_LOCK();
    for (int i=0;i<g_feature_count;i++) {
        if (strcmp(g_features[i].name, name)==0) {
            char path_copy[512];
            strncpy(path_copy, g_features[i].path, sizeof(path_copy)-1);
            path_copy[sizeof(path_copy)-1]='\0';
            LibHandle old = g_features[i].handle;
            LibHandle h = Platform_LoadLibrary(path_copy);
            if (!h) {
#ifndef _WIN32
                log_error("reload %s failed: %s (keeping old)", name, dlerror());
#else
                log_error("reload %s failed (keeping old)", name);
#endif
                FL_UNLOCK(); return;
            }
            typedef void (*InitFn)(void);
            InitFn init = (InitFn)Platform_GetSymbol(h, "FlintFeatureInit");
            if (!init) init = (InitFn)Platform_GetSymbol(h, "FlintFeatureReload");
            if (!init) {
                log_error("reload %s: no init symbol, keeping old", name);
                Platform_UnloadLibrary(h);
                FL_UNLOCK(); return;
            }
            typedef void (*ShutdownFn)(void);
            ShutdownFn shut = (ShutdownFn)Platform_GetSymbol(old, "FlintFeatureShutdown");
            if (shut) shut();
            Platform_UnloadLibrary(old);
#ifdef _WIN32
            FL_UNLOCK();
            Sleep(100);
            FL_LOCK();
            if (i >= g_feature_count || strcmp(g_features[i].name, name)!=0) {
                Platform_UnloadLibrary(h);
                FL_UNLOCK();
                char file2[MaxFeatures];
                snprintf(file2, sizeof(file2), "%s%s", name, Platform_LibExtension());
                FL_LOCK();
                bool ok = load_single("build/features", file2);
                FL_UNLOCK();
                (void)ok;
                return;
            }
#endif
            init();
            g_features[i].handle = h;
            strncpy(g_features[i].path, path_copy, sizeof(g_features[i].path)-1);
            log_info("reloaded %s", name);
            FL_UNLOCK();
            return;
        }
    }
    char file[MaxFeatures];
    snprintf(file, sizeof(file), "%s%s", name, Platform_LibExtension());
    bool ok = load_single("build/features", file);
    (void)ok;
    FL_UNLOCK();
log_info("FeatureLoader_Reload completed");
    return;
}

void FeatureLoader_Unload(const char* name) {
    if (!name) return;
    FL_LOCK();
    for (int i=0;i<g_feature_count;i++) {
        if (strcmp(g_features[i].name, name)==0) {
            typedef void (*ShutdownFn)(void);
            ShutdownFn shut = (ShutdownFn)Platform_GetSymbol(g_features[i].handle, "FlintFeatureShutdown");
            if (shut) shut();
            Platform_UnloadLibrary(g_features[i].handle);
            for (int k=i;k+1<g_feature_count;k++) g_features[k]=g_features[k+1];
            g_feature_count--;
            log_info("unloaded %s", name);
            FL_UNLOCK();
            return;
        }
    }
    FL_UNLOCK();
log_info("FeatureLoader_Unload completed");
}

void FeatureLoader_ReloadAll(void) {
    PROFILE_START(timer1);

    FeatureLoader_Shutdown();
    FeatureLoader_Init();

        log_info("FeatureLoader_ReloadAll completed");
PROFILE_END_VAR(timer1, "Hot Reload");
}