#ifndef CLIPBOARD_BACKEND_X11_H
#define CLIPBOARD_BACKEND_X11_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
  X11_CLIPBOARD_BACKEND_X11 = 0,
  X11_CLIPBOARD_BACKEND_WAYLAND = 1,
} X11ClipboardBackendKind;

bool x11_clipboard_backend_is_wayland(void);

void x11_clipboard_backend_init(void);
void x11_clipboard_backend_shutdown(void);

int x11_clipboard_backend_set_text(const char *text);
int x11_clipboard_backend_set_image(const unsigned char *png, size_t len);
int x11_clipboard_backend_set_file(const char *path);
char *x11_clipboard_backend_get_text(void);

#endif
