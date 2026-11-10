#ifndef PLATFORM_X11_CLIPBOARD_BACKEND_H
#define PLATFORM_X11_CLIPBOARD_BACKEND_H

#include <stdbool.h>
#include <stddef.h>

bool x11_clipboard_available(void);
int x11_clipboard_set_text(const char *text);
int x11_clipboard_set_image(const unsigned char *png, size_t len);
int x11_clipboard_set_file(const char *path);
char *x11_clipboard_get_text(void);
void x11_clipboard_shutdown(void);

#endif
