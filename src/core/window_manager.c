#include "core/context.h"
#include "core/modal.h"
#include "core/window_manager.h"
#include "core/platform.h"
#include "core/common_ui.h"
#include "core/modal.h"
#include "core/feature_data.h"
#include "core/vim.h"
#include "bindings/vim_helpers.h"
#include "bindings/vim_motions.h"
#include "bindings/bindings.h"
#include "core/number_shortcut.h"
#include "core/prompt_controller.h"
#include "config.h"
#include "core/platform.h"
#include "helpers/hotkey.h"
#include "helpers/alias.h"
#include "helpers/flag.h"
#include "helpers/file.h"
#include "helpers/ui.h"
#include "config.h"
#include "core/feature_loader.h"
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#if !defined(_WIN32)
#include <pthread.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

#if !defined(_WIN32)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <unistd.h>
#include <stdatomic.h>
#include "platform/x11/display.h"
#include "platform/x11/drawing.h"
#include "platform/x11/window_factory.h"
extern int WindowManager_RunLinux(WindowManager* wm);
extern void WindowManager_ProcessEventsLinux(WindowManager* wm);
extern void WindowManager_QuitLinux(WindowManager* wm);
extern void Linux_ProcessXEvent(Display* dpy, XEvent* ev, WindowManager* wm);
extern void EditBufferBackspace(char* buf, size_t* cursor);
extern bool EditBufferDeleteChar(char* buf, size_t* cursor);
#else
#include <windows.h>
#include "core/timer.h"
extern int WindowManager_RunWin32(WindowManager* wm);
extern void WindowManager_ProcessEventsWin32(WindowManager* wm);
extern void WindowManager_QuitWin32(WindowManager* wm);
#endif

static WindowManager* g_window_manager = NULL;

#if !defined(_WIN32)
static _Atomic bool g_watcher_stop = false;
#else
static volatile LONG g_watcher_stop_win = 0;
#endif

#if defined(_WIN32)

int pending_hotkey_vk = 0;
int pending_hotkey_mod = 0;
long pending_hotkey_timeMs = 0;
void SavePerFeatureState(const char* feature, EditorMode mode, int last_index) {
    PROFILE_START(timer);
    (void)feature; (void)mode; (void)last_index;
    PROFILE_END_VAR(timer, "window_manager:SavePerFeatureState");
}
#endif

static void on_c_file_changed(void) {
    printf("A .c file was modified!\n");
    fflush(stdout);
}

#if !defined(_WIN32)
static void* feature_watcher_thread(void* arg) {
    WindowManager* wm = (WindowManager*)arg;
    while (!atomic_load(&g_watcher_stop) && !(wm && wm->watcher_stop)) {
        char* filename = Platform_WatchForCFileChanges(FeaturePath, on_c_file_changed);
        if (filename) {
            printf("Changed file: %s\n", filename);
            fflush(stdout);
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
            printf("[WATCHER] recompiling %s -> %s\n", src, dll); fflush(stdout);
            if (Platform_BuildFeature(src, dll)) {
                printf("[WATCHER] reload %s\n", base); fflush(stdout);
                FeatureLoader_Reload(base);
            } else {
                fprintf(stderr, "[WATCHER] compile failed for %s\n", base);
            }
        }
        for (int i=0;i<5;i++) {
            if (atomic_load(&g_watcher_stop) || (wm && wm->watcher_stop)) break;
            usleep(100000);
        }
    }
    return NULL;
}
#else
#include "platform/lib.h"
static DWORD WINAPI feature_watcher_thread_win(LPVOID arg) {
    WindowManager* wm = (WindowManager*)arg;
    while (InterlockedCompareExchange(&g_watcher_stop_win, 0, 0)==0 && !(wm && wm->watcher_stop)) {
        char* filename = Platform_WatchForCFileChanges(FeaturePath, on_c_file_changed);
        if (filename) {
            printf("Changed file: %s\n", filename);
            fflush(stdout);
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
            printf("[WATCHER] recompiling %s -> %s\n", src, dll); fflush(stdout);
            // unload to release file lock before rebuilding
            FeatureLoader_Unload(base);
            if (Platform_BuildFeature(src, dll)) {
                printf("[WATCHER] reload %s\n", base); fflush(stdout);
                FeatureLoader_Reload(base);
            } else {
                fprintf(stderr, "[WATCHER] compile failed for %s\n", base);
            }
            Sleep(1200); // debounce - avoid double trigger from editor atomic save
        } else {
            for (int i=0;i<5;i++) {
                if (InterlockedCompareExchange(&g_watcher_stop_win, 0, 0)!=0 || (wm && wm->watcher_stop)) break;
                Sleep(100);
            }
        }
    }
    return 0;
}
#endif

WindowManager* WindowManager_Create(void) {
    PROFILE_START(timer);
    WindowManager* wm = calloc(1, sizeof(WindowManager));
    if (!wm) { PROFILE_END_VAR(timer, "window_manager:WindowManager_Create"); return NULL; }
    wm->popup_capacity = 8;
    wm->popups = calloc(wm->popup_capacity, sizeof(Popup*));
    if (!wm->popups) {
        free(wm);
        PROFILE_END_VAR(timer, "window_manager:WindowManager_Create");
        return NULL;
    }
    
    #if !defined(_WIN32)
    wm->display = get_display();
    if (!wm->display) {
        free(wm->popups);
        free(wm);
        PROFILE_END_VAR(timer, "window_manager:WindowManager_Create");
        return NULL;
    }
    #endif
    
    wm->ctx = FlintContext_Get();
    wm->watcher_stop = false;
    g_window_manager = wm;
#if !defined(_WIN32)
    atomic_store(&g_watcher_stop, false);
    wm->watcher_running = false;
    pthread_t tid;
    if (pthread_create(&tid, NULL, feature_watcher_thread, wm) == 0) {
        wm->watcher_thread = (void*)(uintptr_t)tid;
        wm->watcher_running = true;
        /* keep joinable - do not detach, we join on destroy */
    }
#else
    InterlockedExchange(&g_watcher_stop_win, 0);
    HANDLE h = CreateThread(NULL, 0, feature_watcher_thread_win, wm, 0, NULL);
    wm->watcher_thread_win = (void*)h;
#endif
    PROFILE_END_VAR(timer, "window_manager:WindowManager_Create");
    return wm;
}

void WindowManager_Destroy(WindowManager* wm) {
    PROFILE_START(timer);
    if (!wm) { PROFILE_END_VAR(timer, "window_manager:WindowManager_Destroy"); return; }
    
    /* signal watcher thread to stop */
    wm->watcher_stop = true;
#if !defined(_WIN32)
    atomic_store(&g_watcher_stop, true);
    if (wm->watcher_running) {
        pthread_t tid = (pthread_t)(uintptr_t)wm->watcher_thread;
        pthread_join(tid, NULL);
        wm->watcher_running = false;
    }
    Platform_CleanupDirWatchers();
#else
    InterlockedExchange(&g_watcher_stop_win, 1);
    if (wm->watcher_thread_win) {
        HANDLE h = (HANDLE)wm->watcher_thread_win;
        WaitForSingleObject(h, 2000);
        CloseHandle(h);
        wm->watcher_thread_win = NULL;
    }
#endif
    
    for (int i = 0; i < wm->popup_count; i++) {
        if (wm->popups[i] && wm->popups[i]->vtable && wm->popups[i]->vtable->destroy) {
            wm->popups[i]->vtable->destroy(wm->popups[i]);
        }
    }
    
    free(wm->popups);
    
    if (g_window_manager == wm) g_window_manager = NULL;
    free(wm);
    PROFILE_END_VAR(timer, "window_manager:WindowManager_Destroy");
}

WindowManager* WindowManager_Get(void) {
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "window_manager:WindowManager_Get");
    return g_window_manager;
}

int WindowManager_Run(WindowManager* wm) {
    PROFILE_START(timer);
    if (!wm) { PROFILE_END_VAR(timer, "window_manager:WindowManager_Run"); return -1; }
    
    #if defined(_WIN32)
    PROFILE_END_VAR(timer, "window_manager:WindowManager_Run");
    return WindowManager_RunWin32(wm);
    #else
    PROFILE_END_VAR(timer, "window_manager:WindowManager_Run");
    return WindowManager_RunLinux(wm);
    #endif
    PROFILE_END_VAR(timer, "window_manager:WindowManager_Run");
}

void WindowManager_Quit(WindowManager* wm) {
    PROFILE_START(timer);
    if (!wm) { PROFILE_END_VAR(timer, "window_manager:WindowManager_Quit"); return; }
    
    #if defined(_WIN32)
    WindowManager_QuitWin32(wm);
    #else
    WindowManager_QuitLinux(wm);
    #endif
    PROFILE_END_VAR(timer, "window_manager:WindowManager_Quit");
}

Popup* WindowManager_PushPopup(WindowManager* wm, Popup* popup) {
    PROFILE_START(timer);
    if (!wm || !popup) { PROFILE_END_VAR(timer, "window_manager:WindowManager_PushPopup"); return NULL; }
    
    if (wm->popup_count >= wm->popup_capacity) {
        int new_cap = wm->popup_capacity * 2;
        Popup** new_popups = realloc(wm->popups, new_cap * sizeof(Popup*));
        if (!new_popups) { PROFILE_END_VAR(timer, "window_manager:WindowManager_PushPopup"); return NULL; }
        wm->popups = new_popups;
        wm->popup_capacity = new_cap;
    }
    
    popup->manager = wm;
    popup->visible = true;
    wm->popups[wm->popup_count++] = popup;
    
    if (popup->vtable && popup->vtable->on_show) {
        popup->vtable->on_show(popup);
    }
    
    PROFILE_END_VAR(timer, "window_manager:WindowManager_PushPopup");
    return popup;
}

void WindowManager_PopPopup(WindowManager* wm) {
    if (!wm || wm->popup_count == 0) return;
    
    Popup* popup = wm->popups[--wm->popup_count];
    if (popup->vtable && popup->vtable->on_hide) {
        popup->vtable->on_hide(popup);
    }
    popup->visible = false;
    popup->manager = NULL;
    if (wm->popup_count == 0) {
        Platform_DismissMessage();
    }
}

Popup* WindowManager_GetTopPopup(WindowManager* wm) {
    if (!wm || wm->popup_count == 0) return NULL;
    return wm->popups[wm->popup_count - 1];
}

void WindowManager_ProcessEvents(WindowManager* wm) {
    #if defined(_WIN32)
    WindowManager_ProcessEventsWin32(wm);
    #else
    WindowManager_ProcessEventsLinux(wm);
    #endif
}

char* WindowManager_ShowPrompt(WindowManager* wm, const PromptSpec* spec) {
    PROFILE_START(timer);
    if (!wm || !spec) { PROFILE_END_VAR(timer, "window_manager:WindowManager_ShowPrompt"); return NULL; }
#if defined(_WIN32)
    extern char* Platform_PromptUser(const PromptSpec* spec);
    char* res = Platform_PromptUser(spec);
    PROFILE_END_VAR(timer, "window_manager:WindowManager_ShowPrompt");
    return res;
#else
    Popup* popup = PromptPopup_Create(wm, spec);
    if (!popup) { PROFILE_END_VAR(timer, "window_manager:WindowManager_ShowPrompt"); return NULL; }
    Display* dpy = wm->display;
    XEvent ev;
    while (wm->running && wm->popup_count > 0) {
        if (XPending(dpy) > 0) {
            XNextEvent(dpy, &ev);
            Linux_ProcessXEvent(dpy, &ev, wm);
        } else {
            usleep(1000);
        }
    }
    if (popup->manager == NULL) {
        if (popup->vtable && popup->vtable->destroy) popup->vtable->destroy(popup);
    } else {
        WindowManager_PopPopup(wm);
        if (popup->vtable && popup->vtable->destroy) popup->vtable->destroy(popup);
    }
    if (dpy) { XFlush(dpy); XSync(dpy, False); }
    FlintContext* _c = FlintContext_Get();

    PROFILE_END_VAR(timer, "window_manager:WindowManager_ShowPrompt");
    return _c->window.output_buffer[0] ? _c->window.output_buffer : NULL;
#endif
    PROFILE_END_VAR(timer, "window_manager:WindowManager_ShowPrompt");
}

char* WindowManager_ShowSimplePrompt(WindowManager* wm, const SimplePromptSpec* spec) {
    PROFILE_START(timer);
    if (!wm || !spec) { PROFILE_END_VAR(timer, "window_manager:WindowManager_ShowSimplePrompt"); return NULL; }
#if defined(_WIN32)
    extern char* Platform_SimplePromptUser(const SimplePromptSpec* spec);
    char* res = Platform_SimplePromptUser(spec);
    PROFILE_END_VAR(timer, "window_manager:WindowManager_ShowSimplePrompt");
    return res;
#else
    Popup* popup = SimplePromptPopup_Create(wm, spec);
    if (!popup) { PROFILE_END_VAR(timer, "window_manager:WindowManager_ShowSimplePrompt"); return NULL; }
    Display* dpy = wm->display;
    XEvent ev;
    while (wm->running && wm->popup_count > 0) {
        if (XPending(dpy) > 0) {
            XNextEvent(dpy, &ev);
            Linux_ProcessXEvent(dpy, &ev, wm);
        } else {
            usleep(1000);
        }
    }
    if (popup->manager == NULL) {
        if (popup->vtable && popup->vtable->destroy) popup->vtable->destroy(popup);
    } else {
        WindowManager_PopPopup(wm);
        if (popup->vtable && popup->vtable->destroy) popup->vtable->destroy(popup);
    }
    if (dpy) { XFlush(dpy); XSync(dpy, False); }
    FlintContext* _c2 = FlintContext_Get();
    PROFILE_END_VAR(timer, "window_manager:WindowManager_ShowSimplePrompt");
    return _c2->window.output_buffer[0] ? _c2->window.output_buffer : NULL;
#endif
    PROFILE_END_VAR(timer, "window_manager:WindowManager_ShowSimplePrompt");
}



