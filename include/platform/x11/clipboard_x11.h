#ifndef PLATFORM_X11_CLIPBOARD_H
#define PLATFORM_X11_CLIPBOARD_H

#include <stdbool.h>
#include <stddef.h>
typedef struct _XDisplay Display;
typedef union _XEvent XEvent;

void Linux_ClipboardEnsureStarted(void);
void Linux_ClipboardShutdown(void);
int Linux_ClipboardSetText(const char *text);
int Linux_ClipboardSetImage(const unsigned char *png, size_t len);
char* Linux_ClipboardGetText(void);
bool Linux_ClipboardHandleEvent(Display *dpy, XEvent *ev);

#endif
