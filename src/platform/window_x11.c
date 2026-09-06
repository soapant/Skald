#ifndef _WIN32
#include "platform/window.h"
#include "x11_display.h"
#include <X11/Xlib.h>
#include <string.h>

static int _x11_err = 0;
static int _x11_err_handler(Display *dpy, XErrorEvent *ev) { (void)dpy; (void)ev; _x11_err = 1; return 0; }

bool Window_X11_GetActiveWindowTitle(char *buffer, int buffer_size)
{
    if (!buffer || buffer_size <= 0) return false;
    buffer[0] = '\0';
    Display *display = get_display();
    if (!display) return false;
    Window focus = None;
    int revert_to = 0;
    _x11_err = 0;
    XErrorHandler old = XSetErrorHandler(_x11_err_handler);
    XGetInputFocus(display, &focus, &revert_to);
    XSync(display, False);
    if (_x11_err || focus == None || focus == PointerRoot) {
        XSetErrorHandler(old);
        return false;
    }
    char *name = NULL;
    _x11_err = 0;
    int ok = XFetchName(display, focus, &name);
    XSync(display, False);
    XSetErrorHandler(old);
    if (_x11_err || !ok || !name) {
        if (name) XFree(name);
        return false;
    }
    strncpy(buffer, name, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
    XFree(name);
    return buffer[0] != '\0';
}
#endif
