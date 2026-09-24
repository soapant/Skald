#include "core/window_manager.h"
#include "core/platform.h"
#include "core/feature_loader.h"
#include "deps/log.h"
#include "config.h"
#include "platform/lib.h"
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

static _Atomic bool g_watcher_stop = false;

static void* feature_watcher_thread(void* arg) {
    WindowManager* wm = (WindowManager*)arg;
    while (!atomic_load(&g_watcher_stop) && !(wm && wm->watcher_stop)) {
        char* filename = Platform_WatchForCFileChanges(FeaturePath);
        if (filename) {
            usleep(200000);
            Platform_WatchForCFileChanges(FeaturePath);
            log_info("Changed file: %s", filename);
            char buf[256];
            strncpy(buf, filename, sizeof(buf)-1);
            buf[sizeof(buf)-1]='\0';
            free(filename);
            char* base = strrchr(buf, '/');
            if (!base) base = strrchr(buf, '\\');
            if (base) base++; else base = buf;
            char* dot = strrchr(base, '.');
            if (dot) *dot = '\0';
            char src[512], dll[512];
            snprintf(src, sizeof(src), "src/features/%s.c", base);
            snprintf(dll, sizeof(dll), "build/features/%s.so", base);
            log_info("recompiling %s -> %s", src, dll); fflush(stdout);
            if (Platform_BuildFeature(src, dll)) {
                log_info("reload %s", base); fflush(stdout);
                FeatureLoader_Reload(base);
            } else {
                log_error("compile failed for %s", base);
            }
        }
        for (int i=0;i<5;i++) {
            if (atomic_load(&g_watcher_stop) || (wm && wm->watcher_stop)) break;
            usleep(100000);
            Config_PollWatcher();
        }
    }
    return NULL;
}

void* Platform_FeatureWatcherThread(void* arg) {
    return feature_watcher_thread(arg);
}

bool Platform_WatcherStart(WindowManager* wm) {
    if (!wm) return false;
    atomic_store(&g_watcher_stop, false);
    wm->watcher_stop = false;
    wm->watcher_running = false;
    pthread_t tid;
    if (pthread_create(&tid, NULL, feature_watcher_thread, wm) == 0) {
        wm->watcher_thread = (void*)(uintptr_t)tid;
        wm->watcher_running = true;
        return true;
    }
    return false;
}

void Platform_WatcherStop(WindowManager* wm) {
    if (!wm) return;
    wm->watcher_stop = true;
    atomic_store(&g_watcher_stop, true);
    if (wm->watcher_running) {
        pthread_t tid = (pthread_t)(uintptr_t)wm->watcher_thread;
        pthread_join(tid, NULL);
        wm->watcher_running = false;
    }
    Platform_CleanupDirWatchers();
}
