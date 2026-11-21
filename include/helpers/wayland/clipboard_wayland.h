#ifndef HELPERS_WAYLAND_CLIPBOARD_H
#define HELPERS_WAYLAND_CLIPBOARD_H

#include <stdbool.h>
#include <stddef.h>

int wayland_clipboard_set_text(const char *text);
int wayland_clipboard_set_image(const unsigned char *png, size_t len);
int wayland_clipboard_set_file(const char *path);
char *wayland_clipboard_get_text(void);
void wayland_clipboard_shutdown(void);

#endif
