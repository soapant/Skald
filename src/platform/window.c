#include "platform/window.h"
#include <string.h>
#include <stdlib.h>

#include "core/platform.h"
#include "helpers.h"
#include "platform/x11/display.h"
#include <X11/Xlib.h>

static int _x11_err = 0;
static int _x11_err_handler(Display *dpy, XErrorEvent *ev) { (void)dpy; (void)ev; _x11_err = 1; return 0; }

static bool get_title_x11(char *buffer, int buffer_size) {
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

static bool get_title_sway(char *buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) return false;
    buffer[0] = '\0';
    char *command_output = Platform_RunTerminalCommand(
        "sh -c \"swaymsg -t get_tree | jq '.. | objects | select(.focused? == true)'\"",
        false
    );
    if (!command_output) return false;
    if (command_output[0] == '\0') { free(command_output); return false; }
    StringList lines = String_Split(command_output, "\n");
    char *name = NULL;
    char *app_id = NULL;
    for (size_t i = 0; i < lines.count; i++) {
        if (!lines.items[i]) continue;
        if (!name && Platform_StringContainsCaseInsensitive(lines.items[i], "\"name\"")) {
            name = lines.items[i];
        } else if (Platform_StringContainsCaseInsensitive(lines.items[i], "\"app_id\"")) {
            app_id = lines.items[i];
            break;
        }
    }
    if (!name) name = (char*)"";
    if (!app_id) app_id = (char*)"";
    size_t nlen = strlen(name);
    size_t alen = strlen(app_id);
    size_t size = nlen + alen + 2;
    char *output_name = malloc(size);
    if (!output_name) { StringList_Free(&lines); free(command_output); return false; }
    output_name[0] = '\0';
    if (nlen) strncpy(output_name, name, size-1);
    if (alen) strncat(output_name, app_id, size-1 - strlen(output_name));
    strncpy(buffer, output_name, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
    free(output_name);
    StringList_Free(&lines);
    free(command_output);
    return buffer[0] != '\0';
}

static bool get_title_hyprland(char *buffer, int buffer_size) {
    (void)buffer; (void)buffer_size;
    return false;
}

bool Window_GetActiveWindowTitle(char *buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) return false;
    buffer[0] = '\0';
    if (get_title_hyprland(buffer, buffer_size)) return true;
    if (get_title_sway(buffer, buffer_size)) return true;
    return get_title_x11(buffer, buffer_size);
}

bool Window_FocusByTitle(const char *title) {
    (void)title;
    return false;
}

