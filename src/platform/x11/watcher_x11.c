#include "core/window_manager.h"
#include "platform/platform.h"
#include "core/feature_loader.h"
#include "deps/log.h"
#include "config.h"
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

#include "core/timer.h"
static _Atomic bool g_watcher_stop = false;

static void* feature_watcher_thread(void* arg) {
        PROFILE_START(_pt);
WindowManager* wm = (WindowManager*)arg;
    while (!atomic_load(&g_watcher_stop) && !(wm && wm->watcher_stop)) {
        char* filename = Platform_WatchForCFileChanges(FeaturePath);
        if (filename) {
            usleep(200000);
            char* drained = Platform_WatchForCFileChanges(FeaturePath);
            free(drained);
            log_info("Changed file: %s", filename);
            char rel[256];
            strncpy(rel, filename, sizeof(rel)-1);
            rel[sizeof(rel)-1]='\0';
            char* base = strrchr(rel, '/');
            if (!base) base = strrchr(rel, '\\');
            char name[128];
            snprintf(name, sizeof(name), "%s", base ? base + 1 : rel);
            char* dot = strrchr(name, '.');
            if (dot) *dot = '\0';
            free(filename);
            if (!FeaturePath || !FeaturePath[0]) continue;
            char src[512], dll[512];
            snprintf(src, sizeof(src), "%s/%s", FeaturePath, rel);
            snprintf(dll, sizeof(dll), "build/features/%s.so", name);
            log_info("recompiling %s -> %s", src, dll); fflush(stdout);
            if (Platform_BuildFeature(src, dll)) {
                log_info("reload %s", name); fflush(stdout);
                FeatureLoader_Reload(name);
            } else {
                log_error("compile failed for %s", name);
            }
        }
        for (int i=0;i<5;i++) {
            if (atomic_load(&g_watcher_stop) || (wm && wm->watcher_stop)) break;
            usleep(100000);
            Config_PollWatcher();
        }
    }
    { PROFILE_END_VAR(_pt, "feature_watcher_thread"); return NULL; }
}

void* Platform_FeatureWatcherThread(void* arg) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "Platform_FeatureWatcherThread"); return feature_watcher_thread(arg); }
}

bool Platform_WatcherStart(WindowManager* wm) {
        PROFILE_START(_pt);
if (!wm) { PROFILE_END_VAR(_pt, "Platform_WatcherStart"); return false; }
    atomic_store(&g_watcher_stop, false);
    wm->watcher_stop = false;
    wm->watcher_running = false;
    pthread_t tid;
    if (pthread_create(&tid, NULL, feature_watcher_thread, wm) == 0) {
        wm->watcher_thread = (void*)(uintptr_t)tid;
        wm->watcher_running = true;
        return true;
    }
    { PROFILE_END_VAR(_pt, "Platform_WatcherStart"); return false; }
}

void Platform_WatcherStop(WindowManager* wm) {
        PROFILE_START(_pt);
if (!wm) { PROFILE_END_VAR(_pt, "Platform_WatcherStop"); return ; }
    wm->watcher_stop = true;
    atomic_store(&g_watcher_stop, true);
    if (wm->watcher_running) {
        pthread_t tid = (pthread_t)(uintptr_t)wm->watcher_thread;
        pthread_join(tid, NULL);
        wm->watcher_running = false;
    }
    Platform_CleanupDirWatchers();
}
