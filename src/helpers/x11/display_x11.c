#include "helpers/x11/display_x11.h"
#include <stdio.h>
#include <dirent.h>
#include "deps/log.h"

static Display *g_display = NULL;
static bool g_display_initialized = false;

int x11_error_handler(Display *d, XErrorEvent *e) {
  char error_text[256];
  XGetErrorText(d, e->error_code, error_text, sizeof(error_text));
  log_error(
      "Request Code: %d, Error Code: %d (%s), Resource ID: %lu, Serial: %lu",
      e->request_code, e->error_code, error_text, e->resourceid, e->serial);
  return 0;
}

static Display *x11_open_best_display(void) {

  Display *d = XOpenDisplay(NULL);
  if (d)
    return d;

  DIR *dir = opendir("/tmp/.X11-unix");
  if (!dir)
    return NULL;

  char name[32];
  struct dirent *e;
  while ((e = readdir(dir)) != NULL) {
    if (e->d_name[0] == 'X') {
      snprintf(name, sizeof(name), ":%s", e->d_name + 1);
      d = XOpenDisplay(name);
      if (d)
        break;
    }
  }
  closedir(dir);
  return d;
}

bool x11_init_display(void) {
  if (g_display_initialized) {
    return true;
  }

  XInitThreads();

  XSetErrorHandler(x11_error_handler);

  g_display = x11_open_best_display();
  if (g_display == NULL) {
    log_error("Failed to open X11 display.");
    return false;
  }

  g_display_initialized = true;
  return true;
}

Display *x11_get_display(void) {
  if (!g_display_initialized) {
    if (!x11_init_display()) {
      return NULL;
    }
  }
  return g_display;
}

void x11_cleanup_display(void) {
  if (g_display_initialized && g_display != NULL) {
    XCloseDisplay(g_display);
    g_display = NULL;
    g_display_initialized = false;

    XSetErrorHandler(NULL);
  }
}
