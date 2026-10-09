#ifndef PLATFORM_X11_CLIPBOARD_H
#define PLATFORM_X11_CLIPBOARD_H

#include <stdbool.h>
#include <stddef.h>
typedef struct _XDisplay Display;
typedef union _XEvent XEvent;

void x11_clipboard_ensure_started(void);
void x11_clipboard_shutdown(void);
int x11_clipboard_set_text(const char *text);
int x11_clipboard_set_image(const unsigned char *png, size_t len);
int x11_clipboard_set_file(const char *path);
char* x11_clipboard_get_text(void);
bool x11_clipboard_handle_event(Display *dpy, XEvent *ev);

#endif
