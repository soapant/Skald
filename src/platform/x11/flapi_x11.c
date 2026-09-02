#include "core/flapi.h"
#include "core/platform.h"
#include "core/flapi_platform.h"
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

bool fl_case_insensitive_string_match_platform(char* needle,char* haystack) {
  return strcasecmp(needle,haystack);
}

int fl_copy_into_clipboard_platform(const char* text) {
    int status = 0;
    size_t len = strlen(text);

    char buffer[256];
    char cmd[256];

    const char *wayland_display = getenv("WAYLAND_DISPLAY");
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if ((!wayland_display || !wayland_display[0]) || (!runtime_dir || !runtime_dir[0])) {
        static char found_display[64];
        static char found_runtime[256];
        found_display[0] = '\0';
        found_runtime[0] = '\0';
        DIR *dir = opendir("/run/user");
        if (dir) {
            struct dirent *e;
            while ((e = readdir(dir)) != NULL) {
                if (e->d_name[0] == '.') continue;
                char udir[256];
                snprintf(udir, sizeof(udir), "/run/user/%s", e->d_name);
                DIR *sub = opendir(udir);
                if (!sub) continue;
                struct dirent *f;
                while ((f = readdir(sub)) != NULL) {
                    if (strncmp(f->d_name, "wayland-", 8) == 0) {
                        snprintf(found_display, sizeof(found_display), "%s", f->d_name);
                        snprintf(found_runtime, sizeof(found_runtime), "%s", udir);
                        break;
                    }
                }
                closedir(sub);
                if (found_display[0] != '\0') break;
            }
            closedir(dir);
        }
        if (!wayland_display || !wayland_display[0])
            wayland_display = found_display[0] != '\0' ? found_display : NULL;
        if (!runtime_dir || !runtime_dir[0])
            runtime_dir = found_runtime[0] != '\0' ? found_runtime : NULL;
    }

    if (wayland_display != NULL && wayland_display[0] != '\0' &&
        runtime_dir != NULL && runtime_dir[0] != '\0') {
        snprintf(cmd, sizeof(cmd), "XDG_RUNTIME_DIR=%s WAYLAND_DISPLAY=%s wl-copy",
                 runtime_dir, wayland_display);
    } else {
        snprintf(cmd, sizeof(cmd), "xclip -selection clipboard");
    }

    FILE *pipe = popen(cmd, "w");
    if (!pipe) {
        pipe = popen("xclip -selection clipboard", "w");
        if (!pipe) {
            pipe = popen("xsel --clipboard --input", "w");
        }
    }

    if (!pipe) {
        fprintf(stderr, "[PLATFORM] Error: Failed to find a valid Linux clipboard manager.\n");
        return 0;
    }

    size_t len2 = strlen(text);
    fprintf(pipe, "%s", text);
    
    status = pclose(pipe);
    return (status == 0) ? 1 : 0;
}
bool fl_get_all_files_from_dir_platform(const char* dir_path, char** list_of_allowed_extensions, ListOfFiles* list_of_files,char** list_of_names_to_ignore) {

    DIR* dir = opendir(dir_path);
    if (!dir) return false;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            bool should_ignore = false;
            if (list_of_names_to_ignore) {
                for (int i = 0; list_of_names_to_ignore[i] != NULL; i++) {
                    if (strcasecmp(entry->d_name, list_of_names_to_ignore[i]) == 0) {
                        should_ignore = true;
                        break;
                    }
                }
            }
            if (!should_ignore) {
                fl_get_all_files_from_dir_platform(full_path, list_of_allowed_extensions, list_of_files, list_of_names_to_ignore);
            }
        } else {
            bool allowed = true;
            if (list_of_allowed_extensions) {
                allowed = false;
                const char* ext = strrchr(entry->d_name, '.');
                if (ext) {
                    ext++;
                    for (int i = 0; list_of_allowed_extensions[i] != NULL; i++) {
                        if (strcasecmp(ext, list_of_allowed_extensions[i]) == 0) {
                            allowed = true;
                            break;
                        }
                    }
                }
            }

            if (allowed) {
                if (list_of_files->count >= list_of_files->capacity) {
                    list_of_files->capacity = list_of_files->capacity == 0 ? 10 : list_of_files->capacity * 2;
                    list_of_files->items = realloc(list_of_files->items, list_of_files->capacity * sizeof(FileItem));
                }

                FileItem* item = &list_of_files->items[list_of_files->count];

                strncpy(item->path, full_path, sizeof(item->path) - 1);
                item->path[sizeof(item->path) - 1] = '\0';

                char clean_name[256];
                strncpy(clean_name, entry->d_name, sizeof(clean_name) - 1);
                clean_name[sizeof(clean_name) - 1] = '\0';
                char* dot = strrchr(clean_name, '.');
                if (dot) *dot = '\0';
                strncpy(item->name, clean_name, sizeof(item->name) - 1);
                item->name[sizeof(item->name) - 1] = '\0';

                list_of_files->count++;
            }
        }
    }
    closedir(dir);
    return true;
}

bool fl_launch_with_args_platform(const char* path, const char* args) {
    if (!path) return false;
    char expanded[1024];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) return false;
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
                if (strncasecmp(p, "Terminal=", 9)==0){
                    char* v=p+9; while(*v==' '||*v=='\t') v++; v[strcspn(v, "\r\n")]='\0';
                    if(strcasecmp(v,"true")==0 || strcmp(v,"1")==0) is_terminal=true;
                } else if (strncasecmp(p, "Exec=", 5)==0){
                    char* v=p+5; while(*v==' '||*v=='\t') v++; v[strcspn(v, "\r\n")]='\0';
                    strncpy(exec_line, v, sizeof(exec_line)-1);
                } else if (strncasecmp(p, "Path=", 5)==0){
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
            if (args && args[0]) snprintf(extra, sizeof(extra), "-e sh -c '%s %s; exec ${SHELL:-/bin/sh}'", exec_line, args);
            else snprintf(extra, sizeof(extra), "-e sh -c '%s; exec ${SHELL:-/bin/sh}'", exec_line);
            if(wd) return Platform_LaunchTerminalAtPath(wd, extra);
            else return Platform_LaunchTerminalAtPath(Platform_GetRealHome(), extra);
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
                    char extra2[2048]; snprintf(extra2, sizeof(extra2), "-e sh -c '%s; exec ${SHELL:-/bin/sh}'", exec_line);
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
                    } else if(pid3>0){ int st2=0; waitpid(pid3,&st2,0); return WIFEXITED(st2) && WEXITSTATUS(st2)==0; }
                }
            }
            return true;
        }
        return false;
    } else {
        // direct launch with args (like old launch_exe_with_args)
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
            pid_t pid=fork(); if(pid<0) return false;
            if(pid==0){ setsid(); pid_t p2=fork(); if(p2<0) _exit(1); if(p2>0) _exit(0); int fd=open("/dev/null",O_RDWR); if(fd>=0){dup2(fd,0);dup2(fd,1);dup2(fd,2); if(fd>2) close(fd);} execvp(argv[0],argv); _exit(127); }
            int st=0; waitpid(pid,&st,0); return true;
        } else {
            pid_t pid=fork(); if(pid<0) return false;
            if(pid==0){
                setsid(); pid_t pid2=fork(); if(pid2<0) _exit(1); if(pid2>0) _exit(0);
                int fd=open("/dev/null",O_RDWR); if(fd>=0){dup2(fd,0);dup2(fd,1);dup2(fd,2); if(fd>2) close(fd);}
                setenv("DISPLAY",display,1); setenv("XAUTHORITY",xauthority,1); setenv("XDG_RUNTIME_DIR",runtime_dir,1);
                execl("/bin/sh","sh","-c",cmd,(char*)NULL);
                _exit(127);
            }
            int st=0; waitpid(pid,&st,0); return true;
        }
    }
}

bool fl_launch_with_args_as_admin_platform(const char* path, const char* args) {
    if (!path) return false;
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) return false;
    // pkexec with args
    char cmd[2048];
    if (args && args[0]) snprintf(cmd, sizeof(cmd), "%s %s", expanded, args);
    else snprintf(cmd, sizeof(cmd), "%s", expanded);
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        char *argv1[] = {"pkexec", cmd, NULL};
        execvp("pkexec", argv1);
        char *argv2[] = {"sudo", cmd, NULL};
        execvp("sudo", argv2);
        _exit(1);
    }
    return true;
}

char* fl_search_file_in_dir_platform(const char* directory, const char* search_string,char* output_path) {
    if (!directory || !search_string || strlen(search_string) == 0) return NULL;
    DIR* dir = opendir(directory);
    if (!dir) return NULL;
    static char found_path[MAX_PATH];
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0]=='.' && (entry->d_name[1]=='\0' || (entry->d_name[1]=='.' && entry->d_name[2]=='\0'))) continue;
        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", directory, entry->d_name);
        struct stat st;
        if (stat(full_path, &st)!=0) continue;
        if (S_ISDIR(st.st_mode)) continue;
        if (strcasestr(entry->d_name, search_string) != NULL) {
            snprintf(found_path, sizeof(found_path), "%s", full_path);
            closedir(dir);
            return found_path;
        }
    }
    closedir(dir);
    return NULL;
}
