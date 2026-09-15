#include "core/flapi.h"
#include "deps/log.h"
#include "core/platform.h"
#include "core/flapi_platform.h"
#include "platform/draw.h"
#include "platform/x11/drawing.h"
#include "platform/x11/audio_x11.h"
#include "config.h"
#include <errno.h>
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


bool fl_get_all_files_from_dir_platform(const char* dir_path, char** list_of_allowed_extensions,
                                        ListOfFiles* list_of_files, char** list_of_names_to_ignore) {
    PROFILE_START(timer);

    if (!dir_path || !list_of_files) {
        PROFILE_END_VAR(timer, "flapi_x11:fl_get_all_files_from_dir_platform");
        return false;
    }

    DIR* dir = opendir(dir_path);
    if (!dir) {
        perror("opendir");
        PROFILE_END_VAR(timer, "flapi_x11:fl_get_all_files_from_dir_platform");
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
                fl_get_all_files_from_dir_platform(full_path, list_of_allowed_extensions,
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

                    FileItem* new_items = realloc(list_of_files->items, new_capacity * sizeof(FileItem));
                    if (!new_items) {
                        closedir(dir);
                        PROFILE_END_VAR(timer, "flapi_x11:fl_get_all_files_from_dir_platform");
                        return false;
                    }
                    list_of_files->items    = new_items;
                    list_of_files->capacity = new_capacity;
                }

                FileItem* item = &list_of_files->items[list_of_files->count];

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
    PROFILE_END_VAR(timer, "flapi_x11:fl_get_all_files_from_dir_platform");
    return true;
}

bool fl_launch_with_args_platform(const char* path, const char* args) {
    PROFILE_START(timer);
    if (!path) { PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform"); return false; }
    char expanded[1024];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) { PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform"); return false; }
    path = expanded;
    if (strstr(path, ".desktop") != NULL) {
        bool is_terminal = false;
        char exec_line[1024] = {0};
        char workdir[1024] = {0};
        FILE* df = fopen(path, "r");
        if (df){
            char line[1024];
            while (fgets(line, sizeof(line), df)){
                char* p=line; while(*p==' '||*p=='\t') p++;
                if (Platform_StringContainsCaseInsensitive(p, "Terminal=")){
                    char* v=p+9; while(*v==' '||*v=='\t') v++; v[strcspn(v, "\r\n")]='\0';
                    if(Platform_StringContainsCaseInsensitive(v, "true") || strcmp(v,"1")==0) is_terminal=true;
                } else if (Platform_StringContainsCaseInsensitive(p, "Exec=")){
                    char* v=p+5; while(*v==' '||*v=='\t') v++; v[strcspn(v, "\r\n")]='\0';
                    strncpy(exec_line, v, sizeof(exec_line)-1);
                } else if (Platform_StringContainsCaseInsensitive(p, "Path=")){
                    char* v=p+5; while(*v==' '||*v=='\t') v++; v[strcspn(v, "\r\n")]='\0';
                    strncpy(workdir, v, sizeof(workdir)-1);
                }
            }
            fclose(df);
        }
        if(exec_line[0]){
            char cleaned[1024]={0}; size_t o=0;
            for(size_t i=0; exec_line[i] && o<sizeof(cleaned)-1; i++){
                if(exec_line[i]=='%' && exec_line[i+1]){
                    char c=exec_line[i+1];
                    if(c=='f'||c=='F'||c=='u'||c=='U'||c=='i'||c=='c'||c=='k'||c=='v'||c=='m'){ i++; while(exec_line[i+1]==' ') i++; continue; }
                }
                cleaned[o++]=exec_line[i];
            }
            cleaned[o]='\0';
            char* s=cleaned; while(*s==' '||*s=='\t') s++;
            char* e=s+strlen(s)-1; while(e>s && (*e==' '||*e=='\t')) *e--='\0';
            strncpy(exec_line, s, sizeof(exec_line)-1);
        }
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
        const char* real_user = Platform_GetRealUser();
        const char* display = Platform_GetSessionDisplay();
        const char* xauthority = Platform_GetSessionXAuthority();
        const char* wayland_display = getenv("WAYLAND_DISPLAY");
        char runtime_dir[256]; snprintf(runtime_dir, sizeof(runtime_dir), "%s", Platform_GetRuntimeDir());
        char* filename = strrchr(expanded, '/');
        filename = filename ? filename + 1 : expanded;
        char app_id[256]; strncpy(app_id, filename, sizeof(app_id)-1); app_id[sizeof(app_id)-1]='\0';
        char* ext = strstr(app_id, ".desktop"); if (ext) *ext='\0';
        char disp_env[256], xauth_env[512], runtime_env[512], wayland_env[512];
        snprintf(disp_env, sizeof(disp_env), "DISPLAY=%s", display);
        snprintf(xauth_env, sizeof(xauth_env), "XAUTHORITY=%s", xauthority);
        snprintf(runtime_env, sizeof(runtime_env), "XDG_RUNTIME_DIR=%s", runtime_dir);
        snprintf(wayland_env, sizeof(wayland_env), "WAYLAND_DISPLAY=%s", wayland_display ? wayland_display : "");
        pid_t pid = fork();
        if (pid==0){
            setsid(); pid_t pid2=fork(); if(pid2<0) _exit(1); if(pid2>0) _exit(0);
            int fd=open("/dev/null",O_RDWR); if(fd>=0){dup2(fd,0);dup2(fd,1);dup2(fd,2); if(fd>2) close(fd);}
            if(Platform_IsRunningAsRoot()){
                char* const argv_sysd[] = {"sudo","-u",(char*)real_user,"env",disp_env,xauth_env,runtime_env,wayland_env,"systemd-run","--user","--scope","gtk-launch",app_id,NULL};
                execvp(argv_sysd[0], argv_sysd);
                char* const argv_fallback[] = {"sudo","-u",(char*)real_user,"env",disp_env,xauth_env,runtime_env,wayland_env,"gtk-launch",app_id,NULL};
                execvp(argv_fallback[0], argv_fallback);
            } else {
                setenv("DISPLAY",display,1); setenv("XAUTHORITY",xauthority,1); setenv("XDG_RUNTIME_DIR",runtime_dir,1); setenv("WAYLAND_DISPLAY", wayland_display ? wayland_display : "", 1);
                char* const argv_sysd2[] = {"systemd-run","--user","--scope","gtk-launch",app_id,NULL};
                execvp(argv_sysd2[0], argv_sysd2);
                char* const argv_fallback2[] = {"gtk-launch",app_id,NULL};
                execvp(argv_fallback2[0], argv_fallback2);
            }
            _exit(127);
        } else if(pid>0){ int st=0; waitpid(pid,&st,0); if(WIFEXITED(st) && WEXITSTATUS(st)!=0 && exec_line[0]){
                if(is_terminal){
                    char extra2[2048]; snprintf(extra2, sizeof(extra2), "-e sh -c '%s; exec'", exec_line,UsedShell);
                    PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform");
                    return Platform_LaunchTerminalAtPath(Platform_GetRealHome(), extra2);
                } else {
                    pid_t pid3=fork();
                    if(pid3==0){
                        setsid(); pid_t p2=fork(); if(p2<0) _exit(1); if(p2>0) _exit(0);
                        int fd2=open("/dev/null",O_RDWR); if(fd2>=0){dup2(fd2,0);dup2(fd2,1);dup2(fd2,2); if(fd2>2) close(fd2);}
                        if(Platform_IsRunningAsRoot()){
                            char* const av2[]={"sudo","-u",(char*)real_user,"env",disp_env,xauth_env,runtime_env,wayland_env,"sh","-c",exec_line,NULL};
                            execvp(av2[0],av2);
                        } else {
                            setenv("DISPLAY",display,1); setenv("XAUTHORITY",xauthority,1); setenv("XDG_RUNTIME_DIR",runtime_dir,1); setenv("WAYLAND_DISPLAY", wayland_display ? wayland_display : "", 1);
                            execl("/bin/sh","sh","-c",exec_line,(char*)NULL);
                        }
                        _exit(127);
                    } else if(pid3>0){ int st2=0; waitpid(pid3,&st2,0); PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform"); return WIFEXITED(st2) && WEXITSTATUS(st2)==0; }
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
        const char* real_user = Platform_GetRealUser();
        const char* display = Platform_GetSessionDisplay();
        const char* xauthority = Platform_GetSessionXAuthority();
        char runtime_dir[256]; snprintf(runtime_dir, sizeof(runtime_dir), "%s", Platform_GetRuntimeDir());
        char disp_env[256], xauth_env[512], runtime_env[512];
        snprintf(disp_env, sizeof(disp_env), "DISPLAY=%s", display);
        snprintf(xauth_env, sizeof(xauth_env), "XAUTHORITY=%s", xauthority);
        snprintf(runtime_env, sizeof(runtime_env), "XDG_RUNTIME_DIR=%s", runtime_dir);
        if(Platform_IsRunningAsRoot()){
            char* const argv[] = {"sudo","-u",(char*)real_user,"env",disp_env,xauth_env,runtime_env,"sh","-c",cmd,NULL};
            pid_t pid=fork(); if(pid<0) { PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform"); return false; }
            if(pid==0){ setsid(); pid_t p2=fork(); if(p2<0) _exit(1); if(p2>0) _exit(0); int fd=open("/dev/null",O_RDWR); if(fd>=0){dup2(fd,0);dup2(fd,1);dup2(fd,2); if(fd>2) close(fd);} execvp(argv[0],argv); _exit(127); }
            int st=0; waitpid(pid,&st,0); PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform"); return true;
        } else {
            pid_t pid=fork(); if(pid<0) { PROFILE_END_VAR(timer, "flapi_x11:fl_launch_with_args_platform"); return false; }
            if(pid==0){
                setsid(); pid_t pid2=fork(); if(pid2<0) _exit(1); if(pid2>0) _exit(0);
                int fd=open("/dev/null",O_RDWR); if(fd>=0){dup2(fd,0);dup2(fd,1);dup2(fd,2); if(fd>2) close(fd);}
                setenv("DISPLAY",display,1); setenv("XAUTHORITY",xauthority,1); setenv("XDG_RUNTIME_DIR",runtime_dir,1);
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
    g_fl_list_itemH = spec->item_height ? spec->item_height : FontSize + 14;
    g_fl_list_y0 = spec->y ? spec->y : 70;
    int list_h = spec->h ? spec->h : 400;
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
        int R=6; int t=ListboxBorderThickness; if(t<1) t=1; if(t>4) t=4;
        int bx = g_fl_list_spec.x + 1;
        int bw = (g_fl_list_spec.w ? g_fl_list_spec.w : 650) - 2;
        if (sel) {
            Linux_FillRoundedRect(g_fl_list_dpy, g_fl_list_xd, g_fl_list_vis, g_fl_list_cmap, rgb_to_hex(COLOR_SELECTED_TEXT), OpacityAlpha16(), bx+t, y+1+t, bw-2*t, g_fl_list_itemH-2-2*t, R-1);
        } else if (!EnableShaders || ShaderPreset==0) {
            Linux_FillRoundedRect(g_fl_list_dpy, g_fl_list_xd, g_fl_list_vis, g_fl_list_cmap, rgb_to_hex(COLOR_BG), OpacityAlpha16(), bx+t, y+1+t, bw-2*t, g_fl_list_itemH-2-2*t, R-1);
        }
    } else {
        PromptState tmp = {0};
        tmp.width = g_fl_list_spec.w ? g_fl_list_spec.w : 900;
        tmp.height = g_fl_list_spec.h ? g_fl_list_spec.h : 400;
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
