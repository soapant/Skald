#include "wm/x11_wm.h"
#include "deps/log.h"
#include "platform/x11/display.h"
#include <X11/Xlib.h>
#include <ctype.h>
#include <string.h>
#include "core/platform.h"
#include "config.h"
#include "helpers/hotkey.h"
#include "helpers/alias.h"
#include "helpers/flag.h"
#include "helpers/file.h"
#include "helpers/ui.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

static int _x11_err = 0;
static int _x11_err_handler(Display *dpy, XErrorEvent *ev) { (void)dpy; (void)ev; _x11_err = 1; return 0; }

bool get_title_x11(char *buffer, int buffer_size) {
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

char* get_clipboard_entry_x11() {
  return Platform_RunTerminalCommand("xclip -o -selection clipboard",false);
}

char* paste_x11(char* content) {
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"xdotool %s",content);
  return Platform_RunTerminalCommand(buffer,false);
}

bool focus_window_title_x11(char* title) {
  return Platform_RunTerminalCommand("wmctrl -a \"firefox\"",false);

}

WindowList get_window_names_x11() {
   char* output = Platform_RunTerminalCommand("wmctrl -x -l", false);
   WindowList list = { .windows = NULL, .count = 0 };
   char buffer[256]; 
   if (output == NULL || strlen(output) == 0) return list;
   StringList lines = String_Split(output, "\n");
   for (size_t i = 0; i < lines.count; i++) {
       char hex_id[32];
       char desktop_num[16];
       char class_string[128];
       if (sscanf(lines.items[i], "%s %s %s", hex_id, desktop_num, class_string) == 3) {
           char *dot = strchr(class_string, '.');
           if (dot) *dot = '\0';
           strncpy(buffer, class_string, sizeof(buffer) - 1);
           buffer[sizeof(buffer) - 1] = '\0';
           if (strcmp(buffer, "N/A") == 0 || strlen(buffer) == 0) continue;
           buffer[0] = toupper((unsigned char)buffer[0]);
           char** temp = realloc(list.windows, (list.count + 1) * sizeof(char*));
           if (temp == NULL) break;
           list.windows = temp;
           list.windows[list.count] = strdup(buffer);
           list.count++;
       }
   }
   StringList_Free(&lines);
   free(output);
   return list;
}
WindowList get_window_titles_x11() {
   char* output = Platform_RunTerminalCommand("wmctrl -l", false);
   WindowList list = { .windows = NULL, .count = 0 };
   if (output == NULL || strlen(output) == 0) return list;
   StringList lines = String_Split(output, "\n");
   for (size_t i = 0; i < lines.count; i++) {
       char *line = lines.items[i];
       if (!line || strlen(line)==0) continue;
       char hex_id[32]; char desktop[16]; char host[128];
       int off=0;
       if (sscanf(line, "%31s %15s %127s %n", hex_id, desktop, host, &off) < 3) continue;
       char *title = line + off;
       while (*title==' ' || *title=='\t') title++;
       if (strlen(title)==0) continue;
       char** temp = realloc(list.windows, (list.count + 1) * sizeof(char*));
       if (temp == NULL) break;
       list.windows = temp;
       list.windows[list.count] = strdup(title);
       list.count++;
   }
   StringList_Free(&lines);
   free(output);
   return list;
}
WindowList get_list_of_windows_x11() { return get_window_names_x11(); }

