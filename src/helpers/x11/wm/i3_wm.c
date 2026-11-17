#include "wm/i3_wm.h"
#include "wm/wm.h"

#include <X11/Xlib.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "deps/log.h"
#include "platform/platform.h"
#include "helpers/x11/display_x11.h"
#include "helpers/x11/env_helpers_x11.h"

static int i3_probe = -1;
static char i3_sock[512];

static bool i3_sock_live(const char *path) {
  struct stat st;
  return path && path[0] && stat(path, &st) == 0;
}

bool i3_is_active(void) {
  if (i3_probe != -1)
    return i3_probe == 1;
  i3_probe = 0;
  const char *cur = getenv("I3SOCK");
  if (i3_sock_live(cur)) {
    snprintf(i3_sock, sizeof(i3_sock), "%s", cur);
    i3_probe = 1;
    return true;
  }
  const char *rtd = x11_env_xdg_runtime_dir();
  if (rtd && rtd[0]) {
    char pat[512];
    snprintf(pat, sizeof(pat), "%s/i3/ipc-socket.*", rtd);
    glob_t g;
    memset(&g, 0, sizeof(g));
    if (glob(pat, 0, NULL, &g) == 0 && g.gl_pathc > 0) {
      const char *last = g.gl_pathv[g.gl_pathc - 1];
      if (i3_sock_live(last)) {
        snprintf(i3_sock, sizeof(i3_sock), "%s", last);
        setenv("I3SOCK", i3_sock, 1);
        i3_probe = 1;
      }
    }
    globfree(&g);
  }
  return i3_probe == 1;
}

static bool i3_exec(const char *command) {
  if (!i3_is_active())
    return false;
  char *out = Platform_RunTerminalCommand(command, false);
  if (!out || out[0] == '\0') {
    if (out)
      free(out);
    return false;
  }
  bool ok = !(strstr(out, "\"success\":false") || strstr(out, "\"error\""));
  free(out);
  return ok;
}

static Window i3_input_focus(Display *dpy) {
  Window focus = None;
  int revert_to = 0;
  if (!dpy)
    return None;
  XGetInputFocus(dpy, &focus, &revert_to);
  XSync(dpy, False);
  if (focus == None || focus == PointerRoot)
    return None;
  return focus;
}

static bool i3_move_to_output(bool right) {
  Display *dpy = x11_get_display();
  Window focus = i3_input_focus(dpy);
  char cmd[128];
  snprintf(cmd, sizeof(cmd), "i3-msg move container to output %s",
           right ? "right" : "left");
  if (!i3_exec(cmd))
    return false;
  if (focus != None) {
    char focus_cmd[96];
    snprintf(focus_cmd, sizeof(focus_cmd), "i3-msg '[id=\"0x%lx\"] focus'",
             (unsigned long)focus);
    if (!i3_exec(focus_cmd))
      log_warn("i3: failed to refocus container 0x%lx after move",
               (unsigned long)focus);
  }
  return true;
}

bool move_active_window_left_i3(void) { return i3_move_to_output(false); }

bool move_active_window_right_i3(void) { return i3_move_to_output(true); }

bool move_active_window_to_monitor_i3(bool right) {
  return i3_move_to_output(right);
}
