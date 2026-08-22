#include "x11_display.h"
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>

static Display* g_display = NULL;
static bool g_display_initialized = false;

// Custom X11 error handler
int x11_error_handler(Display *d, XErrorEvent *e) {
    char error_text[256];
    XGetErrorText(d, e->error_code, error_text, sizeof(error_text));
    fprintf(stderr, "[X11 Error] Request Code: %d, Error Code: %d (%s), Resource ID: %lu, Serial: %lu\n",
            e->request_code, e->error_code, error_text, e->resourceid, e->serial);
    return 0; // Indicates that the error has been handled.
}

// Attempts to open the best available X11 display, similar to the original logic.
static Display* OpenBestDisplay(void) {
    Display* d = XOpenDisplay(NULL);
    if (d) return d;

    DIR* dir = opendir("/tmp/.X11-unix");
    if (!dir) return NULL;

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
    return d;
}

bool init_display(void) {
    if (g_display_initialized) {
        return true; // Already initialized
    }

    // Enable Xlib thread-safety (must precede all other Xlib calls):
    // the flash-message worker thread uses the shared connection.
    XInitThreads();

    // Set the custom error handler
    XSetErrorHandler(x11_error_handler);

    g_display = OpenBestDisplay();
    if (g_display == NULL) {
        fprintf(stderr, "[X11] Failed to open X11 display.\n");
        return false;
    }

    g_display_initialized = true;
    return true;
}

Display* get_display(void) {
    if (!g_display_initialized) {
        if (!init_display()) {
            return NULL; // Failed to initialize
        }
    }
    return g_display;
}

void cleanup_display(void) {
    if (g_display_initialized && g_display != NULL) {
        XCloseDisplay(g_display);
        g_display = NULL;
        g_display_initialized = false;
        // Reset error handler to default or a no-op handler if necessary
        XSetErrorHandler(NULL); 
    }
}
