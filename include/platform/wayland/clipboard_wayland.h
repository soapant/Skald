#ifndef PLATFORM_WAYLAND_CLIPBOARD_H
#define PLATFORM_WAYLAND_CLIPBOARD_H

#include <stdbool.h>
#include <stddef.h>

int Platform_WaylandClipboard_SetText(const char *text);
int Platform_WaylandClipboard_SetImage(const unsigned char *png, size_t len);
int Platform_WaylandClipboard_SetFile(const char *path);
char *Platform_WaylandClipboard_GetText(void);
void Platform_WaylandClipboard_Shutdown(void);

#endif
