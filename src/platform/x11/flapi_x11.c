#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>
#include <X11/X.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <fontconfig/fontconfig.h>
#include <stdbool.h>

#include "platform/flapi_platform.h"
#include "deps/log.h"
#include "platform/platform.h"
#include "helpers/helpers_text.h"
#include "platform/draw.h"
#include "platform/ui.h"
#include "platform/x11/env_helpers_x11.h"
#include "platform/x11/desktop_helpers_x11.h"
#include "platform/x11/drawing_x11.h"
#include "helpers/helpers_ui.h"
#include "core/prompt_geometry.h"
#include "core/listbox_hover.h"
#include "core/listbox_layout.h"
#include "core/editbox.h"
#include "core/window_manager.h"
#include "platform/wm_platform.h"
#include "core/modal.h"
#include "config_values.h"
#include "core/types.h"
#include "core/types/flapi_types.h"
#include "core/types/keys.h"
#include "core/types/window.h"

static bool platform_exe_in_path(const char *name) {
  if (!name || !name[0])
    return false;
  const char *path = getenv("PATH");
  if (!path || !path[0])
    return false;
  const char *p = path;
  while (*p) {
    const char *sep = strchr(p, ':');
    size_t len = sep ? (size_t)(sep - p) : strlen(p);
    char cand[1024];
    if (len == 0)
      snprintf(cand, sizeof(cand), "./%s", name);
    else
      snprintf(cand, sizeof(cand), "%.*s/%s", (int)len, p, name);
    if (access(cand, X_OK) == 0)
      return true;
    if (!sep)
      break;
    p = sep + 1;
  }
  return false;
}

static bool platform_resolve_desktop_exec(const char *exec_line,
                                          const char *desktop_dir,
                                          const char *workdir, char *out,
                                          size_t out_size) {
  if (!exec_line || !out || out_size == 0)
    return false;
  const char *p = exec_line;
  while (*p == ' ' || *p == '\t')
    p++;

  char token[1024];
  size_t t = 0;
  if (*p == '"') {
    p++;
    while (*p) {
      if (*p == '\\' && (p[1] == '"' || p[1] == '\\'))
        p++;
      else if (*p == '"') {
        p++;
        break;
      }
      if (t + 1 < sizeof(token))
        token[t++] = *p;
      p++;
    }
  } else {
    while (*p && *p != ' ' && *p != '\t') {
      if (t + 1 < sizeof(token))
        token[t++] = *p;
      p++;
    }
  }
  token[t] = '\0';
  while (*p == ' ' || *p == '\t')
    p++;
  if (!token[0])
    return false;

  const char *base = (workdir && workdir[0]) ? workdir : desktop_dir;
  char prog[1024];
  if (strchr(token, '/')) {
    if (token[0] == '/')
      snprintf(prog, sizeof(prog), "%s", token);
    else
      snprintf(prog, sizeof(prog), "%s/%s", base, token);
  } else if (platform_exe_in_path(token)) {
    snprintf(prog, sizeof(prog), "%s", token);
  } else {
    snprintf(prog, sizeof(prog), "%s/%s", desktop_dir, token);
  }

  if (strchr(prog, '/')) {
    if (access(prog, X_OK) != 0) {
      log_info("desktop exec resolve: '%s' not executable", prog);
      return false;
    }
  } else if (!platform_exe_in_path(prog)) {
    return false;
  }

  char q[2200];
  size_t n = 0;
  q[n++] = '\'';
  for (const char *s = prog; *s && n + 5 < sizeof(q); s++) {
    if (*s == '\'') {
      memcpy(q + n, "'\\''", 4);
      n += 4;
    } else
      q[n++] = *s;
  }
  q[n++] = '\'';
  q[n] = '\0';

  snprintf(out, out_size, "%s%s%s", q, p[0] ? " " : "", p);
  return true;
}

static int platform_run_status(char *const argv[], const X11SessionEnv *env,
                               bool apply_env) {
  pid_t pid = fork();
  if (pid < 0)
    return -1;
  if (pid == 0) {
    if (apply_env && env)
      Env_ApplyToCurrentProcess(env);
    execvp(argv[0], argv);
    _exit(127);
  }
  int st = 0;
  if (waitpid(pid, &st, 0) < 0)
    return -1;
  if (!WIFEXITED(st))
    return -1;
  return WEXITSTATUS(st);
}

bool fl_launch_with_args_platform(const char *path, const char *args) {
  log_info("fl_launch path='%s' args='%s'", path ? path : "null",
           args ? args : "null");
  if (!path)
    return false;
  char expanded[1024];
  if (!Platform_ExpandPath(path, expanded, sizeof(expanded)))
    return false;
  log_info("fl_launch expanded='%s'", expanded);
  path = expanded;
  if (strstr(path, ".desktop") != NULL) {
    log_info("fl_launch desktop detected expanded='%s'", expanded);
    bool is_terminal = false;
    char exec_line[1024] = {0};
    char workdir[1024] = {0};
    Desktop_Parse(path, exec_line, workdir, &is_terminal);
    log_info("Desktop_Parse exec='%s' workdir='%s' is_terminal=%d", exec_line,
             workdir, is_terminal);
    Desktop_CleanExec(exec_line);
    log_info("Desktop_CleanExec exec='%s'", exec_line);
    if (is_terminal && exec_line[0]) {
      const char *wd = workdir[0] ? workdir : NULL;
      char extra[2048];
      if (args && args[0])
        snprintf(extra, sizeof(extra), "-e sh -c '%s %s; exec %s'", exec_line,
                 args, UsedShell);
      else
        snprintf(extra, sizeof(extra), "-e sh -c '%s; exec %s'", exec_line,
                 UsedShell);
      if (wd)
        return Platform_LaunchTerminalAtPath(wd, extra);
      else
        return Platform_LaunchTerminalAtPath(Platform_GetRealHome(), extra);
    }
    if (exec_line[0] && args && args[0]) {
      char combined[2048];
      snprintf(combined, sizeof(combined), "%s %s", exec_line, args);
      strncpy(exec_line, combined, sizeof(exec_line) - 1);
      exec_line[sizeof(exec_line) - 1] = '\0';
    }
    if (!exec_line[0]) {
      log_warn("desktop '%s' has no usable Exec line", expanded);
      return false;
    }

    char desktop_dir[1024] = {0};
    snprintf(desktop_dir, sizeof(desktop_dir), "%s", expanded);
    char *dslash = strrchr(desktop_dir, '/');
    if (dslash)
      *dslash = '\0';
    else
      snprintf(desktop_dir, sizeof(desktop_dir), ".");
    const char *run_dir = workdir[0] ? workdir : desktop_dir;

    X11SessionEnv env;
    Env_GetSessionEnv(&env);
    char disp_env[256], xauth_env[512], runtime_env[512], wayland_env[512];
    Env_BuildEnvStrings(&env, disp_env, xauth_env, runtime_env, wayland_env);
    bool as_root = Env_IsRunningAsRoot();
    log_info("desktop '%s' exec='%s' workdir='%s' run_dir='%s' as_root=%d",
             expanded, exec_line, workdir, run_dir, as_root);

    int gio_st;
    if (as_root) {
      char *const av_gio[] = {"sudo",      "-u",        (char *)env.real_user,
                              "env",       disp_env,    xauth_env,
                              runtime_env, wayland_env, "gio",
                              "launch",    expanded,    NULL};
      gio_st = platform_run_status(av_gio, NULL, false);
    } else {
      char *const av_gio2[] = {"gio", "launch", expanded, NULL};
      gio_st = platform_run_status(av_gio2, &env, true);
    }
    log_info("gio launch '%s' exit=%d", expanded, gio_st);
    if (gio_st == 0) {
      return true;
    }

    char cmd[4096];
    if (!platform_resolve_desktop_exec(exec_line, desktop_dir, workdir, cmd,
                                       sizeof(cmd))) {
      log_warn(
          "desktop '%s': cannot resolve Exec '%s' via PATH or desktop dir '%s'",
          expanded, exec_line, desktop_dir);
      return false;
    }
    char launch_cmd[4608];
    snprintf(launch_cmd, sizeof(launch_cmd), "cd '%s' && %s", run_dir, cmd);
    log_info("desktop resolved launch_cmd='%s'", launch_cmd);

    if (as_root) {
      char *const av_run[] = {"sudo",      "-u",        (char *)env.real_user,
                              "env",       disp_env,    xauth_env,
                              runtime_env, wayland_env, "sh",
                              "-c",        launch_cmd,  NULL};
      if (!Env_DaemonExec(av_run)) {
        return false;
      }
    } else {
      X11SessionEnv env2 = env;
      char *const av_run2[] = {"sh", "-c", launch_cmd, NULL};
      if (!Env_DaemonExecWithEnv(av_run2, &env2)) {
        return false;
      }
    }
    return true;
  } else {
    X11SessionEnv env;
    Env_GetSessionEnv(&env);
    char disp_env[256], xauth_env[512], runtime_env[512];
    Env_BuildEnvStrings(&env, disp_env, xauth_env, runtime_env, NULL);
    struct stat pst;
    bool is_regular_file = (stat(path, &pst) == 0 && S_ISREG(pst.st_mode));
    bool is_executable = (access(path, X_OK) == 0);
    if (is_regular_file && !is_executable) {
      char *const av_open2[] = {"gio", "open", path, NULL};
      if (!Env_DaemonExecWithEnv(av_open2, &env))
        return false;
      return true;
    }
    char cmd[2048];
    if (args && args[0] && strcmp(args, "0") != 0)
      snprintf(cmd, sizeof(cmd), "\"%s\" %s", path, args);
    else
      snprintf(cmd, sizeof(cmd), "\"%s\"", path);
    if (Env_IsRunningAsRoot()) {
      char *const argv[] = {"sudo",      "-u",     (char *)env.real_user,
                            "env",       disp_env, xauth_env,
                            runtime_env, "sh",     "-c",
                            cmd,         NULL};
      if (!Env_DaemonExec(argv))
        return false;
      return true;
    } else {
      X11SessionEnv env2 = env;
      pid_t pid = fork();
      if (pid < 0)
        return false;
      if (pid == 0) {
        setsid();
        pid_t pid2 = fork();
        if (pid2 < 0)
          _exit(1);
        if (pid2 > 0)
          _exit(0);
        int fd = open("/dev/null", O_RDWR);
        if (fd >= 0) {
          dup2(fd, 0);
          dup2(fd, 1);
          dup2(fd, 2);
          if (fd > 2)
            close(fd);
        }
        Env_ApplyToCurrentProcess(&env2);
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
      }
      int st = 0;
      waitpid(pid, &st, 0);
      return true;
    }
  }
}

bool fl_launch_with_args_as_admin_platform(const char *path, const char *args) {
  if (!path)
    return false;
  char expanded[MAX_PATH];
  if (!Platform_ExpandPath(path, expanded, sizeof(expanded)))
    return false;
  char cmd[2048];
  if (args && args[0])
    snprintf(cmd, sizeof(cmd), "%s %s", expanded, args);
  else
    snprintf(cmd, sizeof(cmd), "%s", expanded);
  pid_t pid = fork();
  if (pid < 0)
    return false;
  if (pid == 0) {
    char *argv1[] = {"pkexec", cmd, NULL};
    execvp("pkexec", argv1);
    char *argv2[] = {"sudo", cmd, NULL};
    execvp("sudo", argv2);
    _exit(1);
  }
  return true;
}

static __thread PlListSpec g_fl_list_spec;
static __thread ListBoxInfo *g_fl_list_lb;
static __thread int g_fl_list_y0, g_fl_list_itemH, g_fl_list_visible,
    g_fl_list_cur, g_fl_list_h;
static __thread int g_fl_list_bx, g_fl_list_bw, g_fl_list_fw, g_fl_list_fh;
static __thread Display *g_fl_list_dpy;
static __thread XftDraw *g_fl_list_xd;
static __thread Visual *g_fl_list_vis;
static __thread Colormap g_fl_list_cmap;
static __thread FontHandle g_fl_list_font;
static __thread bool g_fl_list_font_owned;

bool fl_listbox_begin_platform(const PlListSpec *spec) {
  if (!spec)
    return false;
  ListBoxInfo *lb = spec->lb;
  if (!lb) {
    if (!spec->items || spec->count <= 0)
      return false;
    return false;
  }
  Platform_GetDrawContext(&g_fl_list_dpy, &g_fl_list_xd, &g_fl_list_vis,
                          &g_fl_list_cmap);
  if (!g_fl_list_dpy || !g_fl_list_xd)
    return false;
  g_fl_list_spec = *spec;
  g_fl_list_lb = lb;
  g_fl_list_font =
      Platform_CreateFontOnDisplay(g_fl_list_dpy, FontSize, UsedFont);
  g_fl_list_itemH = spec->item_height ? spec->item_height
                                      : x11_font_row_height_of(g_fl_list_font);
  WindowManager *wmgr = WindowManager_Get();
  PromptState *st = NULL;
  if (wmgr) {
    for (int _i = wmgr->popup_count - 1; _i >= 0; _i--) {
      Popup *_p = wmgr->popups[_i];
      if (_p && _p->user_data && _p->type == POPUP_PROMPT) {
        PromptState *_ps = (PromptState *)_p->user_data;
        if (&_ps->lb == lb) {
          st = _ps;
          break;
        }
      }
    }
    if (!st && wmgr->popup_count > 0) {
      Popup *top = WindowManager_GetTopPopup(wmgr);
      if (top && top->user_data && top->type == POPUP_PROMPT)
        st = (PromptState *)top->user_data;
    }
  }
  ListboxLayout lay;
  Listbox_ComputeLayout(spec, x11_font_list_top_of(g_fl_list_font),
                        g_fl_list_itemH, st ? st->width : 0,
                        st ? st->height : 0, lb->item_count, &lay);
  g_fl_list_y0 = lay.y0;
  g_fl_list_bx = lay.bx;
  g_fl_list_bw = lay.bw;
  g_fl_list_fw = lay.fw;
  g_fl_list_fh = lay.fh;
  g_fl_list_h = lay.list_h;
  g_fl_list_visible = lay.visible;
  UI_LB_SetVisibleCount(lay.visible);
  g_fl_list_cur = 0;
  g_fl_list_font =
      Platform_CreateFontOnDisplay(g_fl_list_dpy, FontSize, UsedFont);
  g_fl_list_font_owned = true;
  ListBox_ApplyPendingScroll(g_fl_list_lb, lay.visible);
  ListBox_SetGeometry(g_fl_list_y0, g_fl_list_itemH, lay.visible);
  ListBox_HoverFocus(g_fl_list_lb, g_fl_list_y0, g_fl_list_itemH,
                     g_fl_list_lb->view_offset, lay.visible);
  return true;
}

bool fl_listbox_next_platform(FlListItemCtx *out) {
  if (!out || !g_fl_list_lb)
    return false;
  if (g_fl_list_cur >= g_fl_list_visible)
    return false;
  int idx = g_fl_list_lb->view_offset + g_fl_list_cur;
  if (idx < 0 || idx >= g_fl_list_lb->item_count)
    return false;
  int y = g_fl_list_y0 + g_fl_list_cur * g_fl_list_itemH;
  if (g_fl_list_cur == 0)
    Platform_DrawPromptSeparator(
        (PlDrawPromptSeparatorSpec){.y = g_fl_list_y0, .width = g_fl_list_fw});
  bool sel = (idx == g_fl_list_lb->selected_index);
  if (RoundedCorners && CornerRadius > 0) {
    int R = PROMPT_SEL_RADIUS;
    int t = PROMPT_SEL_INSET;
    int bx = g_fl_list_bx, bw = g_fl_list_bw;
    if (sel) {
      x11_fill_rounded_rect(g_fl_list_dpy, g_fl_list_xd, g_fl_list_vis,
                            g_fl_list_cmap, rgb_to_hex(COLOR_SELECTED_TEXT),
                            0xFFFF, bx + t, y + PROMPT_SEL_TOP_INSET + t,
                            bw - 2 * t, g_fl_list_itemH - 2 - 2 * t, R - 1,
                            g_fl_list_fw, g_fl_list_fh);
    }
  } else {
    PromptState tmp = {0};
    tmp.width = g_fl_list_fw;
    tmp.height = g_fl_list_y0 + g_fl_list_visible * g_fl_list_itemH;
    Platform_DrawListBackground(&tmp, g_fl_list_dpy, g_fl_list_xd,
                                g_fl_list_vis, g_fl_list_cmap, y,
                                g_fl_list_itemH, sel);
  }
  out->idx = idx;
  out->selected = sel;
  out->x = g_fl_list_spec.x;
  out->y = y;
  out->w = g_fl_list_spec.w;
  out->h = g_fl_list_itemH;
  out->text = g_fl_list_lb->items[idx];
  g_fl_list_cur++;
  return true;
}

void fl_listbox_end_platform(void) {
  if (PromptListBottomMargin() > 0 && g_fl_list_y0 >= 0) {
    Platform_DrawPromptSeparator((PlDrawPromptSeparatorSpec){
        .y = g_fl_list_y0 + g_fl_list_h, .width = g_fl_list_fw});
  }
  if (g_fl_list_font_owned) {
    Platform_DestroyFont(g_fl_list_font);
    g_fl_list_font_owned = false;
  }
  g_fl_list_lb = NULL;
  g_fl_list_visible = 0;
  g_fl_list_cur = 0;
}

int fl_listbox_get_count(void) {
  return g_fl_list_lb ? g_fl_list_lb->item_count : 0;
}

unsigned long fl_listbox_get_current_standard_color(void) {
  if (!g_fl_list_lb || g_fl_list_cur == 0)
    return 0;
  int idx = g_fl_list_lb->view_offset + g_fl_list_cur - 1;
  if (idx < 0 || idx >= g_fl_list_lb->item_count)
    return 0;
  unsigned long ncol, bcol, pcol;
  Platform_GetItemColors(
      (PlGetItemColorsSpec){.idx = idx,
                            .count = g_fl_list_lb->item_count,
                            .num_color = &ncol,
                            .body_color = &bcol,
                            .plain_color = &pcol});
  return pcol;
}

char *fl_search_file_in_dir_platform(const char *directory,
                                     const char *search_string,
                                     char *output_path) {
  if (!directory || !search_string || strlen(search_string) == 0)
    return NULL;
  DIR *dir = opendir(directory);
  if (!dir)
    return NULL;
  static char found_path[MAX_PATH];
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_name[0] == '.' &&
        (entry->d_name[1] == '\0' ||
         (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
      continue;
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s", directory, entry->d_name);
    struct stat st;
    if (stat(full_path, &st) != 0)
      continue;
    if (S_ISDIR(st.st_mode))
      continue;
    if (Platform_StringContainsCaseInsensitive(entry->d_name, search_string)) {
      snprintf(found_path, sizeof(found_path), "%s", full_path);
      closedir(dir);
      return found_path;
    }
  }
  closedir(dir);
  return NULL;
}

bool fl_place_editbox_platform(const FlEditSpec *spec) {
  if (!spec)
    return false;
  Display *dpy;
  XftDraw *xd;
  Visual *vis;
  Colormap cmap;
  Platform_GetDrawContext(&dpy, &xd, &vis, &cmap);
  if (!dpy || !xd)
    return false;
  WindowManager *wm = WindowManager_Get();
  PromptState *state = NULL;
  if (wm) {
    Popup *top = WindowManager_GetTopPopup(wm);
    if (top && top->user_data && top->type == POPUP_PROMPT)
      state = (PromptState *)top->user_data;
  }
  const char *buf = spec->buffer;
  if (!buf) {
    if (state)
      buf = state->eb->filter;
    else
      buf = "";
  }
  int x = spec->x;
  int w = spec->w;
  int h = spec->h;
  if (w <= 0)
    w = state ? state->width : PROMPT_CUSTOM_FALLBACK_W;
  if (h <= 0)
    h = PromptCustomEditHeight();
  FontHandle fh = spec->font;
  bool needDestroy = false;
  if (!FH_TO_XFT(fh)) {
    fh = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    needDestroy = true;
  }
  XftFont *xf = FH_TO_XFT(fh);
  Platform_SetDrawContext(dpy, xd, vis, cmap);

  int text_len = (int)strlen(buf);
  int cursor = state ? state->eb->cursor : text_len;
  if (cursor < 0)
    cursor = 0;
  if (cursor > text_len)
    cursor = text_len;

  EditboxX11Metrics metrics = {dpy, xf};
  int cursor_x = 0;
  int text_w = 0;
  if (xf) {
    XGlyphInfo ce;
    XftTextExtentsUtf8(dpy, xf, (FcChar8 *)buf, cursor, &ce);
    cursor_x = ce.xOff;
    XGlyphInfo te;
    XftTextExtentsUtf8(dpy, xf, (FcChar8 *)buf, text_len, &te);
    text_w = te.xOff;
  }

  int y_raw = (spec->y == 0) ? PROMPT_EDIT_INSET : spec->y;

  EditboxInit ebi;
  memset(&ebi, 0, sizeof(ebi));
  ebi.state = state;
  ebi.text = buf;
  ebi.text_len = text_len;
  ebi.cursor = cursor;
  ebi.x = x;
  ebi.y = y_raw;
  ebi.w = w;
  ebi.cursor_x = cursor_x;
  ebi.text_w = text_w;
  ebi.ascent = xf ? xf->ascent : 0;
  ebi.descent = xf ? xf->descent : 0;
  ebi.text_color = rgb_to_hex(COLOR_TEXT);
  ebi.font = fh;

  EditboxSpec eb;
  Editbox_BuildSpec(&eb, &ebi);
  eb.measure = x11_editbox_measure;
  eb.measure_user = &metrics;

  Editbox_Render(&eb, NULL);
  if (state)
    state->eb->scroll_offset = eb.scroll;

  if (state) {
    if (WindowTitleText && state->window_title) {
      unsigned long title_col = 0;
      eval_window_title_color(state, &title_col);
      Platform_SetDrawContext(dpy, xd, vis, cmap);
      Platform_DrawTextRaw(
          (PlDrawTextRawSpec){.x = PROMPT_FOOTER_X,
                              .y = state->height - PROMPT_FOOTER_Y_FROM_BOTTOM,
                              .text = state->window_title,
                              .color = title_col,
                              .font = fh,
                              .align = "LEFT",
                              .pad = PROMPT_FOOTER_PAD,
                              .fontsize = FontSize});
    }
  }
  if (needDestroy)
    Platform_DestroyFont(fh);
  return true;
}

bool fl_type_text_platform(const char *s) {
  if (!s)
    return false;
  char *old = Platform_GetClipboardEntry();
  Platform_CopyToClipboard(s);
  Platform_EmitKey(VK_LCONTROL, 1);
  Platform_EmitKey('V', 1);
  Platform_EmitKey('V', 0);
  Platform_EmitKey(VK_LCONTROL, 0);
  WindowManager *wm = WindowManager_Get();
  for (int i = 0; i < 40; i++) {
    if (wm)
      Platform_WM_ProcessEvents(wm);
    usleep(5000);
  }
  if (old) {
    Platform_CopyToClipboard(old);
    free(old);
  }
  return true;
}

typedef struct {
  double seconds;
  void (*callback)(void);
} TimerPayload;

static void *TimerThread(void *arg) {

  TimerPayload *payload = (TimerPayload *)arg;
  if (!payload)
    return NULL;

  useconds_t micros = (useconds_t)(payload->seconds * 1000000.0);
  usleep(micros);

  if (payload->callback) {
    payload->callback();
  }

  free(payload);
  return NULL;
}

void fl_run_after_time_platform(double seconds, void (*callback)(void)) {
  if (!callback || seconds < 0)
    return;
  TimerPayload *payload = (TimerPayload *)malloc(sizeof(TimerPayload));
  if (!payload) {
    return;
  }
  payload->seconds = seconds;
  payload->callback = callback;

  pthread_t thread;
  if (pthread_create(&thread, NULL, TimerThread, payload) != 0) {
    free(payload);
    return;
  }

  pthread_detach(thread);
}

void fl_sleep_platform(double seconds) {
  if (seconds <= 0)
    return;
  struct timespec req;
  req.tv_sec = (time_t)seconds;
  req.tv_nsec = (long)((seconds - (double)req.tv_sec) * 1000000000.0);
  nanosleep(&req, NULL);
}

int fl_get_active_interface_platform(char out[], size_t max_len) {
  static char content[16384];
  if (File_ReadAll("/proc/net/route", content, (long)sizeof(content)) <= 0)
    return 0;
  int found = 0;
  char *cur = content;
  if (*cur) {
    char *nl = strchr(cur, '\n');
    if (nl)
      *nl = '\0';
    cur = nl ? nl + 1 : cur + strlen(cur);
  }
  while (*cur) {
    char *nl = strchr(cur, '\n');
    if (nl)
      *nl = '\0';
    char *line = cur;
    cur = nl ? nl + 1 : cur + strlen(cur);
    char iface[64];
    unsigned long dest, mask;
    if (sscanf(line, "%63s %lx %*s %*s %*s %*s %*s %lx", iface, &dest, &mask) ==
        3) {
      if (dest == 0 && mask == 0) {
        strncpy(out, iface, max_len - 1);
        out[max_len - 1] = '\0';
        found = 1;
        break;
      }
    }
  }
  return found;
}
unsigned long long fl_net_rx_bytes_platform() {
  char want[64];
  if (!fl_get_active_interface_platform(want, sizeof(want)) || !want[0])
    return 0;
  FILE *f = fopen("/proc/net/dev", "r");
  if (!f)
    return 0;
  char buf[512];
  unsigned long long rx_bytes = 0;

  if (fgets(buf, sizeof(buf), f) && fgets(buf, sizeof(buf), f)) {
    while (fgets(buf, sizeof(buf), f)) {
      char name[64];
      unsigned long long bytes = 0;
      if (sscanf(buf, " %63[^:]:%llu", name, &bytes) == 2) {
        char *trimmed = name;
        while (*trimmed == ' ' || *trimmed == '\t')
          trimmed++;
        char *tail = trimmed + strlen(trimmed);
        while (tail > trimmed && (tail[-1] == ' ' || tail[-1] == '\t' ||
                                  tail[-1] == '\n' || tail[-1] == '\r'))
          *--tail = '\0';
        if (strcmp(trimmed, want) == 0) {
          rx_bytes = bytes;
          break;
        }
      }
    }
  }
  fclose(f);
  return rx_bytes;
}

FlNetStatus fl_net_status_platform(void) {
  char iface[32] = {0};
  int found = fl_get_active_interface_platform(iface, sizeof(iface));

  if (!found)
    return FL_NET_OFFLINE;
  char sysfs_path[128];
  snprintf(sysfs_path, sizeof(sysfs_path), "/sys/class/net/%s/carrier", iface);

  FILE *f = fopen(sysfs_path, "r");
  if (!f)
    return FL_NET_OFFLINE;

  char carrier = (char)fgetc(f);
  fclose(f);

  if (carrier != '1')
    return FL_NET_OFFLINE;

  if (strncmp(iface, "en", 2) == 0 || strncmp(iface, "eth", 3) == 0) {
    return FL_NET_ETHERNET;
  }
  if (strncmp(iface, "wl", 2) == 0) {
    return FL_NET_WIFI;
  }
  return FL_NET_OFFLINE;
}

bool fl_dismiss_message_platform(const char *name) {
  Platform_DismissIndefiniteMessage();
  return Platform_DismissMessage(name);
}

int fl_get_pid_platform() { return (int)getpid(); }