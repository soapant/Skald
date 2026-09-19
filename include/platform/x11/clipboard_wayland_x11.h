#ifndef PLATFORM_X11_CLIPBOARD_WAYLAND_H
#define PLATFORM_X11_CLIPBOARD_WAYLAND_H

#include <stdbool.h>
#include <stddef.h>

bool Wayland_ClipboardAvailable(void);
int Wayland_ClipboardSetText(const char *text);
int Wayland_ClipboardSetImage(const unsigned char *png, size_t len);
void Wayland_ClipboardShutdown(void);

#endif
