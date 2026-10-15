#ifndef PLATFORM_X11_CLIPBOARD_BACKEND_H
#define PLATFORM_X11_CLIPBOARD_BACKEND_H

#include <stdbool.h>
#include <stddef.h>

bool Platform_ClipboardAvailable(void);
int Platform_ClipboardSetText(const char *text);
int Platform_ClipboardSetImage(const unsigned char *png, size_t len);
int Platform_ClipboardSetFile(const char *path);
char* Platform_ClipboardGetText(void);
void Platform_ClipboardShutdown(void);

#endif
