#include "helpers/x11/clipboard_backend_x11.h"
#include "helpers/x11/clipboard_x11.h"
#include "helpers/x11/env_helpers_x11.h"
#include "deps/log.h"

#if defined(SKALD_WAYLAND_SESSION)
#include "helpers/wayland/clipboard_wayland.h"
#endif

static X11ClipboardBackendKind s_backend = X11_CLIPBOARD_BACKEND_X11;
static bool s_initialized = false;

X11ClipboardBackendKind x11_clipboard_backend_kind(void) {
  return s_backend;
}

bool x11_clipboard_backend_is_wayland(void) {
  return s_backend == X11_CLIPBOARD_BACKEND_WAYLAND;
}

void x11_clipboard_backend_init(void) {
  if (s_initialized)
    return;
  s_initialized = true;
#if defined(SKALD_WAYLAND_SESSION)
  if (x11_env_is_wayland())
    s_backend = X11_CLIPBOARD_BACKEND_WAYLAND;
#endif
  log_info("clipboard backend: %s",
           s_backend == X11_CLIPBOARD_BACKEND_WAYLAND ? "wayland" : "x11");
}

void x11_clipboard_backend_shutdown(void) {
#if defined(SKALD_WAYLAND_SESSION)
  wayland_clipboard_shutdown();
#endif
  x11_clipboard_shutdown();
}

int x11_clipboard_backend_set_text(const char *text) {
  int ok = 0;
#if defined(SKALD_WAYLAND_SESSION)
  if (s_backend == X11_CLIPBOARD_BACKEND_WAYLAND)
    ok = wayland_clipboard_set_text(text);
  if (!ok)
#endif
    ok = x11_clipboard_set_text(text);
  return ok;
}

int x11_clipboard_backend_set_image(const unsigned char *png, size_t len) {
  int ok = 0;
#if defined(SKALD_WAYLAND_SESSION)
  if (s_backend == X11_CLIPBOARD_BACKEND_WAYLAND)
    ok = wayland_clipboard_set_image(png, len);
  if (!ok)
#endif
    ok = x11_clipboard_set_image(png, len);
  return ok;
}

int x11_clipboard_backend_set_file(const char *path) {
  int ok = 0;
#if defined(SKALD_WAYLAND_SESSION)
  if (s_backend == X11_CLIPBOARD_BACKEND_WAYLAND)
    ok = wayland_clipboard_set_file(path);
  if (!ok)
#endif
    ok = x11_clipboard_set_file(path);
  return ok;
}

char *x11_clipboard_backend_get_text(void) {
#if defined(SKALD_WAYLAND_SESSION)
  if (s_backend == X11_CLIPBOARD_BACKEND_WAYLAND) {
    char *out = wayland_clipboard_get_text();
    if (out)
      return out;
  }
#endif
  return x11_clipboard_get_text();
}
