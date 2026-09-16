#include "wm/x11_wm.h"
#include "deps/log.h"
#include "platform/x11/display_x11.h"
#include <X11/Xlib.h>
#include <ctype.h>
#include <string.h>
#include "core/platform.h"
#include "config.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_flag.h"
#include "helpers/helpers_file.h"
#include "helpers/helpers_text.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

static int _x11_err = 0;
static int _x11_err_handler(Display *dpy, XErrorEvent *ev) { (void)dpy; (void)ev; _x11_err = 1; return 0; }

bool get_title_x11(char *buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) { log_error("buffer='%s' buffer_size=%d", buffer ? buffer : "null", buffer_size); return false; }
    buffer[0] = '\0';
    Display *display = get_display();
    if (!display) { log_error("display is NULL buffer='%s' buffer_size=%d", buffer ? buffer : "null", buffer_size); return false; }
    Window focus = None;
    int revert_to = 0;
    _x11_err = 0;
    XErrorHandler old = XSetErrorHandler(_x11_err_handler);
    XGetInputFocus(display, &focus, &revert_to);
    XSync(display, False);
    if (_x11_err || focus == None || focus == PointerRoot) {
        log_error("focus is NULL or PointerRoot buffer='%s' buffer_size=%d", buffer ? buffer : "null", buffer_size);
        XSetErrorHandler(old);
        return false;
    }
    char *name = NULL;
    _x11_err = 0;
    int ok = XFetchName(display, focus, &name);
    XSync(display, False);
    XSetErrorHandler(old);
    if (_x11_err || !ok || !name) {
        log_error("XFetchName failed buffer='%s' buffer_size=%d", buffer ? buffer : "null", buffer_size);
        if (name) XFree(name);
        return false;
    }
    strncpy(buffer, name, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
    XFree(name);
    return buffer[0] != '\0';
}

char* get_clipboard_entry_x11() {
  char* out = Platform_RunTerminalCommand("xclip -o -selection clipboard",false);
  if (!out) log_error("xclip failed");
  return out;
}

char* paste_x11(char* content) {
  if (!content) { log_error("content is NULL"); return NULL; }
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"xdotool %s",content);
  char* out = Platform_RunTerminalCommand(buffer,false);
  if (!out) log_error("content='%s'", content ? content : "null");
  return out;
}

bool focus_window_title_x11(char* title) {
    if (!title) { log_error("title is NULL"); return false; }
    char command[512];
    snprintf(command, sizeof(command),
             "id=$(xwininfo -root -children | grep -i \"%s\" | awk '{print $1}' | head -n 1); "
             "[ -n \"$id\" ] && xprop -root -f _NET_ACTIVE_WINDOW 32a -set _NET_ACTIVE_WINDOW \"$id\"", 
             title);
    char* out = Platform_RunTerminalCommand(command, false);
    if (!out) { log_error("title='%s'", title ? title : "null"); return false; }
    free(out);
    return true;
}

WindowList get_window_names_x11() {
   char* output = Platform_RunTerminalCommand("wmctrl -x -l", false);
   WindowList list = { .windows = NULL, .count = 0 };
   char buffer[256]; 
   if (output == NULL || strlen(output) == 0) { log_error("output is NULL or empty"); return list; }
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
   if (output == NULL || strlen(output) == 0) { log_error("output is NULL or empty"); return list; }
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

