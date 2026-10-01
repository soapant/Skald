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
#include "config.h"
#include "core/window_manager.h"
#include "core/widget_registry.h"
#ifdef _WIN32
#include <windows.h>
#endif

#define MAX_FEATURES 130
#define MAX_RETIRED 64
#define MAX_PINS 16

typedef struct {
    char name[64];
    char path[512];
    char uniq[512];
    LibHandle handle;
} LoadedFeature;

static int g_reload_seq = 0;

static bool copy_file(const char* src, const char* dst) {
    FILE* in = fopen(src, "rb");
    if (!in) return false;
    FILE* out = fopen(dst, "wb");
    if (!out) { fclose(in); return false; }
    char buf[8192];
    size_t n;
    bool ok = true;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { ok = false; break; }
    }
    if (ferror(in)) ok = false;
    fclose(in);
    if (fclose(out) != 0) ok = false;
    if (!ok) remove(dst);
    return ok;
}

static LoadedFeature g_features[MAX_FEATURES];
static int g_feature_count = 0;

typedef struct {
    LibHandle handle;
    char uniq[512];
    char owner[512];
    unsigned owner_gen;
    Popup* pins[MAX_PINS];
    int npins;
} RetiredLib;
static RetiredLib g_retired[MAX_RETIRED];
static int g_retired_count = 0;

static void retire_old_lib(LibHandle handle, const char* uniq, const char* owner) {
    if (!handle) return;
    WindowManager* wm = WindowManager_Get();
    RetiredLib r;
    r.handle = handle;
    r.uniq[0] = '\0';
    if (uniq && uniq[0]) {
        strncpy(r.uniq, uniq, sizeof(r.uniq) - 1);
        r.uniq[sizeof(r.uniq) - 1] = '\0';
    }
    r.owner_gen = fl_widget_current_generation();
    r.owner[0] = '\0';
    if (owner && owner[0]) {
        strncpy(r.owner, owner, sizeof(r.owner) - 1);
        r.owner[sizeof(r.owner) - 1] = '\0';
    }
    r.npins = 0;
    if (wm && wm->popup_count > MAX_PINS) {
        r.npins = -1;
    } else if (wm) {
        for (int i = 0; i < wm->popup_count; i++) r.pins[r.npins++] = wm->popups[i];
    }
    if (g_retired_count >= MAX_RETIRED) {
        log_error("retired lib overflow, force unloading oldest");
        Platform_UnloadLibrary(g_retired[0].handle);
        if (g_retired[0].uniq[0]) remove(g_retired[0].uniq);
        for (int k = 0; k + 1 < g_retired_count; k++) g_retired[k] = g_retired[k + 1];
        g_retired_count--;
    }
    g_retired[g_retired_count++] = r;
}

static void sweep_retired_libs(void) {
    WindowManager* wm = WindowManager_Get();
    int n = 0;
    for (int i = 0; i < g_retired_count; i++) {
        bool alive = false;
        if (g_retired[i].owner[0] && fl_widget_owner_active(g_retired[i].owner)) {
            alive = true;
        } else if (wm && g_retired[i].npins >= 0) {
            for (int a = 0; a < g_retired[i].npins && !alive; a++) {
                for (int b = 0; b < wm->popup_count; b++) {
                    if (wm->popups[b] == g_retired[i].pins[a]) { alive = true; break; }
                }
            }
        } else if (wm && wm->popup_count > 0) {
            alive = true;
        }
        if (alive) {
            g_retired[n++] = g_retired[i];
        } else {
            Platform_UnloadLibrary(g_retired[i].handle);
            if (g_retired[i].uniq[0]) remove(g_retired[i].uniq);
        }
    }
    g_retired_count = n;
}

#if !defined(_WIN32)
#include <pthread.h>
static pthread_mutex_t g_fl_mutex;
static pthread_once_t g_fl_once = PTHREAD_ONCE_INIT;
static void fl_mutex_init(void) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&g_fl_mutex, &attr);
    pthread_mutexattr_destroy(&attr);
}
#define FL_LOCK() do { pthread_once(&g_fl_once, fl_mutex_init); pthread_mutex_lock(&g_fl_mutex); } while (0)
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

static double g_feature_init_us = 0.0;
static int g_feature_init_count = 0;

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
        log_error("dlopen %s failed: %s", full, Platform_GetLoadError());
        return false;
    }
    WinTimer t_load = TimerStart();
    typedef void (*InitFn)(void);
    InitFn init = (InitFn)Platform_GetSymbol(h, "FlintFeatureInit");
    if (!init) init = (InitFn)Platform_GetSymbol(h, "FlintFeatureReload");
    if (init) {
        init();
        char label[96];
        snprintf(label, sizeof(label), "feature_init: %s", name);
        TimerEndAlways(t_load, label);
        g_feature_init_us += TimerElapsedUs(t_load);
        g_feature_init_count++;
    } else {
        log_warn("feature %s has no FlintFeatureInit/Reload symbol", name);
    }

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
    g_feature_init_us = 0.0;
    g_feature_init_count = 0;
    FeatureLoader_LoadAllFromDir("build/features");
    FL_LOCK();
    int cnt = g_feature_count;
    FL_UNLOCK();
    log_info("init done, %d features loaded (hot reload dir: build/features)", cnt);
    if (DoTimeDebugs) {
        char label[96];
        snprintf(label, sizeof(label), "feature_init: TOTAL (%d features)", g_feature_init_count);
        TimerPrintRaw(g_feature_init_us, label);
    }
    return true;
}

void FeatureLoader_Shutdown(void) {
    FL_LOCK();
    for (int i=0;i<g_feature_count;i++) {
        if (g_features[i].handle) {
            typedef void (*ShutdownFn)(void);
            ShutdownFn shut = (ShutdownFn)Platform_GetSymbol(g_features[i].handle, "FlintFeatureShutdown");
            if (shut) shut();
            fl_widget_purge_owner(NULL, g_features[i].path, fl_widget_bump_generation());
            Platform_UnloadLibrary(g_features[i].handle);
            if (g_features[i].uniq[0]) remove(g_features[i].uniq);
        }
    }
    for (int i=0;i<g_retired_count;i++) {
        Platform_UnloadLibrary(g_retired[i].handle);
        if (g_retired[i].uniq[0]) remove(g_retired[i].uniq);
    }
    g_retired_count = 0;
    g_feature_count = 0;
    FL_UNLOCK();
log_info("FeatureLoader_Shutdown completed");
}

void FeatureLoader_Reload(const char* name) {
    if (!name) return FeatureLoader_ReloadAll();
    FL_LOCK();
    sweep_retired_libs();
    for (int i=0;i<g_feature_count;i++) {
        if (strcmp(g_features[i].name, name)==0) {
            char path_copy[512];
            strncpy(path_copy, g_features[i].path, sizeof(path_copy)-1);
            path_copy[sizeof(path_copy)-1]='\0';
            LibHandle old = g_features[i].handle;
            char uniquepath[1024];
            snprintf(uniquepath, sizeof(uniquepath), "%s.hot%d", path_copy, ++g_reload_seq);
            if (!copy_file(path_copy, uniquepath)) {
                log_error("reload %s: copy failed (keeping old)", name);
                FL_UNLOCK(); return;
            }
            LibHandle h = Platform_LoadLibrary(uniquepath);
            if (!h) {
                log_error("reload %s failed: %s (keeping old)", name, Platform_GetLoadError());
                remove(uniquepath);
                FL_UNLOCK(); return;
            }
            typedef void (*InitFn)(void);
            InitFn init = (InitFn)Platform_GetSymbol(h, "FlintFeatureInit");
            if (!init) init = (InitFn)Platform_GetSymbol(h, "FlintFeatureReload");
            if (!init) {
                log_error("reload %s: no init symbol, keeping old", name);
                Platform_UnloadLibrary(h);
                remove(uniquepath);
                FL_UNLOCK(); return;
            }
            typedef void (*ShutdownFn)(void);
            ShutdownFn shut = (ShutdownFn)Platform_GetSymbol(old, "FlintFeatureShutdown");
            if (shut) shut();
            unsigned retire_gen = fl_widget_current_generation();
            char retire_owner[512];
            retire_owner[0] = '\0';
            if (g_features[i].uniq[0]) {
                strncpy(retire_owner, g_features[i].uniq, sizeof(retire_owner)-1);
            } else {
                strncpy(retire_owner, g_features[i].path, sizeof(retire_owner)-1);
            }
            retire_owner[sizeof(retire_owner)-1] = '\0';
            retire_old_lib(old, g_features[i].uniq, retire_owner);
#ifdef _WIN32
            FL_UNLOCK();
            Sleep(100);
            FL_LOCK();
            if (i >= g_feature_count || strcmp(g_features[i].name, name)!=0) {
                Platform_UnloadLibrary(h);
                remove(uniquepath);
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
            fl_widget_bump_generation();
            WinTimer t_reload = TimerStart();
            init();
            {
                char label[96];
                snprintf(label, sizeof(label), "feature_reload: %s", name);
                TimerEndAlways(t_reload, label);
            }
            g_features[i].handle = h;
            strncpy(g_features[i].path, path_copy, sizeof(g_features[i].path)-1);
            g_features[i].path[sizeof(g_features[i].path)-1] = '\0';
            strncpy(g_features[i].uniq, uniquepath, sizeof(g_features[i].uniq)-1);
            g_features[i].uniq[sizeof(g_features[i].uniq)-1] = '\0';
            fl_widget_purge_owner(retire_owner, path_copy, retire_gen);
            sweep_retired_libs();
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
            {
                char loaded[512];
                loaded[0] = '\0';
                if (g_features[i].uniq[0]) {
                    strncpy(loaded, g_features[i].uniq, sizeof(loaded)-1);
                } else {
                    strncpy(loaded, g_features[i].path, sizeof(loaded)-1);
                }
                loaded[sizeof(loaded)-1] = '\0';
                retire_old_lib(g_features[i].handle, g_features[i].uniq, loaded);
            }
            sweep_retired_libs();
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

    FL_LOCK();
    for (int i = 0; i < g_feature_count; i++) {
        if (g_features[i].handle) {
            typedef void (*ShutdownFn)(void);
            ShutdownFn shut = (ShutdownFn)Platform_GetSymbol(g_features[i].handle, "FlintFeatureShutdown");
            if (shut) shut();
            {
                char loaded[512];
                loaded[0] = '\0';
                if (g_features[i].uniq[0]) {
                    strncpy(loaded, g_features[i].uniq, sizeof(loaded)-1);
                } else {
                    strncpy(loaded, g_features[i].path, sizeof(loaded)-1);
                }
                loaded[sizeof(loaded)-1] = '\0';
                retire_old_lib(g_features[i].handle, g_features[i].uniq, loaded);
            }
            g_features[i].handle = 0;
            g_features[i].uniq[0] = '\0';
        }
    }
    g_feature_count = 0;
    sweep_retired_libs();
    FL_UNLOCK();
    fl_widget_bump_generation();
    FeatureLoader_Init();
    FL_LOCK();
    for (int i = 0; i < g_retired_count; i++) {
        if (g_retired[i].owner[0]) fl_widget_purge_owner(g_retired[i].owner, NULL, g_retired[i].owner_gen);
    }
    sweep_retired_libs();
    FL_UNLOCK();

        log_info("FeatureLoader_ReloadAll completed");
PROFILE_END_VAR(timer1, "Hot Reload");
}

void FeatureLoader_Lock(void) {
    FL_LOCK();
}

void FeatureLoader_Unlock(void) {
    FL_UNLOCK();
}
