#include "wm/hyprland_wm.h"
#include "deps/log.h"
#include "platform/platform.h"
#include "helpers/helpers_text.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <glob.h>
#include <strings.h>
#include <time.h>
#include <sys/stat.h>

static bool hypr_sock_live(const char* sig) {
    if (!sig || !sig[0]) return false;
    const char* rtd = getenv("XDG_RUNTIME_DIR");
    if (!rtd || !rtd[0]) return false;
    char path[512];
    snprintf(path, sizeof(path), "%s/hypr/%s/.socket.sock", rtd, sig);
    struct stat st;
    return stat(path, &st) == 0;
}

static void hypr_ensure_sig(void) {
    const char* cur = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if (hypr_sock_live(cur)) return;
    if (cur && cur[0]) unsetenv("HYPRLAND_INSTANCE_SIGNATURE");
    const char* rtd = getenv("XDG_RUNTIME_DIR");
    if (!rtd || !rtd[0]) {
        log_error("hypr discovery no XDG_RUNTIME_DIR");
        return;
    }
    char pat[512];
    snprintf(pat, sizeof(pat), "%s/hypr/*/.socket.sock", rtd);
    glob_t g;
    memset(&g, 0, sizeof(g));
    if (glob(pat, 0, NULL, &g) != 0 || g.gl_pathc == 0) {
        log_error("hypr discovery no match pat='%s'", pat);
        globfree(&g);
        return;
    }
    const char* best = NULL;
    const char* best_p = NULL;
    size_t best_n = 0;
    time_t best_mtime = 0;
    for (size_t i = 0; i < g.gl_pathc; i++) {
        const char* cand = g.gl_pathv[i];
        const char* end = strrchr(cand, '/');
        if (!end || end <= cand) continue;
        const char* p = end - 1;
        while (p > cand && *(p - 1) != '/') p--;
        size_t n = (size_t)(end - p);
        if (n == 0 || n >= 128) continue;
        struct stat st;
        if (stat(cand, &st) != 0) continue;
        if (!best || st.st_mtime >= best_mtime) {
            best = cand;
            best_p = p;
            best_n = n;
            best_mtime = st.st_mtime;
        }
    }
    if (best && best_p) {
        char signame[128];
        memcpy(signame, best_p, best_n);
        signame[best_n] = '\0';
        setenv("HYPRLAND_INSTANCE_SIGNATURE", signame, 1);
        log_error("hypr discovery sig='%s' sock='%s'", signame, best);
    } else {
        log_error("hypr discovery no live sock pat='%s' count=%d", pat, (int)g.gl_pathc);
    }
    globfree(&g);
}
bool get_title_hyprland(char *buffer, int buffer_size) {
    return false;
}

bool get_app_id_hyprland(char *buffer, int buffer_size) {
    (void)buffer;
    (void)buffer_size;
    return false;
}

char* paste_hyprland(char* content) {
  if (!content) { log_error("content is NULL"); return NULL; }
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"wtype %s",content);
  char* out = Platform_RunTerminalCommand(buffer,false);
  if (!out) log_error("content='%s'", content ? content : "null");
  return out;
}


bool succeded_hypr(char* output) {
    if (!output) { log_error("output is NULL"); return false; }
    if (!output[0]) { log_error("hyprctl empty output"); return false; }
    StringList lines = String_Split(output, "\n");
    for (size_t i = 0; i < lines.count; i++) {
      if (Platform_StringContainsCaseInsensitive(lines.items[i],"running")) {
        log_error("hyprctl output='%s'", output);
        return false;
      }
    }
    if (!Platform_StringContainsCaseInsensitive(output, "ok")) {
        log_error("hyprctl no ok output='%s'", output);
        return false;
    }
    return true;
}

static bool hypr_focus_lua_cmd(const char* title, char* out, size_t cap) {
    char esc[1024];
    size_t o = 0;
    for (const char* s = title; *s && o + 4 < sizeof(esc); s++) {
        if (*s == '\\' || *s == '"') {
            esc[o++] = '\\';
            esc[o++] = *s;
        } else if (*s == '\'') {
            esc[o++] = '\'';
            esc[o++] = '\\';
            esc[o++] = '\'';
            esc[o++] = '\'';
        } else if (*s == '\n' || *s == '\r') {
            continue;
        } else {
            esc[o++] = *s;
        }
    }
    esc[o] = '\0';
    snprintf(out, cap, "hyprctl dispatch 'hl.dsp.focus({ window = \"title:%s\" })'", esc);
    return esc[0] != '\0';
}

bool focus_window_title_hyprland(char* title) {
  if (!title) { log_error("title is NULL"); return false; }
  hypr_ensure_sig();
  char cmd[1280];
  if (!hypr_focus_lua_cmd(title, cmd, sizeof(cmd))) { log_error("title='%s'", title ? title : "null"); return false; }
  char* out = Platform_RunTerminalCommand(cmd, false);
  if (succeded_hypr(out)) { free(out); return true; }
  free(out);
  if (title[0] != '\0') {
      char alt[1024];
      strncpy(alt, title, sizeof(alt)-1);
      alt[sizeof(alt)-1] = '\0';
      alt[0] = toupper((unsigned char)alt[0]);
      if (hypr_focus_lua_cmd(alt, cmd, sizeof(cmd))) {
          out = Platform_RunTerminalCommand(cmd, false);
          if (succeded_hypr(out)) { free(out); return true; }
          free(out);
      }
  }
  log_error("title='%s'", title ? title : "null");
  return false;
}

Fl_WindowList get_window_names_hyprland() {
   hypr_ensure_sig();
   char* output = Platform_RunTerminalCommand("hyprctl clients -j", false);
   Fl_WindowList list = { .windows = NULL, .count = 0 };
   char buffer[512];
   if (output == NULL || strlen(output) == 0) { log_error("output is NULL or empty"); return list; }
   StringList lines = String_Split(output, "\n");
   for (size_t i = 0; i < lines.count; i++) {
       char *match = strstr(lines.items[i], "\"class\"");
       if (match) {
           if (sscanf(match, "\"class\": \"%[^\"]\"", buffer) == 1) {
               if (strlen(buffer) == 0) continue;
                if (!strcasecmp(buffer, "window") || !strcasecmp(buffer, "colored")) continue;
               buffer[0] = toupper((unsigned char)buffer[0]);
               char** temp = realloc(list.windows, (list.count + 1) * sizeof(char*));
               if (temp == NULL) break;
               list.windows = temp;
               list.windows[list.count] = strdup(buffer);
               list.count++;
           }
       }
   }
   StringList_Free(&lines);
   free(output);
   return list;
}
Fl_WindowList get_window_titles_hyprland() {
   hypr_ensure_sig();
   char* output = Platform_RunTerminalCommand("hyprctl clients -j", false);
   Fl_WindowList list = { .windows = NULL, .count = 0 };
   char buffer[512];
   if (output == NULL || strlen(output) == 0) { log_error("output is NULL or empty"); return list; }
   StringList lines = String_Split(output, "\n");
   for (size_t i = 0; i < lines.count; i++) {
       char *match = strstr(lines.items[i], "\"title\"");
       if (match) {
           if (sscanf(match, "\"title\": \"%[^\"]\"", buffer) == 1) {
               if (strlen(buffer) == 0) continue;
                if (!strcasecmp(buffer, "window") || !strcasecmp(buffer, "colored")) continue;
               char** temp = realloc(list.windows, (list.count + 1) * sizeof(char*));
               if (temp == NULL) break;
               list.windows = temp;
               list.windows[list.count] = strdup(buffer);
               list.count++;
           }
       }
   }
   StringList_Free(&lines);
   free(output);
   return list;
}

Fl_Pos get_cursor_position_hyprland(void) {
  hypr_ensure_sig();
  
  char* output = Platform_RunTerminalCommand("hyprctl cursorpos", false);

  int x_val = 0;
  int y_val = 0;

  if (sscanf(output, "%d, %d", &x_val, &y_val) != 2) {
      x_val = 0;
      y_val = 0;
  }

  free(output);
  return (Fl_Pos){x_val, y_val};
}

bool close_active_window_hyprland(void) {
  hypr_ensure_sig();
  const char* sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");
  if (!sig || !sig[0]) return false;
  char* out = Platform_RunTerminalCommand("hyprctl dispatch 'hl.dsp.window.close()'", false);
  bool ok = succeded_hypr(out);
  free(out);
  return ok;
}

bool toggle_fullscreen_active_hyprland(void) {
  hypr_ensure_sig();
  const char* sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");
  if (!sig || !sig[0]) return false;
  char* out = Platform_RunTerminalCommand("hyprctl dispatch 'hl.dsp.window.fullscreen({ mode = \"fullscreen\", action = \"toggle\" })'", false);
  bool ok = succeded_hypr(out);
  free(out);
  return ok;
}

bool move_active_window_to_monitor_hyprland(bool right) {
  hypr_ensure_sig();
  const char* sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");
  if (!sig || !sig[0]) return false;
  char cmd[96];
  snprintf(cmd, sizeof(cmd), "hyprctl dispatch 'hl.dsp.window.move({ monitor = \"%c\" })'", right ? 'r' : 'l');
  char* out = Platform_RunTerminalCommand(cmd, false);
  bool ok = succeded_hypr(out);
  free(out);
  return ok;
}

bool minimize_active_window_hyprland(void) {
  hypr_ensure_sig();
  const char* sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");
  if (!sig || !sig[0]) return false;
  char* out = Platform_RunTerminalCommand("hyprctl dispatch 'hl.dsp.window.move({ workspace = \"special\", follow = false })'", false);
  bool ok = succeded_hypr(out);
  free(out);
  if (ok) return true;
  out = Platform_RunTerminalCommand("hyprctl dispatch 'hl.dsp.workspace.toggle_special()'", false);
  ok = succeded_hypr(out);
  free(out);
  return ok;
}

bool focus_window_left_hyprland(void) {
  hypr_ensure_sig();
  const char* sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");
  if (!sig || !sig[0]) return false;
  char* out = Platform_RunTerminalCommand("hyprctl dispatch 'hl.dsp.focus({ direction = \"l\" })'", false);
  bool ok = succeded_hypr(out);
  free(out);
  return ok;
}

bool focus_window_right_hyprland(void) {
  hypr_ensure_sig();
  const char* sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");
  if (!sig || !sig[0]) return false;
  char* out = Platform_RunTerminalCommand("hyprctl dispatch 'hl.dsp.focus({ direction = \"r\" })'", false);
  bool ok = succeded_hypr(out);
  free(out);
  return ok;
}

bool move_active_window_left_hyprland(void) {
  return move_active_window_to_monitor_hyprland(false);
}

bool move_active_window_right_hyprland(void) {
  return move_active_window_to_monitor_hyprland(true);
}
