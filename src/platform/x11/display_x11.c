#include "platform/x11/display.h"
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include "core/timer.h"

static Display* g_display = NULL;
static bool g_display_initialized = false;


int x11_error_handler(Display *d, XErrorEvent *e) {
    WinTimer timer = TimerStart();
    char error_text[256];
    XGetErrorText(d, e->error_code, error_text, sizeof(error_text));
    fprintf(stderr, "[X11 Error] Request Code: %d, Error Code: %d (%s), Resource ID: %lu, Serial: %lu\n",
            e->request_code, e->error_code, error_text, e->resourceid, e->serial);
    TimerEnd(timer, "display_x11:x11_error_handler");
    return 0; 
}


static Display* OpenBestDisplay(void) {
    WinTimer timer = TimerStart();
    Display* d = XOpenDisplay(NULL);
    if (d) { TimerEnd(timer, "display_x11:OpenBestDisplay"); return d; }

    DIR* dir = opendir("/tmp/.X11-unix");
    if (!dir) { TimerEnd(timer, "display_x11:OpenBestDisplay"); return NULL; }

    char name[32];
    struct dirent* e;
    while ((e = readdir(dir)) != NULL) {
        if (e->d_name[0] == 'X') {
            snprintf(name, sizeof(name), ":%s", e->d_name + 1);
            d = XOpenDisplay(name);
            if (d) break;
        }
    }
    closedir(dir);
    TimerEnd(timer, "display_x11:OpenBestDisplay");
    return d;
}

bool init_display(void) {
    WinTimer timer = TimerStart();
    if (g_display_initialized) {
        TimerEnd(timer, "display_x11:init_display");
        return true; 
    }
    
    XInitThreads();

    XSetErrorHandler(x11_error_handler);

    g_display = OpenBestDisplay();
    if (g_display == NULL) {
        fprintf(stderr, "[X11] Failed to open X11 display.\n");
        TimerEnd(timer, "display_x11:init_display");
        return false;
    }

    g_display_initialized = true;
    TimerEnd(timer, "display_x11:init_display");
    return true;
}

Display* get_display(void) {
    WinTimer timer = TimerStart();
    if (!g_display_initialized) {
        if (!init_display()) {
            TimerEnd(timer, "display_x11:get_display");
            return NULL; 
        }
    }
    TimerEnd(timer, "display_x11:get_display");
    return g_display;
}

void cleanup_display(void) {
    WinTimer timer = TimerStart();
    if (g_display_initialized && g_display != NULL) {
        XCloseDisplay(g_display);
        g_display = NULL;
        g_display_initialized = false;
        
        XSetErrorHandler(NULL); 
    }
    TimerEnd(timer, "display_x11:cleanup_display");
}
