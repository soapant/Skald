#include "platform/flapi_platform.h"
#include "core/clipboard_history.h"
#include "wm/wm.h"
#include "deps/log.h"
#include "platform/platform.h"
#include "platform/draw.h"
#include "platform/x11/env_helpers_x11.h"
#include "platform/x11/desktop_helpers_x11.h"
#include "platform/x11/drawing_x11.h"
#include "platform/x11/audio_x11.h"
#include "helpers/helpers_ui.h"
#include "config.h"
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
#include <curl/curl.h>
#include "core/timer.h"
#include "core/remap_manager.h"
#include "core/window_manager.h"
#include "core/modal.h"
#include <ctype.h>
#include <time.h>
#include <pthread.h>
#include <linux/input.h>
#include <glib.h>
#include <gio/gio.h>


bool Platform_GetAllFilesFromDir(const char* dir_path, char** list_of_allowed_extensions,
                                        ListOfFiles* list_of_files, char** list_of_names_to_ignore) {
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
            if (!should_ignore) {
                Platform_GetAllFilesFromDir(full_path, list_of_allowed_extensions,
                                                   list_of_files, list_of_names_to_ignore);
            }
        } else {
            bool allowed = true;
            if (list_of_allowed_extensions) {
                allowed = false;
                const char* ext = strrchr(entry->d_name, '.');
                if (ext) {
                    ext++;
                    for (int i = 0; list_of_allowed_extensions[i] != NULL; i++) {
                        if (Platform_StringContainsCaseInsensitive(ext, list_of_allowed_extensions[i])) {
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

                char* dot = strrchr(clean_name, '.');
                if (dot) *dot = '\0';

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
    return true;
}

bool fl_launch_with_args_platform(const char* path, const char* args) {
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
        }
        X11SessionEnv env; Env_GetSessionEnv(&env);
        char app_id[256]; Desktop_GetAppId(expanded, app_id);
        log_info("gtk-launch try app_id='%s' env user='%s' display='%s'", app_id, env.real_user, env.display);
        char disp_env[256], xauth_env[512], runtime_env[512], wayland_env[512];
        Env_BuildEnvStrings(&env, disp_env, xauth_env, runtime_env, wayland_env);
        pid_t pid = fork();
        if (pid==0){
            setsid(); pid_t pid2=fork(); if(pid2<0) _exit(1); if(pid2>0) _exit(0);
            int fd=open("/dev/null",O_RDWR); if(fd>=0){dup2(fd,0);dup2(fd,1);dup2(fd,2); if(fd>2) close(fd);}
            if(Env_IsRunningAsRoot()){
                char* const argv_sysd[] = {"sudo","-u",(char*)env.real_user,"env",disp_env,xauth_env,runtime_env,wayland_env,"systemd-run","--user","--scope","gtk-launch",app_id,NULL};
                execvp(argv_sysd[0], argv_sysd);
                char* const argv_fallback[] = {"sudo","-u",(char*)env.real_user,"env",disp_env,xauth_env,runtime_env,wayland_env,"gtk-launch",app_id,NULL};
                execvp(argv_fallback[0], argv_fallback);
            } else {
                Env_ApplyToCurrentProcess(&env);
                char* const argv_sysd2[] = {"systemd-run","--user","--scope","gtk-launch",app_id,NULL};
                execvp(argv_sysd2[0], argv_sysd2);
                char* const argv_fallback2[] = {"gtk-launch",app_id,NULL};
                execvp(argv_fallback2[0], argv_fallback2);
            }
            _exit(127);
        } else if(pid>0){ int st=0; waitpid(pid,&st,0); log_info("gtk-launch pid %d exit %d WIFEXITED=%d", pid, WIFEXITED(st)?WEXITSTATUS(st):-1, WIFEXITED(st)); if(WIFEXITED(st) && WEXITSTATUS(st)!=0 && exec_line[0]){ log_info("gtk-launch failed, fallback exec_line='%s'", exec_line);
                if(is_terminal){
                    char extra2[2048]; snprintf(extra2, sizeof(extra2), "-e sh -c '%s; exec'", exec_line,UsedShell);
                    PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform");
                    return Platform_LaunchTerminalAtPath(Platform_GetRealHome(), extra2);
                } else {
                    pid_t pid3=fork();
                    if(pid3==0){
                        setsid(); pid_t p2=fork(); if(p2<0) _exit(1); if(p2>0) _exit(0);
                        int fd2=open("/dev/null",O_RDWR); if(fd2>=0){dup2(fd2,0);dup2(fd2,1);dup2(fd2,2); if(fd2>2) close(fd2);}
                        if(Env_IsRunningAsRoot()){
                            char* const av2[]={"sudo","-u",(char*)env.real_user,"env",disp_env,xauth_env,runtime_env,wayland_env,"sh","-c",exec_line,NULL};
                            execvp(av2[0],av2);
                        } else {
                            Env_ApplyToCurrentProcess(&env);
                            execl("/bin/sh","sh","-c",exec_line,(char*)NULL);
                        }
                        _exit(127);
                    } else if(pid3>0){ int st2=0; waitpid(pid3,&st2,0); log_info("exec_line pid3 %d exit %d", pid3, WIFEXITED(st2)?WEXITSTATUS(st2):-1); if (WIFEXITED(st2) && WEXITSTATUS(st2)==0) { PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform"); return true; }
                        log_info("exec_line failed, dir fallback try");
                        char desktop_dir[1024]; strncpy(desktop_dir, expanded, sizeof(desktop_dir)-1); desktop_dir[sizeof(desktop_dir)-1]='\0'; char* slash = strrchr(desktop_dir, '/'); if (slash) *slash='\0'; else strcpy(desktop_dir, ".");
                        char dir_candidate[1024]; snprintf(dir_candidate, sizeof(dir_candidate), "%s/%s", desktop_dir, app_id);
                        char dir_cmd[2048];
                        if (access(dir_candidate, X_OK)==0) snprintf(dir_cmd, sizeof(dir_cmd), "cd \"%s\" && ./%s &", desktop_dir, app_id);
                        else snprintf(dir_cmd, sizeof(dir_cmd), "cd \"%s\" && ./%s &", desktop_dir, app_id);
                        log_info("dir fallback desktop_dir='%s' app_id='%s' candidate='%s' access_X=%d access_F=%d cmd='%s'", desktop_dir, app_id, dir_candidate, access(dir_candidate,X_OK), access(dir_candidate,F_OK), dir_cmd);
                        bool dir_exists = (access(dir_candidate, X_OK)==0) || (access(dir_candidate, F_OK)==0);
                        if (dir_exists) {
                            log_info("dir fallback launching '%s'", dir_cmd);
                            X11SessionEnv env3 = env;
                            char disp3[256], xauth3[512], runtime3[512];
                            Env_BuildEnvStrings(&env3, disp3, xauth3, runtime3, NULL);
                            if (Env_IsRunningAsRoot()) {
                                char* const argv3[] = {"sudo","-u",(char*)env3.real_user,"env",disp3,xauth3,runtime3,"sh","-c",dir_cmd,NULL};
                                Env_DaemonExec(argv3);
                            } else {
                                char* const argv3[] = {"sh","-c",dir_cmd,NULL};
                                Env_DaemonExecWithEnv(argv3, &env3);
                            }
                            PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform"); return true;
                        }
                        PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform"); return false;
                    }
                }
            }
            if (strstr(expanded, "/miscellaneous/") != NULL) {
                char desktop_dir2[1024]; strncpy(desktop_dir2, expanded, sizeof(desktop_dir2)-1); desktop_dir2[sizeof(desktop_dir2)-1]='\0'; char* slash2 = strrchr(desktop_dir2, '/'); if (slash2) *slash2='\0'; else strcpy(desktop_dir2, ".");
                char dir_candidate2[1024]; snprintf(dir_candidate2, sizeof(dir_candidate2), "%s/%s", desktop_dir2, app_id);
                if (access(dir_candidate2, X_OK)==0) {
                    log_info("miscellaneous fallback dir_candidate='%s'", dir_candidate2);
                    char dir_cmd2[2048]; snprintf(dir_cmd2, sizeof(dir_cmd2), "cd \"%s\" && ./%s &", desktop_dir2, app_id);
                    X11SessionEnv env4 = env;
                    char disp4[256], xauth4[512], runtime4[512];
                    Env_BuildEnvStrings(&env4, disp4, xauth4, runtime4, NULL);
                    if (Env_IsRunningAsRoot()) {
                        char* const argv4[] = {"sudo","-u",(char*)env4.real_user,"env",disp4,xauth4,runtime4,"sh","-c",dir_cmd2,NULL};
                        Env_DaemonExec(argv4);
                    } else {
                        char* const argv4[] = {"sh","-c",dir_cmd2,NULL};
                        Env_DaemonExecWithEnv(argv4, &env4);
                    }
                    PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform"); return true;
                }
            }
            PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform");
            return true;
        }
        PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform");
        return false;
    } else {
        char cmd[2048];
        if (args && args[0] && strcmp(args,"0")!=0) snprintf(cmd, sizeof(cmd), "\"%s\" %s", path, args);
        else snprintf(cmd, sizeof(cmd), "\"%s\"", path);
        X11SessionEnv env; Env_GetSessionEnv(&env);
        char disp_env[256], xauth_env[512], runtime_env[512];
        Env_BuildEnvStrings(&env, disp_env, xauth_env, runtime_env, NULL);
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
    return true;
}

static FlListSpec g_fl_list_spec;
static ListBoxInfo *g_fl_list_lb;
static int g_fl_list_y0, g_fl_list_itemH, g_fl_list_visible, g_fl_list_cur;
static int g_fl_list_bx, g_fl_list_bw, g_fl_list_fw;
static Display *g_fl_list_dpy;
static XftDraw *g_fl_list_xd;
static Visual *g_fl_list_vis;
static Colormap g_fl_list_cmap;
static FontHandle g_fl_list_font;
static bool g_fl_list_font_owned;

bool fl_listbox_begin_platform(const FlListSpec *spec) {
    if (!spec) return false;
    ListBoxInfo *lb = spec->lb;
    if (!lb) {
        if (!spec->items || spec->count <= 0) return false;
        return false;
    }
    if (!lb->populated_count) return false;
    Platform_GetDrawContext(&g_fl_list_dpy, &g_fl_list_xd, &g_fl_list_vis, &g_fl_list_cmap);
    if (!g_fl_list_dpy || !g_fl_list_xd) return false;
    g_fl_list_spec = *spec;
    g_fl_list_lb = lb;
    g_fl_list_font = Platform_CreateFontOnDisplay(g_fl_list_dpy, FontSize, UsedFont);
    g_fl_list_itemH = spec->item_height ? spec->item_height : x11_font_row_height_of(g_fl_list_font);
    g_fl_list_y0 = spec->y ? spec->y : x11_font_list_top_of(g_fl_list_font);
    int yOff = (WindowTitleText || VimModeText) ? 40 : 0;
    int list_h;
    PromptState *st = Platform_GetPreRenderStatePtr();
    if (!st) {
        WindowManager* wmgr = WindowManager_Get();
        if (wmgr) {
            Popup* top = WindowManager_GetTopPopup(wmgr);
            if (top && top->user_data && top->type == POPUP_PROMPT) st = (PromptState*)top->user_data;
        }
    }
    if (spec->h) list_h = spec->h;
    else list_h = st ? (st->height - g_fl_list_y0 - yOff) : 400;
    if (!spec->x && !spec->w) {
        g_fl_list_bx = 4;
        g_fl_list_bw = (st ? st->width : 880) - 8;
        g_fl_list_fw = st ? st->width : 900;
    } else {
        g_fl_list_bx = spec->x + 1;
        g_fl_list_bw = (spec->w ? spec->w : 650) - 2;
        g_fl_list_fw = spec->w ? spec->w : 900;
    }
    int visible = list_h / g_fl_list_itemH;
    if (visible < 1) visible = 1;
    if (visible > lb->populated_count) visible = lb->populated_count;
    if (lb->selected_index >= 0) {
        if (lb->selected_index < lb->view_offset + PaddingForScrolling)
            lb->view_offset = lb->selected_index - PaddingForScrolling;
        else if (lb->selected_index >= lb->view_offset + visible - PaddingForScrolling)
            lb->view_offset = lb->selected_index - visible + PaddingForScrolling + 1;
    }
    if (lb->view_offset < 0) lb->view_offset = 0;
    if (lb->view_offset + visible > lb->populated_count)
        lb->view_offset = lb->populated_count - visible;
    if (lb->view_offset < 0) lb->view_offset = 0;
    g_fl_list_visible = visible;
    g_fl_list_cur = 0;
    g_fl_list_font = Platform_CreateFontOnDisplay(g_fl_list_dpy, FontSize, UsedFont);
    g_fl_list_font_owned = true;
    return true;
}

bool fl_listbox_next_platform(FlListItemCtx *out) {
    if (!out || !g_fl_list_lb) return false;
    if (g_fl_list_cur >= g_fl_list_visible) return false;
    int idx = g_fl_list_lb->view_offset + g_fl_list_cur;
    if (idx < 0 || idx >= g_fl_list_lb->populated_count) return false;
    int y = g_fl_list_y0 + g_fl_list_cur * g_fl_list_itemH;
    bool sel = (idx == g_fl_list_lb->selected_index);
    if (RoundedCorners && CornerRadius>0) {
        int R=6; int t=1;
        int bx = g_fl_list_bx, bw = g_fl_list_bw;
        if (sel) {
            x11_fill_rounded_rect(g_fl_list_dpy, g_fl_list_xd, g_fl_list_vis, g_fl_list_cmap, rgb_to_hex(COLOR_SELECTED_TEXT), 0xFFFF, bx+t, y+1+t, bw-2*t, g_fl_list_itemH-2-2*t, R-1);
        } else if (!EnableShaders || ShaderPreset==0) {
            x11_fill_rounded_rect(g_fl_list_dpy, g_fl_list_xd, g_fl_list_vis, g_fl_list_cmap, rgb_to_hex(COLOR_BG), 0xFFFF, bx+t, y+1+t, bw-2*t, g_fl_list_itemH-2-2*t, R-1);
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
    return true;
}

void fl_listbox_end_platform(void) {
    if (g_fl_list_font_owned) { Platform_DestroyFont(g_fl_list_font); g_fl_list_font_owned = false; }
    g_fl_list_lb = NULL;
    g_fl_list_visible = 0;
    g_fl_list_cur = 0;
}

int fl_listbox_get_count(void) {
    return g_fl_list_lb ? g_fl_list_lb->populated_count : 0;
}

unsigned long fl_listbox_get_current_standard_color(void) {
    if (!g_fl_list_lb || g_fl_list_cur == 0) return 0;
    int idx = g_fl_list_lb->view_offset + g_fl_list_cur - 1;
    if (idx < 0 || idx >= g_fl_list_lb->populated_count) return 0;
    bool sel = (idx == g_fl_list_lb->selected_index);
    if (sel) return rgb_to_hex(COLOR_TEXT);
    unsigned long ncol, bcol, pcol;
    Platform_GetItemColors(idx, g_fl_list_lb->populated_count, &ncol, &bcol, &pcol);
    return pcol;
}

char* fl_search_file_in_dir_platform(const char* directory, const char* search_string,char* output_path) {
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
    return NULL;
}
void fl_toggle_play_pause_platform(void) {
    GError *error = NULL;
    GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (!conn) { g_error_free(error); return; }

    gchar* bus_name = FindPlayerBusName(conn);
    if (!bus_name) { g_object_unref(conn); return; }

    GDBusProxy *proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE, NULL,
        bus_name, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", NULL, &error);
    
    if (proxy) {
        g_dbus_proxy_call_sync(proxy, "PlayPause", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
        g_object_unref(proxy);
    }

    if (error) g_error_free(error);
    g_free(bus_name);
    g_object_unref(conn);
}

int fl_get_current_volume_platform(void) {
    char* output = Platform_RunTerminalCommand("wpctl get-volume @DEFAULT_AUDIO_SINK@ 2>/dev/null | awk '{print $2}'", false);
    
    if (output == NULL || strlen(output) == 0) {
        return 0;
    }
    
    return (int)(atof(output) * 100);
}

bool fl_set_volume_platform(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "wpctl set-volume @DEFAULT_AUDIO_SINK@ %d%% >/dev/null 2>&1", volume);

    Platform_RunTerminalCommand(cmd, false);
    return true;
}

static bool g_edit_placed = false;
void fl_editbox_reset_placed(void) { g_edit_placed = false; }
bool fl_editbox_was_placed(void) { return g_edit_placed; }
bool fl_place_editbox_platform(const FlEditSpec *spec) {
    if (!spec) return false;
    g_edit_placed = true;
    Display *dpy; XftDraw *xd; Visual *vis; Colormap cmap;
    Platform_GetDrawContext(&dpy,&xd,&vis,&cmap);
    if (!dpy || !xd) return false;
    PromptState *state = Platform_GetPreRenderStatePtr();
    if (!state) {
        WindowManager* wm = WindowManager_Get();
        if (wm) {
            Popup* top = WindowManager_GetTopPopup(wm);
            if (top && top->user_data && top->type == POPUP_PROMPT) state = (PromptState*)top->user_data;
        }
    }
    const char *buf = spec->buffer;
    if (!buf) {
        if (state) buf = state->filter;
        else buf = "";
    }
    int x = spec->x;
    int y = spec->y;
    int w = spec->w;
    int h = spec->h;
    if (w <= 0) w = state ? state->width : 880;
    if (h <= 0) h = FontSize + 28;
    FontHandle fh = spec->font;
    bool needDestroy = false;
    if (!FH_TO_XFT(fh)) {
        fh = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
        needDestroy = true;
    }
    XftFont *xf = FH_TO_XFT(fh);
    unsigned long text_hex = spec->color ? rgb_to_hex(spec->color) : rgb_to_hex(COLOR_TEXT);
    int text_len = (int)strlen(buf);
    int cursor = state ? state->cursor : text_len;
    if (cursor < 0) cursor = 0;
    if (cursor > text_len) cursor = text_len;
    int cursor_x = 0;
    int scroll = 0;
    if (xf) {
        scroll = x11_update_edit_scroll_offset(dpy,xf,buf,text_len,cursor,w, state ? state->scroll_offset : 0, &cursor_x);
        if (state) state->scroll_offset = scroll;
    }
    int draw_x = x + 14 - scroll;
    int textH = FontSize;
    int y_raw;
    int cursor_y;
    if (spec->y == 0) {
        y_raw = 14;
        cursor_y = x11_font_cursor_y_of(fh, 20);
    }
    Platform_SetDrawContext(dpy, xd, vis, cmap);
    if (text_len > 0) {
        Platform_DrawTextRaw(draw_x, y_raw, (char*)buf, text_hex, fh, "", 0, 0);
    }
    int cursor_draw_x = x + 14 + cursor_x - scroll;
    if (cursor_draw_x < x + 2) cursor_draw_x = x + 2;
    if (cursor_draw_x > x + w - 4) cursor_draw_x = x + w - 4;
    if (IsInsertMode()) {
        Platform_DrawRectRaw(cursor_draw_x,cursor_y,2,textH,rgb_to_hex(COLOR_CURSOR));
    } else {
        Platform_DrawRectRaw(cursor_draw_x,cursor_y,12,textH,rgb_to_hex(COLOR_CURSOR));
    }
    if (state) {
        if (WindowTitleText && state->window_title) {
            unsigned long title_col = 0;
            eval_window_title_color(state, &title_col);
            Platform_SetDrawContext(dpy, xd, vis, cmap);
            Platform_DrawTextRaw(5, state->height - 40, state->window_title, title_col, fh, "LEFT", 5, FontSize);
        }
        if (VimModeText) {
            Platform_SetDrawContext(dpy, xd, vis, cmap);
            Platform_DrawTextRaw(5, state->height - 40, g_vim_mode, VimModeTextColor(), fh, "RIGHT", 5, FontSize);
        }
    }
    if (needDestroy) Platform_DestroyFont(fh);
    return true;
}

bool fl_type_text_platform(const char *s){
 if(!s) return false;
 remap_reset();
 Platform_EmitKey(VK_LCONTROL,0); Platform_EmitKey(VK_RCONTROL,0);
 Platform_EmitKey(VK_LSHIFT,0); Platform_EmitKey(VK_RSHIFT,0);
 Platform_EmitKey(VK_LMENU,0); Platform_EmitKey(VK_RMENU,0);
 Platform_EmitKey(VK_LWIN,0); Platform_EmitKey(VK_RWIN,0);
 for(;*s;s++){
  char c=*s;
  if(c==' '){ Platform_EmitKey(VK_SPACE,1); Platform_EmitKey(VK_SPACE,0); continue; }
  if(c=='\n'){ Platform_EmitKey(VK_RETURN,1); Platform_EmitKey(VK_RETURN,0); continue; }
  if(c=='\t'){ Platform_EmitKey(VK_TAB,1); Platform_EmitKey(VK_TAB,0);continue; }
  int vk=0; bool shift=false;
  if(c>='a' && c<='z'){ vk=toupper((unsigned char)c); }
  else if(c>='A' && c<='Z'){ vk=c; shift=true; }
  else if(c>='0' && c<='9'){ vk=c; }
  else {
   switch(c){
    case '!': vk='1'; shift=true; break;
    case '@': vk='2'; shift=true; break;
    case '#': vk='3'; shift=true; break;
    case '$': vk='4'; shift=true; break;
    case '%': vk='5'; shift=true; break;
    case '^': vk='6'; shift=true; break;
    case '&': vk='7'; shift=true; break;
    case '*': vk='8'; shift=true; break;
    case '(': vk='9'; shift=true; break;
    case ')': vk='0'; shift=true; break;
    case '_': vk=VK_OEM_MINUS; shift=true; break;
    case '-': vk=VK_OEM_MINUS; break;
    case '+': vk=VK_OEM_PLUS; shift=true; break;
    case '=': vk=VK_OEM_PLUS; break;
    case '{': vk=VK_OEM_4; shift=true; break;
    case '[': vk=VK_OEM_4; break;
    case '}': vk=VK_OEM_6; shift=true; break;
    case ']': vk=VK_OEM_6; break;
    case ':': vk=VK_OEM_1; shift=true; break;
    case ';': vk=VK_OEM_1; break;
    case '"': vk=VK_OEM_7; shift=true; break;
    case '\'': vk=VK_OEM_7; break;
    case '<': vk=VK_OEM_COMMA; shift=true; break;
    case ',': vk=VK_OEM_COMMA; break;
    case '>': vk=VK_OEM_PERIOD; shift=true; break;
    case '.': vk=VK_OEM_PERIOD; break;
    case '?': vk=VK_OEM_2; shift=true; break;
    case '/': vk=VK_OEM_2; break;
    case '|': vk=0xDC; shift=true; break;
    case '\\': vk=0xDC; break;
    case '`': vk=VK_OEM_3; break;
    case '~': vk=VK_OEM_3; shift=true; break;
    default: continue;
   }
  }
  if(shift) Platform_EmitKey(VK_LSHIFT,1);
  Platform_EmitKey(vk,1); Platform_EmitKey(vk,0);
  if(shift) Platform_EmitKey(VK_LSHIFT,0);
 }
 return true;
}

typedef struct {
    double seconds;
    void (*callback)(void);
} TimerPayload;

static void* TimerThread(void* arg) {
    TimerPayload* payload = (TimerPayload*)arg;
    if (!payload) return NULL;

    useconds_t micros = (useconds_t)(payload->seconds * 1000000.0);
    usleep(micros);

    if (payload->callback) {
        payload->callback();
    }

    free(payload);
    return NULL;
}

void fl_run_after_time_platform(double seconds, void (*callback)(void)) {
    if (!callback || seconds < 0) return;

    TimerPayload* payload = (TimerPayload*)malloc(sizeof(TimerPayload));
    if (!payload) return;
    payload->seconds = seconds;
    payload->callback = callback;

    pthread_t thread;
    if (pthread_create(&thread, NULL, TimerThread, payload) != 0) {
        free(payload);
        return;
    }

    pthread_detach(thread);
}

FlTimer fl_start_timer_platform(void) {
    FlTimer t = {0};
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    t.ticks = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    return t;
}

double fl_end_timer_platform(FlTimer t) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    if (now < t.ticks) return 0.0;
    return (double)(now - t.ticks) / 1000000.0;
}

void fl_sleep_platform(double seconds) {
    if (seconds <= 0) return;
    struct timespec req;
    req.tv_sec = (time_t)seconds;
    req.tv_nsec = (long)((seconds - (double)req.tv_sec) * 1000000000.0);
    nanosleep(&req, NULL);
}

void fl_screenshot_copy_area_platform(void) {
    if (Wm_ScreenshotCopyArea()) return;
    char cmd[512];
    char* wd = Platform_GetWorkingDir();
    char path[MAX_PATH];

    snprintf(path, sizeof(path), "%s/flint_clip.png", wd ? wd : ".");

    if (getenv("WAYLAND_DISPLAY")) {
    snprintf(cmd, sizeof(cmd),
             "%s/external/grim -g \"$(%s/external/slurp)\" -t png %s",
             wd, wd, path);
    } else {
      snprintf(cmd, sizeof(cmd),
         "%s/external/scrot -s %s",
         wd, path);
    }

    free(Platform_RunCommandAndGetOutput(cmd));
    fl_clipboard_set_png_path_platform(path);
}

void fl_screenshot_copy_platform(void) {
    if (Wm_ScreenshotCopy()) return;
    char cmd[512];
    char* wd = Platform_GetWorkingDir();
    char path[MAX_PATH];

    snprintf(path, sizeof(path), "%s/flint_clip.png", wd ? wd : ".");
    if (getenv("WAYLAND_DISPLAY")) {
        snprintf(cmd, sizeof(cmd),
                 "%s/external/grim -t png %s",
                 wd, path);
    } else {
          snprintf(cmd, sizeof(cmd),
             "%s/external/scrot %s",
             wd, path);
    }

    free(Platform_RunCommandAndGetOutput(cmd));
    fl_clipboard_set_png_path_platform(path);
}

void fl_screenshot_save_area_platform(char* save_path) {
    if (Wm_ScreenshotSaveArea(save_path)) return;
    char cmd[512];
    char* wd = Platform_GetWorkingDir();
    
    if (getenv("WAYLAND_DISPLAY")) {
      snprintf(cmd, sizeof(cmd),
               "%s/external/grim -g \"$(%s/external/slurp)\" %s",
               wd, wd,save_path);
    } else {
      snprintf(cmd, sizeof(cmd),
                "%s/external/scrot -s %s",
                wd,save_path);
    }

    free(Platform_RunCommandAndGetOutput(cmd));
}
void fl_screenshot_save_platform(char* save_path) {
    if (Wm_ScreenshotSave(save_path)) return;
    char cmd[512];
    char* wd = Platform_GetWorkingDir();
    
    if (getenv("WAYLAND_DISPLAY")) {
      snprintf(cmd, sizeof(cmd),
               "%s/external/grim %s",
               wd,save_path);
    } else {
      snprintf(cmd, sizeof(cmd),
                "%s/external/scrot %s",
                wd,save_path);
    }

    free(Platform_RunCommandAndGetOutput(cmd));
}

void fl_ocr_area_to_clipboard_platform() {
    char* out = Wm_OcrArea();
    if (!out) {
    char cmd[512];
    char* wd = Platform_GetWorkingDir();

    if (getenv("WAYLAND_DISPLAY")) {
        snprintf(cmd, sizeof(cmd),
                 "%s/external/grim -g \"$(%s/external/slurp)\" - | tesseract stdin stdout -l eng png",
                 wd, wd);
    } else {
          char temp_path[MAX_PATH];
          snprintf(temp_path, sizeof(temp_path), "/tmp/flint_ocr_%d", getpid());

          snprintf(cmd, sizeof(cmd),
                   "%s/external/scrot -s %s && tesseract %s stdout -l eng; rm -f %s",
                   wd, temp_path, temp_path, temp_path);
    }
    out = Platform_RunCommandAndGetOutput(cmd);
    }
    if (!out || !out[0]) {
        free(out);
        return;
    }
    size_t n = strlen(out);
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r')) out[--n] = '\0';
    if (out[0]) { if (Platform_CopyToClipboard(out)) fl_clipboard_history_push_text_platform(out); }
    free(out);

}

struct MemoryStruct {
    char *memory;
    size_t size;
};

void* fl_thread_create_platform(void* (*fn)(void*), void* arg) {
    if (!fn) return NULL;
    pthread_t* t = malloc(sizeof(*t));
    if (!t) return NULL;
    if (pthread_create(t, NULL, fn, arg) != 0) { free(t); return NULL; }
    return t;
}

void fl_thread_join_platform(void* thread) {
    if (!thread) return;
    pthread_t* t = (pthread_t*)thread;
    pthread_join(*t, NULL);
    free(t);
}

void* fl_mutex_create_platform(void) {
    pthread_mutex_t* m = malloc(sizeof(*m));
    if (!m) return NULL;
    if (pthread_mutex_init(m, NULL) != 0) { free(m); return NULL; }
    return m;
}

void fl_mutex_destroy_platform(void* mutex) {
    if (!mutex) return;
    pthread_mutex_t* m = (pthread_mutex_t*)mutex;
    pthread_mutex_destroy(m);
    free(m);
}

void fl_mutex_lock_platform(void* mutex) {
    if (!mutex) return;
    pthread_mutex_lock((pthread_mutex_t*)mutex);
}

void fl_mutex_unlock_platform(void* mutex) {
    if (!mutex) return;
    pthread_mutex_unlock((pthread_mutex_t*)mutex);
}
