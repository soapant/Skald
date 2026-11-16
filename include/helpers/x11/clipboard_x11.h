#ifndef CLIPBOARD_X11_H
#define CLIPBOARD_X11_H

#include <stdbool.h>
#include <stddef.h>
typedef struct _XDisplay Display;
typedef union _XEvent XEvent;

bool x11_clipboard_available(void);
void x11_clipboard_ensure_started(void);
void x11_clipboard_shutdown(void);
int x11_clipboard_set_text(const char *text);
int x11_clipboard_set_image(const unsigned char *png, size_t len);
int x11_clipboard_set_file(const char *path);
char *x11_clipboard_get_text(void);
unsigned long x11_clipboard_owner(void);
bool x11_clipboard_handle_event(Display *dpy, XEvent *ev);

#endif
