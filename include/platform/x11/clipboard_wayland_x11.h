#ifndef PLATFORM_X11_CLIPBOARD_WAYLAND_H
#define PLATFORM_X11_CLIPBOARD_WAYLAND_H

#include <stdbool.h>

bool Wayland_ClipboardAvailable(void);
int Wayland_ClipboardSetText(const char *text);
void Wayland_ClipboardShutdown(void);

#endif
