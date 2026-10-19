#include "core/window_manager.h"
#include "platform/platform.h"
#include "core/feature_loader.h"
#include "deps/log.h"
#include "config.h"
#include "platform/lib.h"
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

#include "core/timer.h"
static volatile LONG g_watcher_stop_win = 0;

static DWORD WINAPI feature_watcher_thread_win(LPVOID arg) {
        PROFILE_START(_pt);
WindowManager* wm = (WindowManager*)arg;
    while (InterlockedCompareExchange(&g_watcher_stop_win, 0, 0)==0 && !(wm && wm->watcher_stop)) {
        char* filename = Platform_WatchForCFileChanges(FeaturePath);
        if (filename) {
            Sleep(200);
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
            snprintf(src, sizeof(src), "%s\\%s", FeaturePath, rel);
            snprintf(dll, sizeof(dll), "build/features/%s%s", name, Platform_LibExtension());
            log_info("recompiling %s -> %s", src, dll); fflush(stdout);
            FeatureLoader_Unload(name);
            if (Platform_BuildFeature(src, dll)) {
                log_info("reload %s", name); fflush(stdout);
                FeatureLoader_Reload(name);
            } else {
                log_error("compile failed for %s", name);
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
    { PROFILE_END_VAR(_pt, "feature_watcher_thread_win"); return 0; }
}

void* Platform_FeatureWatcherThread(void* arg) {
        PROFILE_START(_pt);
feature_watcher_thread_win(arg);
    { PROFILE_END_VAR(_pt, "Platform_FeatureWatcherThread"); return NULL; }
}

bool Platform_WatcherStart(WindowManager* wm) {
        PROFILE_START(_pt);
if (!wm) { PROFILE_END_VAR(_pt, "Platform_WatcherStart"); return false; }
    InterlockedExchange(&g_watcher_stop_win, 0);
    wm->watcher_stop = false;
    wm->watcher_running = false;
    HANDLE h = CreateThread(NULL, 0, feature_watcher_thread_win, wm, 0, NULL);
    wm->watcher_thread = (void*)h;
    wm->watcher_running = (h != NULL);
    { PROFILE_END_VAR(_pt, "Platform_WatcherStart"); return h != NULL; }
}

void Platform_WatcherStop(WindowManager* wm) {
        PROFILE_START(_pt);
if (!wm) { PROFILE_END_VAR(_pt, "Platform_WatcherStop"); return ; }
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
