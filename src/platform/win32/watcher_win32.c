#include "core/window_manager.h"
#include "core/platform.h"
#include "core/feature_loader.h"
#include "deps/log.h"
#include "config.h"
#include "platform/lib.h"
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

static volatile LONG g_watcher_stop_win = 0;

static DWORD WINAPI feature_watcher_thread_win(LPVOID arg) {
    WindowManager* wm = (WindowManager*)arg;
    while (InterlockedCompareExchange(&g_watcher_stop_win, 0, 0)==0 && !(wm && wm->watcher_stop)) {
        char* filename = Platform_WatchForCFileChanges(FeaturePath);
        if (filename) {
            Sleep(200);
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
            snprintf(dll, sizeof(dll), "build/features/%s%s", base, Platform_LibExtension());
            log_info("recompiling %s -> %s", src, dll); fflush(stdout);
            FeatureLoader_Unload(base);
            if (Platform_BuildFeature(src, dll)) {
                log_info("reload %s", base); fflush(stdout);
                FeatureLoader_Reload(base);
            } else {
                log_error("compile failed for %s", base);
            }
            Sleep(TimeBetweenHotReload*1000);
        } else {
            for (int i=0;i<5;i++) {
                if (InterlockedCompareExchange(&g_watcher_stop_win, 0, 0)!=0 || (wm && wm->watcher_stop)) break;
                Sleep(100);
                Config_PollWatcher();
            }
        }
    }
    return 0;
}

void* Platform_FeatureWatcherThread(void* arg) {
    feature_watcher_thread_win(arg);
    return NULL;
}

bool Platform_WatcherStart(WindowManager* wm) {
    if (!wm) return false;
    InterlockedExchange(&g_watcher_stop_win, 0);
    wm->watcher_stop = false;
    wm->watcher_running = false;
    HANDLE h = CreateThread(NULL, 0, feature_watcher_thread_win, wm, 0, NULL);
    wm->watcher_thread = (void*)h;
    wm->watcher_running = (h != NULL);
    return h != NULL;
}

void Platform_WatcherStop(WindowManager* wm) {
    if (!wm) return;
    wm->watcher_stop = true;
    InterlockedExchange(&g_watcher_stop_win, 1);
    if (wm->watcher_thread) {
        HANDLE h = (HANDLE)wm->watcher_thread;
        WaitForSingleObject(h, 2000);
        CloseHandle(h);
        wm->watcher_thread = NULL;
    }
    wm->watcher_running = false;
}
