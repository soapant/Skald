#include "platform/flapi_platform.h"
#include "core/clipboard_history.h"
#include "wm/wm.h"
#include "deps/log.h"
#include "platform/platform.h"
#include "helpers/helpers_text.h"
#include "platform/draw.h"
#include "platform/ui.h"
#include "platform/x11/env_helpers_x11.h"
#include "platform/x11/desktop_helpers_x11.h"
#include "platform/x11/drawing_x11.h"
#include "platform/x11/audio_x11.h"
#include "helpers/helpers_ui.h"
#include "bindings/vim_state.h"
#include "core/flapi.h"
#include "config.h"
#include "core/prompt_geometry.h"
#include "core/listbox_hover.h"
#include "core/listbox_layout.h"
#include "core/editbox.h"
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include "core/timer.h"
#include "core/remap_manager.h"
#include "core/window_manager.h"
#include "platform/wm_platform.h"
#include "core/modal.h"
#include <ctype.h>
#include <time.h>
#include <pthread.h>
#include <linux/input.h>
#include <glib.h>
#include <pulse/pulseaudio.h>


bool Platform_GetAllFilesFromDir(const char* dir_path, char** list_of_allowed_extensions,
                                        ListOfFiles* list_of_files, char** list_of_names_to_ignore,
                                        bool strip_extension, int max_depth) {
        PROFILE_START(_pt);
PROFILE_START(timer);

    if (!dir_path || !list_of_files) {
        PROFILE_END_VAR(timer, "flapi_x11:Platform_GetAllFilesFromDir");
        return false;
    }

    DIR* dir = opendir(dir_path);
    if (!dir) {
        perror("opendir");
        PROFILE_END_VAR(timer, "flapi_x11:Platform_GetAllFilesFromDir");
        return false;
    }

    struct dirent* entry;
    int entry_count = 0;

    while ((entry = readdir(dir)) != NULL) {
        entry_count++;

        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == '\0' ||
             (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }

        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            bool should_ignore = false;
            if (list_of_names_to_ignore) {
                for (int i = 0; list_of_names_to_ignore[i] != NULL; i++) {
                    if (Platform_StringContainsCaseInsensitive(entry->d_name, list_of_names_to_ignore[i])) {
                        should_ignore = true;
                        break;
                    }
                }
            }
            if (!should_ignore && (max_depth == 0 || max_depth > 1)) {
                Platform_GetAllFilesFromDir(full_path, list_of_allowed_extensions,
                                                   list_of_files, list_of_names_to_ignore, strip_extension,
                                                   max_depth > 0 ? max_depth - 1 : 0);
            }
        } else {
            bool allowed = true;
            if (list_of_allowed_extensions) {
                allowed = false;
                const char* ext = strrchr(entry->d_name, '.');
                if (ext) {
                    ext++;
                    for (int i = 0; list_of_allowed_extensions[i] != NULL; i++) {
                        const char* want = list_of_allowed_extensions[i];
                        if (want && want[0] == '.') want++;
                        if (Platform_StringContainsCaseInsensitive(ext, want)) {
                            allowed = true;
                            break;
                        }
                    }
                }
            }

            if (allowed) {
                if (list_of_files->count >= list_of_files->capacity) {
                    size_t new_capacity = list_of_files->capacity == 0 ? 10 : list_of_files->capacity * 2;

                    FileItem* new_items = realloc(list_of_files->files, new_capacity * sizeof(FileItem));
                    if (!new_items) {
                        closedir(dir);
                        PROFILE_END_VAR(timer, "flapi_x11:Platform_GetAllFilesFromDir");
                        return false;
                    }
                    list_of_files->files    = new_items;
                    list_of_files->capacity = new_capacity;
                }

                FileItem* item = &list_of_files->files[list_of_files->count];

                size_t plen = strlen(full_path);
                if (plen >= sizeof(item->path)) plen = sizeof(item->path) - 1;
                memcpy(item->path, full_path, plen);
                item->path[plen] = '\0';

                char clean_name[256];
                size_t nlen = strlen(entry->d_name);
                if (nlen >= sizeof(clean_name)) nlen = sizeof(clean_name) - 1;
                memcpy(clean_name, entry->d_name, nlen);
                clean_name[nlen] = '\0';

                if (strip_extension) {
                    char* dot = strrchr(clean_name, '.');
                    if (dot) *dot = '\0';
                }

                size_t clen = strlen(clean_name);
                if (clen >= sizeof(item->name)) clen = sizeof(item->name) - 1;
                memcpy(item->name, clean_name, clen);
                item->name[clen] = '\0';

                list_of_files->count++;
            }
        }
    }

    closedir(dir);
    PROFILE_END_VAR(timer, "flapi_x11:Platform_GetAllFilesFromDir");
    { PROFILE_END_VAR(_pt, "Platform_GetAllFilesFromDir"); return true; }
}

static bool platform_exe_in_path(const char* name) {
        PROFILE_START(_pt);
if (!name || !name[0]) { PROFILE_END_VAR(_pt, "platform_exe_in_path"); return false; }
    const char* path = getenv("PATH");
    if (!path || !path[0]) { PROFILE_END_VAR(_pt, "platform_exe_in_path"); return false; }
    const char* p = path;
    while (*p) {
        const char* sep = strchr(p, ':');
        size_t len = sep ? (size_t)(sep - p) : strlen(p);
        char cand[1024];
        if (len == 0) snprintf(cand, sizeof(cand), "./%s", name);
        else snprintf(cand, sizeof(cand), "%.*s/%s", (int)len, p, name);
        if (access(cand, X_OK) == 0) return true;
        if (!sep) break;
        p = sep + 1;
    }
    { PROFILE_END_VAR(_pt, "platform_exe_in_path"); return false; }
}

static bool platform_resolve_desktop_exec(const char* exec_line, const char* desktop_dir,
                                          const char* workdir, char* out, size_t out_size) {
        PROFILE_START(_pt);
if (!exec_line || !out || out_size == 0) { PROFILE_END_VAR(_pt, "platform_resolve_desktop_exec"); return false; }
    const char* p = exec_line;
    while (*p == ' ' || *p == '\t') p++;

    char token[1024];
    size_t t = 0;
    if (*p == '"') {
        p++;
        while (*p) {
            if (*p == '\\' && (p[1] == '"' || p[1] == '\\')) p++;
            else if (*p == '"') { p++; break; }
            if (t + 1 < sizeof(token)) token[t++] = *p;
            p++;
        }
    } else {
        while (*p && *p != ' ' && *p != '\t') {
            if (t + 1 < sizeof(token)) token[t++] = *p;
            p++;
        }
    }
    token[t] = '\0';
    while (*p == ' ' || *p == '\t') p++;
    if (!token[0]) { PROFILE_END_VAR(_pt, "platform_resolve_desktop_exec"); return false; }

    const char* base = (workdir && workdir[0]) ? workdir : desktop_dir;
    char prog[1024];
    if (strchr(token, '/')) {
        if (token[0] == '/') snprintf(prog, sizeof(prog), "%s", token);
        else snprintf(prog, sizeof(prog), "%s/%s", base, token);
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
    for (const char* s = prog; *s && n + 5 < sizeof(q); s++) {
        if (*s == '\'') { memcpy(q + n, "'\\''", 4); n += 4; }
        else q[n++] = *s;
    }
    q[n++] = '\'';
    q[n] = '\0';

    snprintf(out, out_size, "%s%s%s", q, p[0] ? " " : "", p);
    { PROFILE_END_VAR(_pt, "platform_resolve_desktop_exec"); return true; }
}

static int platform_run_status(char* const argv[], const X11SessionEnv* env, bool apply_env) {
        PROFILE_START(_pt);
pid_t pid = fork();
    if (pid < 0) { PROFILE_END_VAR(_pt, "platform_run_status"); return -1; }
    if (pid == 0) {
        if (apply_env && env) Env_ApplyToCurrentProcess(env);
        execvp(argv[0], argv);
        _exit(127);
    }
    int st = 0;
    if (waitpid(pid, &st, 0) < 0) { PROFILE_END_VAR(_pt, "platform_run_status"); return -1; }
    if (!WIFEXITED(st)) { PROFILE_END_VAR(_pt, "platform_run_status"); return -1; }
    { PROFILE_END_VAR(_pt, "platform_run_status"); return WEXITSTATUS(st); }
}

bool fl_launch_with_args_platform(const char* path, const char* args) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    log_info("fl_launch path='%s' args='%s'", path ? path : "null", args ? args : "null");
    if (!path) { PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform"); return false; }
    char expanded[1024];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) { PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform"); return false; }
    log_info("fl_launch expanded='%s'", expanded);
    path = expanded;
    if (strstr(path, ".desktop") != NULL) {
        log_info("fl_launch desktop detected expanded='%s'", expanded);
        bool is_terminal = false;
        char exec_line[1024] = {0};
        char workdir[1024] = {0};
        Desktop_Parse(path, exec_line, workdir, &is_terminal);
        log_info("Desktop_Parse exec='%s' workdir='%s' is_terminal=%d", exec_line, workdir, is_terminal);
        Desktop_CleanExec(exec_line);
        log_info("Desktop_CleanExec exec='%s'", exec_line);
        if(is_terminal && exec_line[0]){
            const char* wd = workdir[0] ? workdir : NULL;
            char extra[2048];
            if (args && args[0]) snprintf(extra, sizeof(extra), "-e sh -c '%s %s; exec %s'", exec_line, args,UsedShell);
            else snprintf(extra, sizeof(extra), "-e sh -c '%s; exec %s'", exec_line,UsedShell);
            if(wd) { PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform"); return Platform_LaunchTerminalAtPath(wd, extra); }
            else { PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform"); return Platform_LaunchTerminalAtPath(Platform_GetRealHome(), extra); }
        }
        if (exec_line[0] && args && args[0]) {
            char combined[2048];
            snprintf(combined, sizeof(combined), "%s %s", exec_line, args);
            strncpy(exec_line, combined, sizeof(exec_line)-1);
            exec_line[sizeof(exec_line)-1] = '\0';
        }
        if (!exec_line[0]) {
            log_warn("desktop '%s' has no usable Exec line", expanded);
            PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform");
            return false;
        }

        char desktop_dir[1024] = {0};
        snprintf(desktop_dir, sizeof(desktop_dir), "%s", expanded);
        char* dslash = strrchr(desktop_dir, '/');
        if (dslash) *dslash = '\0';
        else snprintf(desktop_dir, sizeof(desktop_dir), ".");
        const char* run_dir = workdir[0] ? workdir : desktop_dir;

        X11SessionEnv env; Env_GetSessionEnv(&env);
        char disp_env[256], xauth_env[512], runtime_env[512], wayland_env[512];
        Env_BuildEnvStrings(&env, disp_env, xauth_env, runtime_env, wayland_env);
        bool as_root = Env_IsRunningAsRoot();
        log_info("desktop '%s' exec='%s' workdir='%s' run_dir='%s' as_root=%d", expanded, exec_line, workdir, run_dir, as_root);

        int gio_st;
        if (as_root) {
            char* const av_gio[] = {"sudo","-u",(char*)env.real_user,"env",disp_env,xauth_env,runtime_env,wayland_env,"gio","launch",expanded,NULL};
            gio_st = platform_run_status(av_gio, NULL, false);
        } else {
            char* const av_gio2[] = {"gio","launch",expanded,NULL};
            gio_st = platform_run_status(av_gio2, &env, true);
        }
        log_info("gio launch '%s' exit=%d", expanded, gio_st);
        if (gio_st == 0) {
            PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform");
            return true;
        }

        char cmd[4096];
        if (!platform_resolve_desktop_exec(exec_line, desktop_dir, workdir, cmd, sizeof(cmd))) {
            log_warn("desktop '%s': cannot resolve Exec '%s' via PATH or desktop dir '%s'", expanded, exec_line, desktop_dir);
            PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform");
            return false;
        }
        char launch_cmd[4608];
        snprintf(launch_cmd, sizeof(launch_cmd), "cd '%s' && %s", run_dir, cmd);
        log_info("desktop resolved launch_cmd='%s'", launch_cmd);

        if (as_root) {
            char* const av_run[] = {"sudo","-u",(char*)env.real_user,"env",disp_env,xauth_env,runtime_env,wayland_env,"sh","-c",launch_cmd,NULL};
            if (!Env_DaemonExec(av_run)) {
                PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform");
                return false;
            }
        } else {
            X11SessionEnv env2 = env;
            char* const av_run2[] = {"sh","-c",launch_cmd,NULL};
            if (!Env_DaemonExecWithEnv(av_run2, &env2)) {
                PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform");
                return false;
            }
        }
        PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform");
        return true;
    } else {
        X11SessionEnv env; Env_GetSessionEnv(&env);
        char disp_env[256], xauth_env[512], runtime_env[512];
        Env_BuildEnvStrings(&env, disp_env, xauth_env, runtime_env, NULL);
        struct stat pst;
        bool is_regular_file = (stat(path, &pst) == 0 && S_ISREG(pst.st_mode));
        bool is_executable = (access(path, X_OK) == 0);
        if (is_regular_file && !is_executable) {
            char* const av_open2[] = {"gio","open",path,NULL};
            if (!Env_DaemonExecWithEnv(av_open2, &env)) { PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform"); return false; }
            PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform");
            return true;
        }
        char cmd[2048];
        if (args && args[0] && strcmp(args,"0")!=0) snprintf(cmd, sizeof(cmd), "\"%s\" %s", path, args);
        else snprintf(cmd, sizeof(cmd), "\"%s\"", path);
        if(Env_IsRunningAsRoot()){
            char* const argv[] = {"sudo","-u",(char*)env.real_user,"env",disp_env,xauth_env,runtime_env,"sh","-c",cmd,NULL};
            if (!Env_DaemonExec(argv)) { PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform"); return false; }
            PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform"); return true;
        } else {
            X11SessionEnv env2 = env;
            pid_t pid=fork(); if(pid<0) { PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform"); return false; }
            if(pid==0){
                setsid(); pid_t pid2=fork(); if(pid2<0) _exit(1); if(pid2>0) _exit(0);
                int fd=open("/dev/null",O_RDWR); if(fd>=0){dup2(fd,0);dup2(fd,1);dup2(fd,2); if(fd>2) close(fd);}
                Env_ApplyToCurrentProcess(&env2);
                execl("/bin/sh","sh","-c",cmd,(char*)NULL);
                _exit(127);
            }
            int st=0; waitpid(pid,&st,0); PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform"); return true;
        }
    }
    PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform");
}

bool fl_launch_with_args_as_admin_platform(const char* path, const char* args) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    if (!path) { PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_as_admin_platform"); return false; }
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) { PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_as_admin_platform"); return false; }
    char cmd[2048];
    if (args && args[0]) snprintf(cmd, sizeof(cmd), "%s %s", expanded, args);
    else snprintf(cmd, sizeof(cmd), "%s", expanded);
    pid_t pid = fork();
    if (pid < 0) { PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_as_admin_platform"); return false; }
    if (pid == 0) {
        char *argv1[] = {"pkexec", cmd, NULL};
        execvp("pkexec", argv1);
        char *argv2[] = {"sudo", cmd, NULL};
        execvp("sudo", argv2);
        _exit(1);
    }
    PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_as_admin_platform");
    { PROFILE_END_VAR(_pt, "fl_launch_with_args_as_admin_platform"); return true; }
}

static __thread FlListSpec g_fl_list_spec;
static __thread ListBoxInfo *g_fl_list_lb;
static __thread int g_fl_list_y0, g_fl_list_itemH, g_fl_list_visible, g_fl_list_cur, g_fl_list_h;
static __thread int g_fl_list_bx, g_fl_list_bw, g_fl_list_fw, g_fl_list_fh;
static __thread Display *g_fl_list_dpy;
static __thread XftDraw *g_fl_list_xd;
static __thread Visual *g_fl_list_vis;
static __thread Colormap g_fl_list_cmap;
static __thread FontHandle g_fl_list_font;
static __thread bool g_fl_list_font_owned;

bool fl_listbox_begin_platform(const FlListSpec *spec) {
        PROFILE_START(_pt);
if (!spec) { PROFILE_END_VAR(_pt, "fl_listbox_begin_platform"); return false; }
    ListBoxInfo *lb = spec->lb;
    if (!lb) {
        if (!spec->items || spec->count <= 0) return false;
        return false;
    }
    Platform_GetDrawContext(&g_fl_list_dpy, &g_fl_list_xd, &g_fl_list_vis, &g_fl_list_cmap);
    if (!g_fl_list_dpy || !g_fl_list_xd) { PROFILE_END_VAR(_pt, "fl_listbox_begin_platform"); return false; }
    g_fl_list_spec = *spec;
    g_fl_list_lb = lb;
    g_fl_list_font = Platform_CreateFontOnDisplay(g_fl_list_dpy, FontSize, UsedFont);
    g_fl_list_itemH = spec->item_height ? spec->item_height : x11_font_row_height_of(g_fl_list_font);
    WindowManager* wmgr = WindowManager_Get();
    PromptState *st = NULL;
    if (wmgr) {
        for (int _i = wmgr->popup_count - 1; _i >= 0; _i--) {
            Popup* _p = wmgr->popups[_i];
            if (_p && _p->user_data && _p->type == POPUP_PROMPT) {
                PromptState* _ps = (PromptState*)_p->user_data;
                if (&_ps->listbox == lb) { st = _ps; break; }
            }
        }
        if (!st && wmgr->popup_count > 0) {
            Popup* top = WindowManager_GetTopPopup(wmgr);
            if (top && top->user_data && top->type == POPUP_PROMPT) st = (PromptState*)top->user_data;
        }
    }
    ListboxLayout lay;
    Listbox_ComputeLayout(spec, x11_font_list_top_of(g_fl_list_font), g_fl_list_itemH,
                          st ? st->width : 0, st ? st->height : 0, lb->populated_count, &lay);
    g_fl_list_y0 = lay.y0;
    g_fl_list_bx = lay.bx;
    g_fl_list_bw = lay.bw;
    g_fl_list_fw = lay.fw;
    g_fl_list_fh = lay.fh;
    g_fl_list_h = lay.list_h;
    g_fl_list_visible = lay.visible;
    UI_LB_SetVisibleCount(lay.visible);
    g_fl_list_cur = 0;
    g_fl_list_font = Platform_CreateFontOnDisplay(g_fl_list_dpy, FontSize, UsedFont);
    g_fl_list_font_owned = true;
    ListBox_ApplyPendingScroll(g_fl_list_lb, lay.visible);
    ListBox_SetGeometry(g_fl_list_y0, g_fl_list_itemH, lay.visible);
    ListBox_HoverFocus(g_fl_list_lb, g_fl_list_y0, g_fl_list_itemH, g_fl_list_lb->view_offset, lay.visible);
    { PROFILE_END_VAR(_pt, "fl_listbox_begin_platform"); return true; }
}

bool fl_listbox_next_platform(FlListItemCtx *out) {
        PROFILE_START(_pt);
if (!out || !g_fl_list_lb) { PROFILE_END_VAR(_pt, "fl_listbox_next_platform"); return false; }
    if (g_fl_list_cur >= g_fl_list_visible) { PROFILE_END_VAR(_pt, "fl_listbox_next_platform"); return false; }
    int idx = g_fl_list_lb->view_offset + g_fl_list_cur;
    if (idx < 0 || idx >= g_fl_list_lb->populated_count) { PROFILE_END_VAR(_pt, "fl_listbox_next_platform"); return false; }
    int y = g_fl_list_y0 + g_fl_list_cur * g_fl_list_itemH;
    if (g_fl_list_cur == 0) Platform_DrawPromptSeparator(g_fl_list_y0, g_fl_list_fw);
    bool sel = (idx == g_fl_list_lb->selected_index);
    if (RoundedCorners && CornerRadius>0) {
        int R=PROMPT_SEL_RADIUS; int t=PROMPT_SEL_INSET;
        int bx = g_fl_list_bx, bw = g_fl_list_bw;
        if (sel) {
            x11_fill_rounded_rect(g_fl_list_dpy, g_fl_list_xd, g_fl_list_vis, g_fl_list_cmap, rgb_to_hex(COLOR_SELECTED_TEXT), 0xFFFF, bx+t, y+PROMPT_SEL_TOP_INSET+t, bw-2*t, g_fl_list_itemH-2-2*t, R-1, g_fl_list_fw, g_fl_list_fh);
        }
    } else {
        PromptState tmp = {0};
        tmp.width = g_fl_list_fw;
        tmp.height = g_fl_list_y0 + g_fl_list_visible * g_fl_list_itemH;
        Platform_DrawListBackground(&tmp, g_fl_list_dpy, g_fl_list_xd, g_fl_list_vis, g_fl_list_cmap, y, g_fl_list_itemH, sel);
    }
    out->idx = idx;
    out->selected = sel;
    out->x = g_fl_list_spec.x;
    out->y = y;
    out->w = g_fl_list_spec.w;
    out->h = g_fl_list_itemH;
    out->text = g_fl_list_lb->populated_cache[idx];
    g_fl_list_cur++;
    { PROFILE_END_VAR(_pt, "fl_listbox_next_platform"); return true; }
}

void fl_listbox_end_platform(void) {
        PROFILE_START(_pt);
    if (PromptListBottomMargin() > 0 && g_fl_list_y0 >= 0) {
        Platform_DrawPromptSeparator(g_fl_list_y0 + g_fl_list_h, g_fl_list_fw);
    }
    if (g_fl_list_font_owned) { Platform_DestroyFont(g_fl_list_font); g_fl_list_font_owned = false; }
    g_fl_list_lb = NULL;
    g_fl_list_visible = 0;
    g_fl_list_cur = 0;
}

int fl_listbox_get_count(void) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "fl_listbox_get_count"); return g_fl_list_lb ? g_fl_list_lb->populated_count : 0; }
}

unsigned long fl_listbox_get_current_standard_color(void) {
        PROFILE_START(_pt);
if (!g_fl_list_lb || g_fl_list_cur == 0) { PROFILE_END_VAR(_pt, "fl_listbox_get_current_standard_color"); return 0; }
    int idx = g_fl_list_lb->view_offset + g_fl_list_cur - 1;
    if (idx < 0 || idx >= g_fl_list_lb->populated_count) { PROFILE_END_VAR(_pt, "fl_listbox_get_current_standard_color"); return 0; }
    unsigned long ncol, bcol, pcol;
    Platform_GetItemColors(idx, g_fl_list_lb->populated_count, &ncol, &bcol, &pcol);
    { PROFILE_END_VAR(_pt, "fl_listbox_get_current_standard_color"); return pcol; }
}

char* fl_search_file_in_dir_platform(const char* directory, const char* search_string,char* output_path) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    if (!directory || !search_string || strlen(search_string) == 0) { PROFILE_END_VAR(timer, "flapi_x11:fl_search_file_in_dir_platform"); return NULL; }
    DIR* dir = opendir(directory);
    if (!dir) { PROFILE_END_VAR(timer, "flapi_x11:fl_search_file_in_dir_platform"); return NULL; }
    static char found_path[MAX_PATH];
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0]=='.' && (entry->d_name[1]=='\0' || (entry->d_name[1]=='.' && entry->d_name[2]=='\0'))) continue;
        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", directory, entry->d_name);
        struct stat st;
        if (stat(full_path, &st)!=0) continue;
        if (S_ISDIR(st.st_mode)) continue;
        if (Platform_StringContainsCaseInsensitive(entry->d_name, search_string)) {
            snprintf(found_path, sizeof(found_path), "%s", full_path);
            closedir(dir);
            PROFILE_END_VAR(timer, "flapi_x11:fl_search_file_in_dir_platform");
            return found_path;
        }
    }
    closedir(dir);
    PROFILE_END_VAR(timer, "flapi_x11:fl_search_file_in_dir_platform");
    { PROFILE_END_VAR(_pt, "fl_search_file_in_dir_platform"); return NULL; }
}

bool fl_place_editbox_platform(const FlEditSpec *spec) {
        PROFILE_START(_pt);
if (!spec) { PROFILE_END_VAR(_pt, "fl_place_editbox_platform"); return false; }
    Display *dpy; XftDraw *xd; Visual *vis; Colormap cmap;
    Platform_GetDrawContext(&dpy,&xd,&vis,&cmap);
    if (!dpy || !xd) { PROFILE_END_VAR(_pt, "fl_place_editbox_platform"); return false; }
    WindowManager* wm = WindowManager_Get();
    PromptState *state = NULL;
    if (wm) {
        Popup* top = WindowManager_GetTopPopup(wm);
        if (top && top->user_data && top->type == POPUP_PROMPT) state = (PromptState*)top->user_data;
    }
    const char *buf = spec->buffer;
    if (!buf) {
        if (state) buf = state->filter;
        else buf = "";
    }
    int x = spec->x;
    int w = spec->w;
    int h = spec->h;
    if (w <= 0) w = state ? state->width : PROMPT_CUSTOM_FALLBACK_W;
    if (h <= 0) h = PromptCustomEditHeight();
    FontHandle fh = spec->font;
    bool needDestroy = false;
    if (!FH_TO_XFT(fh)) {
        fh = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
        needDestroy = true;
    }
    XftFont *xf = FH_TO_XFT(fh);
    Platform_SetDrawContext(dpy, xd, vis, cmap);

    int text_len = (int)strlen(buf);
    int cursor = state ? state->cursor : text_len;
    if (cursor < 0) cursor = 0;
    if (cursor > text_len) cursor = text_len;

    FlEditboxX11Metrics metrics = { dpy, xf };
    int cursor_x = 0;
    int text_w = 0;
    if (xf) {
        XGlyphInfo ce;
        XftTextExtentsUtf8(dpy, xf, (FcChar8*)buf, cursor, &ce);
        cursor_x = ce.xOff;
        XGlyphInfo te;
        XftTextExtentsUtf8(dpy, xf, (FcChar8*)buf, text_len, &te);
        text_w = te.xOff;
    }

    int y_raw = (spec->y == 0) ? PROMPT_EDIT_INSET : spec->y;

    EditboxInit ebi;
    memset(&ebi, 0, sizeof(ebi));
    ebi.state = state;
    ebi.text = buf;
    ebi.text_len = text_len;
    ebi.cursor = cursor;
    ebi.insert_mode = IsInsertMode() || !EnableVimMode;
    ebi.x = x;
    ebi.y = y_raw;
    ebi.w = w;
    ebi.cursor_x = cursor_x;
    ebi.text_w = text_w;
    ebi.ascent = xf ? xf->ascent : 0;
    ebi.descent = xf ? xf->descent : 0;
    ebi.text_color = rgb_to_hex(COLOR_TEXT);
    ebi.font = fh;

    FlEditboxSpec eb;
    Editbox_BuildSpec(&eb, &ebi);
    eb.measure = x11_editbox_measure;
    eb.measure_user = &metrics;

    Editbox_Render(&eb, NULL);
    if (state) state->scroll_offset = eb.scroll;

    if (state) {
        if (WindowTitleText && state->window_title) {
            unsigned long title_col = 0;
            eval_window_title_color(state, &title_col);
            Platform_SetDrawContext(dpy, xd, vis, cmap);
            Platform_DrawTextRaw(PROMPT_FOOTER_X, state->height - PROMPT_FOOTER_Y_FROM_BOTTOM, state->window_title, title_col, fh, "LEFT", PROMPT_FOOTER_PAD, FontSize);
        }
        if (VimModeText) {
            Platform_SetDrawContext(dpy, xd, vis, cmap);
            Platform_DrawTextRaw(PROMPT_FOOTER_X, state->height - PROMPT_FOOTER_Y_FROM_BOTTOM, g_vim_mode, VimModeTextColor(), fh, "RIGHT", PROMPT_FOOTER_PAD, FontSize);
        }
    }
    if (needDestroy) Platform_DestroyFont(fh);
    { PROFILE_END_VAR(_pt, "fl_place_editbox_platform"); return true; }
}

bool fl_type_text_platform(const char *s){
  PROFILE_START(_pt);
if(!s) { PROFILE_END_VAR(_pt, "fl_type_text_platform"); return false; }
 char* old = Platform_GetClipboardEntry();
 Platform_CopyToClipboard(s);
 Platform_EmitKey(VK_LCONTROL,1);
 Platform_EmitKey('V',1); Platform_EmitKey('V',0);
 Platform_EmitKey(VK_LCONTROL,0);
 WindowManager* wm = WindowManager_Get();
 for (int i=0;i<40;i++){ if (wm) Platform_WM_ProcessEvents(wm); usleep(5000); }
 if (old) { Platform_CopyToClipboard(old); free(old); }
 { PROFILE_END_VAR(_pt, "fl_type_text_platform"); return true; }
}

typedef struct {
    double seconds;
    void (*callback)(void);
} TimerPayload;

static void* TimerThread(void* arg) {
        PROFILE_START(_pt);
TimerPayload* payload = (TimerPayload*)arg;
    if (!payload) { PROFILE_END_VAR(_pt, "TimerThread"); return NULL; }

    useconds_t micros = (useconds_t)(payload->seconds * 1000000.0);
    usleep(micros);

    if (payload->callback) {
        payload->callback();
    }

    free(payload);
    { PROFILE_END_VAR(_pt, "TimerThread"); return NULL; }
}

void fl_run_after_time_platform(double seconds, void (*callback)(void)) {
        PROFILE_START(_pt);
if (!callback || seconds < 0) { PROFILE_END_VAR(_pt, "fl_run_after_time_platform"); return ; }

    TimerPayload* payload = (TimerPayload*)malloc(sizeof(TimerPayload));
    if (!payload) { PROFILE_END_VAR(_pt, "fl_run_after_time_platform"); return ; }
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
        PROFILE_START(_pt);
if (seconds <= 0) { PROFILE_END_VAR(_pt, "fl_sleep_platform"); return ; }
    struct timespec req;
    req.tv_sec = (time_t)seconds;
    req.tv_nsec = (long)((seconds - (double)req.tv_sec) * 1000000000.0);
    nanosleep(&req, NULL);
}

struct MemoryStruct {
    char *memory;
    size_t size;
};

int fl_get_active_interface_platform(char out[], size_t max_len) {
        PROFILE_START(_pt);
static char content[16384];
    if (File_ReadAll("/proc/net/route", content, (long)sizeof(content)) <= 0) { PROFILE_END_VAR(_pt, "fl_get_active_interface_platform"); return 0; }
    int found = 0;
    char* cur = content;
    if (*cur) {
        char* nl = strchr(cur, '\n');
        if (nl) *nl = '\0';
        cur = nl ? nl + 1 : cur + strlen(cur);
    }
    while (*cur) {
        char* nl = strchr(cur, '\n');
        if (nl) *nl = '\0';
        char* line = cur;
        cur = nl ? nl + 1 : cur + strlen(cur);
        char iface[64];
        unsigned long dest, mask;
        if (sscanf(line, "%63s %lx %*s %*s %*s %*s %*s %lx", iface, &dest, &mask) == 3) {
            if (dest == 0 && mask == 0) {
                strncpy(out, iface, max_len - 1);
                out[max_len-1] = '\0';
                found = 1;
                break;
            }
        }
    }
    { PROFILE_END_VAR(_pt, "fl_get_active_interface_platform"); return found; }
}
unsigned long long fl_net_rx_bytes_platform() {
        PROFILE_START(_pt);
char want[64];
    if (!fl_get_active_interface_platform(want, sizeof(want)) || !want[0]) { PROFILE_END_VAR(_pt, "fl_net_rx_bytes_platform"); return 0; }
    FILE *f = fopen("/proc/net/dev", "r");
    if (!f) { PROFILE_END_VAR(_pt, "fl_net_rx_bytes_platform"); return 0; }
    char buf[512];
    unsigned long long rx_bytes = 0;

    if (fgets(buf, sizeof(buf), f) && fgets(buf, sizeof(buf), f)) {
        while (fgets(buf, sizeof(buf), f)) {
            char name[64];
            unsigned long long bytes = 0;
            if (sscanf(buf, " %63[^:]:%llu", name, &bytes) == 2) {
                char *trimmed = name;
                while(*trimmed == ' ' || *trimmed == '\t') trimmed++;
                char *tail = trimmed + strlen(trimmed);
                while (tail > trimmed && (tail[-1] == ' ' || tail[-1] == '\t' || tail[-1] == '\n' || tail[-1] == '\r')) *--tail = '\0';
                if (strcmp(trimmed, want) == 0) {
                    rx_bytes = bytes;
                    break;
                }
            }
        }
    }
    fclose(f);
    { PROFILE_END_VAR(_pt, "fl_net_rx_bytes_platform"); return rx_bytes; }
}

FlNetStatus fl_net_status_platform(void) {
        PROFILE_START(_pt);
char iface[32] = {0};
    int found = fl_get_active_interface_platform(iface, sizeof(iface));

    if (!found) { PROFILE_END_VAR(_pt, "fl_net_status_platform"); return FL_NET_OFFLINE; }
    char sysfs_path[128];
    snprintf(sysfs_path, sizeof(sysfs_path), "/sys/class/net/%s/carrier", iface);

    FILE* f = fopen(sysfs_path, "r");
    if (!f) { PROFILE_END_VAR(_pt, "fl_net_status_platform"); return FL_NET_OFFLINE; }

    char carrier = (char)fgetc(f);
    fclose(f);

    if (carrier != '1') { PROFILE_END_VAR(_pt, "fl_net_status_platform"); return FL_NET_OFFLINE; }

    if (strncmp(iface, "en", 2) == 0 || strncmp(iface, "eth", 3) == 0) {
        return FL_NET_ETHERNET;
    }
    if (strncmp(iface, "wl", 2) == 0) {
        return FL_NET_WIFI;
    }
    { PROFILE_END_VAR(_pt, "fl_net_status_platform"); return FL_NET_OFFLINE; }
}

bool fl_dismiss_message_platform(const char* name) {
    Platform_DismissIndefiniteMessage();
    return Platform_DismissMessage(name);
}

int fl_get_pid_platform() {
  return (int)getpid();
}
