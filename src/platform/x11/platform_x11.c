#define _GNU_SOURCE
#define _XOPEN_SOURCE 500
#include <pthread.h>
#include "deps/log.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include "platform/x11/display.h"

#include <sys/stat.h> 
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <png.h>
#include <ctype.h>
#include "core/platform.h"
#include "config.h"
#include "config.h"
#include "core/platform.h"
#include "helpers/hotkey.h"
#include "helpers/alias.h"
#include "helpers/flag.h"
#include "helpers/file.h"
#include "helpers/ui.h"
#include "core/context.h"
#include "core/modal.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/inotify.h>
#include <pthread.h>
#include "core/timer.h"
#include <linux/uinput.h>
#include <linux/input.h>
#include <errno.h>
#include "platform/x11/drawing.h"
#include "wm/wm.h"
#if __has_include(<X11/extensions/XTest.h>)
#include <X11/extensions/XTest.h>
#endif




static bool x11_daemon_exec(char* const argv[]) {
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        setsid();
        pid_t pid2 = fork();
        if (pid2 < 0) _exit(1);
        if (pid2 > 0) _exit(0);
        int fd = open("/dev/null", O_RDWR);
        if (fd >= 0) {
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            if (fd > 2) close(fd);
        }
        execvp(argv[0], argv);
        _exit(127);
    }
    int st = 0;
    waitpid(pid, &st, 0);
    return true;
}

static bool x11_daemon_exec_with_x11_env(char* const argv[], const char* display, const char* xauth, const char* runtime_dir) {
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        setsid();
        pid_t pid2 = fork();
        if (pid2 < 0) _exit(1);
        if (pid2 > 0) _exit(0);
        int fd = open("/dev/null", O_RDWR);
        if (fd >= 0) {
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            if (fd > 2) close(fd);
        }
        if (display) setenv("DISPLAY", display, 1);
        if (xauth) setenv("XAUTHORITY", xauth, 1);
        if (runtime_dir) setenv("XDG_RUNTIME_DIR", runtime_dir, 1);
        execvp(argv[0], argv);
        _exit(127);
    }
    int st = 0;
    waitpid(pid, &st, 0);
    return true;
}

bool Platform_LaunchTerminalAtPath(const char* working_dir, const char* extra_args){
    PROFILE_START(timer);
    if(!working_dir) { log_error("Platform_LaunchTerminalAtPath: working_dir is NULL"); PROFILE_END_VAR(timer, "platform_x11:Platform_LaunchTerminalAtPath"); return false; }

    const char* real_user = Platform_GetRealUser();
    const char* display = Platform_GetSessionDisplay();
    const char* xauthority = Platform_GetSessionXAuthority();
    const char* runtime_dir_raw = Platform_GetRuntimeDir();
    const char* wayland_display = getenv("WAYLAND_DISPLAY");

    char runtime_dir[256];
    snprintf(runtime_dir, sizeof(runtime_dir), "%s", runtime_dir_raw ? runtime_dir_raw : "");

    char disp_env[256], xauth_env[512], runtime_env[512], wayland_env[512];
    snprintf(disp_env, sizeof(disp_env), "DISPLAY=%s", display ? display : "");
    snprintf(xauth_env, sizeof(xauth_env), "XAUTHORITY=%s", xauthority ? xauthority : "");
    snprintf(runtime_env, sizeof(runtime_env), "XDG_RUNTIME_DIR=%s", runtime_dir);
    snprintf(wayland_env, sizeof(wayland_env), "WAYLAND_DISPLAY=%s", wayland_display ? wayland_display : "");

    pid_t pid = fork();
    if(pid < 0) { log_error("platform_x11:Platform_LaunchTerminalAtPath: operation failed"); PROFILE_END_VAR(timer, "platform_x11:Platform_LaunchTerminalAtPath"); return false; }

    if(pid == 0){
        setsid();
        pid_t pid2 = fork();
        if(pid2 < 0) _exit(1);
        if(pid2 > 0) _exit(0);

        int fd = open("/dev/null", O_RDWR);
        if(fd >= 0){
            dup2(fd, 0);
            dup2(fd, 1);
            dup2(fd, 2);
            if(fd > 2) close(fd);
        }

        if(Platform_IsRunningAsRoot()){
            if(extra_args && extra_args[0]){
                char cmd[2048];
                snprintf(cmd, sizeof(cmd), "%s --working-directory=\"%s\" %s", UsedTerminal, working_dir, extra_args);
                char* const argv[] = {"sudo","-u",(char*)real_user,"env",disp_env,xauth_env,runtime_env,wayland_env,"sh","-c",cmd,NULL};
                execvp(argv[0], argv);
            } else {
                char* const argv[] = {"sudo","-u",(char*)real_user,"env",disp_env,xauth_env,runtime_env,wayland_env,(char*)UsedTerminal,"--working-directory",(char*)working_dir,NULL};
                execvp(argv[0], argv);
                char* const argv2[] = {"sudo","-u",(char*)real_user,"env",disp_env,xauth_env,runtime_env,wayland_env,(char*)UsedTerminal,NULL};
                execvp(argv2[0], argv2);
            }
        } else {
            setenv("DISPLAY", display, 1);
            setenv("XAUTHORITY", xauthority, 1);
            setenv("XDG_RUNTIME_DIR", runtime_dir, 1);
            if (wayland_display && wayland_display[0])
                setenv("WAYLAND_DISPLAY", wayland_display, 1);

            if(extra_args && extra_args[0]){
                char cmd[2048];
                snprintf(cmd, sizeof(cmd), "%s --working-directory=\"%s\" %s", UsedTerminal, working_dir, extra_args);
                execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
            } else {
                execlp(UsedTerminal, UsedTerminal, "--working-directory", working_dir, (char*)NULL);
            }
        }
        _exit(127);
    }

    int st = 0;
    waitpid(pid, &st, 0);
    PROFILE_END_VAR(timer, "platform_x11:Platform_LaunchTerminalAtPath");
    return true;
}

static bool daemon_spawn_sudo_env(const char* real_user, const char* display, const char* xauthority, const char* runtime_dir, char* const argv[]) {
    (void)real_user; (void)display; (void)xauthority; (void)runtime_dir;
    PROFILE_START(timer);
    bool ok = x11_daemon_exec(argv);
    PROFILE_END_VAR(timer, "platform_x11:daemon_spawn_sudo_env");
    return ok;
}

bool Platform_ShellExecuteWithVerb(const char* path, const char* verb, bool async) {
    PROFILE_START(timer);
    if (!path) { log_error("Platform_ShellExecuteWithVerb: path is NULL"); PROFILE_END_VAR(timer, "platform_x11:Platform_ShellExecuteWithVerb"); return false; }
    (void)verb;
    const char* real_user = Platform_GetRealUser();
    const char* display = Platform_GetSessionDisplay();
    const char* xauthority = Platform_GetSessionXAuthority();
    char runtime_dir[256];
    snprintf(runtime_dir, sizeof(runtime_dir), "%s", Platform_GetRuntimeDir());

    if (!async) {
        if (Platform_IsRunningAsRoot()){
            char command[2048];
            snprintf(command, sizeof(command),
                     "sudo -u %s DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s xdg-open \"%s\" > /dev/null 2>&1",
                     real_user, display, xauthority, runtime_dir, path);
            PROFILE_END_VAR(timer, "platform_x11:Platform_ShellExecuteWithVerb");
            return Platform_RunTerminalCommand(command,false);
        } else {
            char command[2048];
            snprintf(command, sizeof(command),
                     "DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s xdg-open \"%s\" > /dev/null 2>&1",
                     display, xauthority, runtime_dir, path);
            PROFILE_END_VAR(timer, "platform_x11:Platform_ShellExecuteWithVerb");
            return  Platform_RunTerminalCommand(command,false);
        }
    }
    char disp_env[256], xauth_env[512], runtime_env[512];
    snprintf(disp_env, sizeof(disp_env), "DISPLAY=%s", display);
    snprintf(xauth_env, sizeof(xauth_env), "XAUTHORITY=%s", xauthority);
    snprintf(runtime_env, sizeof(runtime_env), "XDG_RUNTIME_DIR=%s", runtime_dir);
    if (Platform_IsRunningAsRoot()){
        char* const argv[] = {"sudo","-u", (char*)real_user, "env", disp_env, xauth_env, runtime_env, "xdg-open", (char*)path, NULL};
        PROFILE_END_VAR(timer, "platform_x11:Platform_ShellExecuteWithVerb");
        return daemon_spawn_sudo_env(real_user, display, xauthority, runtime_dir, argv);
    } else {
        char* const argv[] = {"xdg-open", (char*)path, NULL};
        bool ok = x11_daemon_exec_with_x11_env(argv, display, xauthority, runtime_dir);
        PROFILE_END_VAR(timer, "platform_x11:Platform_ShellExecuteWithVerb");
        return ok;
    }
    PROFILE_END_VAR(timer, "platform_x11:Platform_ShellExecuteWithVerb");
}



static Window FindWindowByTitleRecursive(Display* dpy, Window w, const char* title) {
    PROFILE_START(timer);
    char* name = NULL;
    if (XFetchName(dpy, w, &name) && name != NULL) {
        bool match = (strcmp(name, title) == 0);
        XFree(name);
        if (match) { PROFILE_END_VAR(timer, "platform_x11:FindWindowByTitleRecursive"); return w; }
    }

    Window root, parent, *children = NULL;
    unsigned int n = 0;
    if (XQueryTree(dpy, w, &root, &parent, &children, &n)) {
        for (unsigned int i = 0; i < n; i++) {
            Window found = FindWindowByTitleRecursive(dpy, children[i], title);
            if (found != None) {
                if (children) XFree(children);
                PROFILE_END_VAR(timer, "platform_x11:FindWindowByTitleRecursive");
                return found;
            }
        }
        if (children) XFree(children);
    }
    PROFILE_END_VAR(timer, "platform_x11:FindWindowByTitleRecursive");
    return None;
}

static Window FindWindowByTitle(Display* dpy, const char* title) {
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "platform_x11:FindWindowByTitle");
    return FindWindowByTitleRecursive(dpy, DefaultRootWindow(dpy), title);
}

bool Platform_SendX11Command(const char* window_title, const char* atom_name, long command) {
    PROFILE_START(timer);
    Display* dpy = get_display();
    if (!dpy) { log_error("Platform_SendX11Command: no X11 display"); PROFILE_END_VAR(timer, "platform_x11:Platform_SendX11Command"); return false; }

    Window win = FindWindowByTitle(dpy, window_title);
    bool ok = false;
    if (win != None) {
        XEvent ev = {0};
        ev.xclient.type = ClientMessage;
        ev.xclient.window = win;
        ev.xclient.message_type = XInternAtom(dpy, atom_name, False);
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = command;
        if (XSendEvent(dpy, win, False, NoEventMask, &ev) != 0) {
            XFlush(dpy);
            ok = true;
        }
    }
    PROFILE_END_VAR(timer, "platform_x11:Platform_SendX11Command");
    return ok;
}

bool Platform_SetX11Property(const char* window_title, const char* atom_name, long value) {
    PROFILE_START(timer);
    Display* dpy = get_display();
    if (!dpy) { log_error("Platform_SetX11Property: no X11 display"); PROFILE_END_VAR(timer, "platform_x11:Platform_SetX11Property"); return false; }

    Window win = FindWindowByTitle(dpy, window_title);
    bool ok = false;
    if (win != None) {
        XChangeProperty(dpy, win, XInternAtom(dpy, atom_name, False), XA_INTEGER, 32,
                        PropModeReplace, (unsigned char*)&value, 1);
        XFlush(dpy);
        ok = true;
    }
    PROFILE_END_VAR(timer, "platform_x11:Platform_SetX11Property");
    return ok;
}

bool Platform_SetX11StringProperty(const char* window_title, const char* atom_name, const char* value) {
    PROFILE_START(timer);
    Display* dpy = get_display();
    if (!dpy) { log_error("Platform_SetX11StringProperty: no X11 display"); PROFILE_END_VAR(timer, "platform_x11:Platform_SetX11StringProperty"); return false; }

    Window win = FindWindowByTitle(dpy, window_title);
    bool ok = false;
    if (win != None && value != NULL) {
        XChangeProperty(dpy, win, XInternAtom(dpy, atom_name, False), XA_STRING, 8,
                        PropModeReplace, (const unsigned char*)value, (int)strlen(value));
        XFlush(dpy);
        ok = true;
    }
    PROFILE_END_VAR(timer, "platform_x11:Platform_SetX11StringProperty");
    return ok;
}

char* Platform_RunCommandAndGetOutput(char* command) {
    PROFILE_START(timer);
    FILE* fp;
    char buffer[8192];
    size_t cap = 8192;
    size_t total_size = 0;
    char* output = malloc(cap);

    if (output == NULL) {
        log_error("Platform_RunCommandAndGetOutput: malloc failed");
        PROFILE_END_VAR(timer, "platform_x11:Platform_RunCommandAndGetOutput");
        return NULL;
    }

    fp = popen(command, "r");
    if (fp == NULL) {
        log_error("Platform_RunCommandAndGetOutput: popen failed for command");
        free(output);
        PROFILE_END_VAR(timer, "platform_x11:Platform_RunCommandAndGetOutput");
        return strdup("");
    }

    size_t len;
    while ((len = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        while (total_size + len > cap) {
            cap *= 2;
            char* new_output = realloc(output, cap);
            if (!new_output) {
                free(output);
                pclose(fp);
                PROFILE_END_VAR(timer, "platform_x11:Platform_RunCommandAndGetOutput");
                return strdup("");
            }
            output = new_output;
        }
        memcpy(output + total_size, buffer, len);
        total_size += len;
    }

    pclose(fp);
    output[total_size] = '\0';
    PROFILE_END_VAR(timer, "platform_x11:Platform_RunCommandAndGetOutput");
    return output;
}

char* Platform_RunTerminalCommand(char* command,bool open_terminal) {
    PROFILE_START(timer);
    if (command == NULL || strlen(command) == 0) { log_error("Platform_RunTerminalCommand: command is NULL or empty"); PROFILE_END_VAR(timer, "platform_x11:Platform_RunTerminalCommand"); return NULL; }

    const char* real_user = Platform_GetRealUser();
    const char* real_home = Platform_GetRealHome();

    if (!open_terminal) {
        char* result = Platform_RunCommandAndGetOutput(command);
        PROFILE_END_VAR(timer, "platform_x11:Platform_RunTerminalCommand");
        return result;
    }

    char temp_file[MAX_PATH];
    char cache_dir[MAX_PATH];
    snprintf(cache_dir, sizeof(cache_dir), "%s/.cache", real_home);
    snprintf(temp_file, sizeof(temp_file), "%s/.cache/temp_cmd.sh", real_home);

    if (mkdir(cache_dir, 0755) != 0 && errno != EEXIST) { /* ignore */ }

    FILE* f = fopen(temp_file, "w");
    if (f) {
        fprintf(f, "#!/bin/%s\n",UsedShell);
        fprintf(f, "%s\n", command);
        fclose(f);
        if (Platform_IsRunningAsRoot()){
            char chown_cmd[MAX_PATH + 64];
            snprintf(chown_cmd, sizeof(chown_cmd), "chown %s:%s \"%s\" && chmod 755 \"%s\"", real_user, real_user, temp_file, temp_file);
            Platform_RunTerminalCommand(chown_cmd,false);
        } else {
            chmod(temp_file, 0755);
        }
    }

    const char* display = Platform_GetSessionDisplay();
    const char* xauthority = Platform_GetSessionXAuthority();
    char runtime_dir[256];
    snprintf(runtime_dir,sizeof(runtime_dir),"%s",Platform_GetRuntimeDir());
    char extra[1024];

    snprintf(extra, sizeof(extra),
             "%s -c \"%s '%s'; exec %s\"",
             UsedShell, UsedShell, temp_file, UsedShell);

    Platform_LaunchTerminalAtPath(real_home, extra);
    PROFILE_END_VAR(timer, "platform_x11:Platform_RunTerminalCommand");
    return strdup("");
}

bool Platform_LaunchFileManagerAtPath(char* path,char* args) {
     PROFILE_START(timer);
     bool ret = Platform_ShellExecuteWithVerb(path, "open", true);
     PROFILE_END_VAR(timer, "platform_x11:Platform_LaunchFileManagerAtPath");
     return ret;
}

bool Platform_LaunchApp(const char* path) {
    PROFILE_START(timer);
    if (!path) { log_error("Platform_LaunchApp: path is NULL"); PROFILE_END_VAR(timer, "platform_x11:Platform_LaunchApp"); return false; }
    char expanded[1024];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) { log_error("Platform_LaunchApp: Platform_ExpandPath failed for %s", path); PROFILE_END_VAR(timer, "platform_x11:Platform_LaunchApp"); return false; }
    log_info("Launching App Of Path %s ", path);
    const char* real_user = Platform_GetRealUser();
    const char* display = Platform_GetSessionDisplay();
    const char* xauthority = Platform_GetSessionXAuthority();
    char runtime_dir[256];
    snprintf(runtime_dir, sizeof(runtime_dir), "%s", Platform_GetRuntimeDir());

    if (strstr(expanded, ".desktop") != NULL) {
        bool is_terminal = false;
        char exec_line[1024] = {0};
        char workdir[1024] = {0};
        FILE* df = fopen(expanded, "r");
        if (df){
            char line[1024];
            while (fgets(line, sizeof(line), df)){
                
                char* p=line;
                while(*p==' '||*p=='\t') p++;
                if (Platform_StringContainsCaseInsensitive(p, "Terminal=")){
                    char* v=p+9;
                    while(*v==' '||*v=='\t') v++;
                    v[strcspn(v, "\r\n")]='\0';
                    if(Platform_StringContainsCaseInsensitive(v, "true") || strcmp(v,"1")==0) is_terminal=true;
                } else if (Platform_StringContainsCaseInsensitive(p, "Exec=")){
                    char* v=p+5;
                    while(*v==' '||*v=='\t') v++;
                    v[strcspn(v, "\r\n")]='\0';
                    strncpy(exec_line, v, sizeof(exec_line)-1);
                } else if (Platform_StringContainsCaseInsensitive(p, "Path=")){
                    char* v=p+5;
                    while(*v==' '||*v=='\t') v++;
                    v[strcspn(v, "\r\n")]='\0';
                    strncpy(workdir, v, sizeof(workdir)-1);
                }
            }
            fclose(df);
        }
        
        if(exec_line[0]){
            char cleaned[1024]={0};
            size_t o=0;
            for(size_t i=0; exec_line[i] && o<sizeof(cleaned)-1; i++){
                if(exec_line[i]=='%' && exec_line[i+1]){
                    char c=exec_line[i+1];
                    if(c=='f'||c=='F'||c=='u'||c=='U'||c=='i'||c=='c'||c=='k'||c=='v'||c=='m'){
                        i++; 
                        
                        while(exec_line[i+1]==' ') i++;
                        continue;
                    }
                }
                cleaned[o++]=exec_line[i];
            }
            cleaned[o]='\0';
            
            char* s=cleaned;
            while(*s==' '||*s=='\t') s++;
            char* e=s+strlen(s)-1;
            while(e>s && (*e==' '||*e=='\t')) *e--='\0';
            strncpy(exec_line, s, sizeof(exec_line)-1);
        }
        if(is_terminal && exec_line[0]){
            const char* wd = workdir[0] ? workdir : NULL;
            char extra[2048];
            snprintf(extra, sizeof(extra), "-e sh -c '%s; exec ${SHELL:-/bin/sh}'", exec_line);
            if(wd) { PROFILE_END_VAR(timer, "platform_x11:Platform_LaunchApp"); return Platform_LaunchTerminalAtPath(wd, extra); }
            else {
                const char* home = Platform_GetRealHome();
                PROFILE_END_VAR(timer, "platform_x11:Platform_LaunchApp");
                return Platform_LaunchTerminalAtPath(home, extra);
            }
        }
        char* filename = strrchr(expanded, '/');
        filename = filename ? filename + 1 : expanded;
        char app_id[256];
        strncpy(app_id, filename, sizeof(app_id) - 1);
        app_id[sizeof(app_id) - 1] = '\0';
        char* ext = strstr(app_id, ".desktop");
        if (ext) *ext = '\0';
        char disp_env[256], xauth_env[512], runtime_env[512];
        snprintf(disp_env, sizeof(disp_env), "DISPLAY=%s", display);
        snprintf(xauth_env, sizeof(xauth_env), "XAUTHORITY=%s", xauthority);
        snprintf(runtime_env, sizeof(runtime_env), "XDG_RUNTIME_DIR=%s", runtime_dir);
        pid_t pid = fork();
        if (pid==0){
            setsid();
            pid_t pid2=fork();
            if(pid2<0) _exit(1);
            if(pid2>0) _exit(0);
            int fd=open("/dev/null",O_RDWR); if(fd>=0){dup2(fd,0);dup2(fd,1);dup2(fd,2); if(fd>2) close(fd);}
            if(Platform_IsRunningAsRoot()){
                char* const argv_sysd[] = {"sudo","-u",(char*)real_user,"env",disp_env,xauth_env,runtime_env,"systemd-run","--user","--scope","gtk-launch",app_id,NULL};
                execvp(argv_sysd[0], argv_sysd);
                char* const argv_fallback[] = {"sudo","-u",(char*)real_user,"env",disp_env,xauth_env,runtime_env,"gtk-launch",app_id,NULL};
                execvp(argv_fallback[0], argv_fallback);
            } else {
                setenv("DISPLAY",display,1); setenv("XAUTHORITY",xauthority,1); setenv("XDG_RUNTIME_DIR",runtime_dir,1);
                char* const argv_sysd2[] = {"systemd-run","--user","--scope","gtk-launch",app_id,NULL};
                execvp(argv_sysd2[0], argv_sysd2);
                char* const argv_fallback2[] = {"gtk-launch",app_id,NULL};
                execvp(argv_fallback2[0], argv_fallback2);
            }
            _exit(127);
        } else if(pid>0){
            int st=0; waitpid(pid,&st,0);
            if(WIFEXITED(st) && WEXITSTATUS(st)!=0 && exec_line[0]){
                log_error("[LAUNCH] gtk-launch failed for %s, trying direct Exec: %s", app_id, exec_line);
                if(is_terminal){
                    char extra2[2048];
                    snprintf(extra2, sizeof(extra2), "-e sh -c '%s; exec ${SHELL:-/bin/sh}'", exec_line);
                    PROFILE_END_VAR(timer, "platform_x11:Platform_LaunchApp");
                    return Platform_LaunchTerminalAtPath(Platform_GetRealHome(), extra2);
                } else {
                    pid_t pid3=fork();
                    if(pid3==0){
                        setsid(); pid_t p2=fork(); if(p2<0) _exit(1); if(p2>0) _exit(0);
                        int fd2=open("/dev/null",O_RDWR); if(fd2>=0){dup2(fd2,0);dup2(fd2,1);dup2(fd2,2); if(fd2>2) close(fd2);}
                        if(Platform_IsRunningAsRoot()){
                            char* const av2[]={"sudo","-u",(char*)real_user,"env",disp_env,xauth_env,runtime_env,"sh","-c",exec_line,NULL};
                            execvp(av2[0],av2);
                        } else {
                            setenv("DISPLAY",display,1); setenv("XAUTHORITY",xauthority,1); setenv("XDG_RUNTIME_DIR",runtime_dir,1);
                            execl("/bin/sh","sh","-c",exec_line,(char*)NULL);
                        }
                        _exit(127);
                    } else if(pid3>0){ int st2=0; waitpid(pid3,&st2,0); PROFILE_END_VAR(timer, "platform_x11:Platform_LaunchApp"); return WIFEXITED(st2) && WEXITSTATUS(st2)==0; }
                }
            }
            PROFILE_END_VAR(timer, "platform_x11:Platform_LaunchApp");
            return true;
        }
        PROFILE_END_VAR(timer, "platform_x11:Platform_LaunchApp");
        return false;
    } else {
        char disp_env[256], xauth_env[512], runtime_env[512];
        snprintf(disp_env, sizeof(disp_env), "DISPLAY=%s", display);
        snprintf(xauth_env, sizeof(xauth_env), "XAUTHORITY=%s", xauthority);
        snprintf(runtime_env, sizeof(runtime_env), "XDG_RUNTIME_DIR=%s", runtime_dir);
        if(Platform_IsRunningAsRoot()){
            char* const argv[] = {"sudo","-u",(char*)real_user,"env",disp_env,xauth_env,runtime_env,"sh","-c",expanded,NULL};
            PROFILE_END_VAR(timer, "platform_x11:Platform_LaunchApp");
            return daemon_spawn_sudo_env(real_user, display, xauthority, runtime_dir, argv);
        } else {
            char* const argv[] = {"sh","-c",expanded,NULL};
            bool ok = x11_daemon_exec_with_x11_env(argv, display, xauthority, runtime_dir);
            PROFILE_END_VAR(timer, "platform_x11:Platform_LaunchApp");
            return ok;
        }
    }
    PROFILE_END_VAR(timer, "platform_x11:Platform_LaunchApp");
}


bool Platform_LaunchAppAsAdmin(const char* path) {
    PROFILE_START(timer);
    if (!path) { log_error("Platform_LaunchAppAsAdmin: path is NULL"); PROFILE_END_VAR(timer, "platform_x11:Platform_LaunchAppAsAdmin"); return false; }

    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) { log_error("Platform_LaunchAppAsAdmin: Platform_ExpandPath failed for %s", path); PROFILE_END_VAR(timer, "platform_x11:Platform_LaunchAppAsAdmin"); return false; }
    path = expanded;

    pid_t pid = fork();
    if (pid < 0) { log_error("platform_x11:Platform_LaunchAppAsAdmin: operation failed"); PROFILE_END_VAR(timer, "platform_x11:Platform_LaunchAppAsAdmin"); return false; }

    if (pid == 0) {
        char *args[] = {"pkexec", (char*)path, NULL};
        
        execvp("pkexec", args);
        
        char *sudo_args[] = {"sudo", (char*)path, NULL};
        execvp("sudo", sudo_args);

        exit(1); 
    }

    PROFILE_END_VAR(timer, "platform_x11:Platform_LaunchAppAsAdmin");
    return true;
}

bool Platform_IsCtrlPressed(void) {
    PROFILE_START(timer);
    Display *display = get_display();
    if (!display) {
        log_error("platform_x11:Platform_IsCtrlPressed: operation failed"); PROFILE_END_VAR(timer, "platform_x11:Platform_IsCtrlPressed");
        return false; 
    }

    Window root, child;
    int root_x, root_y, win_x, win_y;
    unsigned int mask = 0;

    XQueryPointer(display, DefaultRootWindow(display), 
                  &root, &child, &root_x, &root_y, &win_x, &win_y, &mask);

    PROFILE_END_VAR(timer, "platform_x11:Platform_IsCtrlPressed");
    return (mask & ControlMask) != 0;
}

char* Platform_QueryZoxidePath(const char* query) {
    PROFILE_START(timer);
    if (!query) {
        log_error("Platform_QueryZoxidePath: query is NULL");
        PROFILE_END_VAR(timer, "platform_x11:Platform_QueryZoxidePath");
        return NULL;
    }

    char clean_query[512];
    strncpy(clean_query, query, sizeof(clean_query) - 1);
    clean_query[sizeof(clean_query) - 1] = '\0';
    clean_query[strcspn(clean_query, "\r\n")] = '\0';

    const char* real_user = Platform_GetRealUser();
    const char* real_home = Platform_GetRealHome();
    
    char command[1024];
    if (Platform_IsRunningAsRoot()){
        snprintf(command, sizeof(command),
                 "export HOME=\"%s\"; "
                 "export _ZO_DATA_DIR=\"%s/.local/share/zoxide\"; "
                 "export PATH=\"%s/.local/bin:%s/.cargo/bin:/usr/local/bin:$PATH\"; "
                 "sudo -u %s zoxide query -- \"%s\" 2>&1",
                 real_home, real_home, real_home, real_home, real_user, clean_query);
    } else {
        snprintf(command, sizeof(command),
                 "export HOME=\"%s\"; "
                 "export _ZO_DATA_DIR=\"%s/.local/share/zoxide\"; "
                 "export PATH=\"%s/.local/bin:%s/.cargo/bin:/usr/local/bin:$PATH\"; "
                 "zoxide query -- \"%s\" 2>&1",
                 real_home, real_home, real_home, real_home, clean_query);
    }

    FILE *pipe = popen(command, "r");
    if (!pipe) {
        perror("Zoxide Debug -> popen failed");
        log_error("platform_x11:Platform_QueryZoxidePath: returned NULL"); PROFILE_END_VAR(timer, "platform_x11:Platform_QueryZoxidePath");
        return NULL;
    }

    char temp_path[1024];
    temp_path[0] = '\0';
    if (fgets(temp_path, sizeof(temp_path) - 1, pipe) != NULL) {
        temp_path[strcspn(temp_path, "\r\n")] = '\0';
    }

    int pclose_res = pclose(pipe);

    if (pclose_res != 0 || strstr(temp_path, "no match found") || strlen(temp_path) == 0) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "Query {%s} Doesnt Have A Zoxide Path", query);
        SubjectiveMessageDisplay(buffer, 140, TimeOutForMessages, COLOR_ERROR);
        PROFILE_END_VAR(timer, "platform_x11:Platform_QueryZoxidePath");
        return NULL;
    }

    PROFILE_END_VAR(timer, "platform_x11:Platform_QueryZoxidePath");
    return strdup(temp_path);
}

void Platform_StartUniversalTimer(void) {
    PROFILE_START(timer);
    log_warn("[PLATFORM] Platform_StartUniversalTimer not implemented yet");
    PROFILE_END_VAR(timer, "platform_x11:Platform_StartUniversalTimer");
}

void Platform_ResetUniversalTimer(void) {
    PROFILE_START(timer);
    log_warn("[PLATFORM] Platform_ResetUniversalTimer not implemented yet");
    PROFILE_END_VAR(timer, "platform_x11:Platform_ResetUniversalTimer");
}

void Platform_ModifyUniversalTimer(double seconds_to_add) {
    PROFILE_START(timer);
    (void)seconds_to_add;
    log_warn("[PLATFORM] Platform_ModifyUniversalTimer not implemented yet");
    PROFILE_END_VAR(timer, "platform_x11:Platform_ModifyUniversalTimer");
}

double Platform_GetUniversalTimeSeconds(void) {
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "platform_x11:Platform_GetUniversalTimeSeconds");
    return 0.0;
}

void Platform_SaveRegionToPNG(int x1, int y1, int x2, int y2, const char* fullPath) {
    PROFILE_START(timer);
    (void)x1; (void)y1; (void)x2; (void)y2; (void)fullPath;
    log_warn("[PLATFORM] Platform_SaveRegionToPNG not implemented yet");
    PROFILE_END_VAR(timer, "platform_x11:Platform_SaveRegionToPNG");
}

void Platform_WaitForKeypressAsync(KeyCallback callback) {
    PROFILE_START(timer);
    (void)callback;
    log_warn("[PLATFORM] Platform_WaitForKeypressAsync not implemented yet on Linux");
    PROFILE_END_VAR(timer, "platform_x11:Platform_WaitForKeypressAsync");
}



static FontHandle g_cached_font = FH_NULL;
static Display* g_cached_font_dpy = NULL;
static int g_cached_font_size = 0;
static const char* g_cached_font_name = NULL;

FontHandle Platform_CreateFontOnDisplay(Display* dpy, int size, const char* face_name) {
    PROFILE_START(timer);
    if (!dpy) { PROFILE_END_VAR(timer, "platform_x11:Platform_CreateFontOnDisplay"); return FH_NULL; }

    if (g_cached_font_dpy == dpy && g_cached_font_size == size && g_cached_font_name == face_name && FH_TO_XFT(g_cached_font)) {
        PROFILE_END_VAR(timer, "platform_x11:Platform_CreateFontOnDisplay");
        return g_cached_font;
    }

    if (FH_TO_XFT(g_cached_font)) {
        XftFontClose(g_cached_font_dpy, FH_TO_XFT(g_cached_font));
        g_cached_font = FH_NULL;
    }

    int screen = DefaultScreen(dpy);
    XLockDisplay(dpy);

    XftFont* xf = XftFontOpen(dpy, screen,
                            FC_FAMILY, FcTypeString, face_name,
                            FC_PIXEL_SIZE, FcTypeDouble, (double)size,
                            NULL);
    if (!xf) {
        xf = XftFontOpen(dpy, screen,
                       FC_FAMILY, FcTypeString, "sans",
                       FC_PIXEL_SIZE, FcTypeDouble, (double)size,
                       NULL);
    }
    if (!xf) {
        xf = XftFontOpenName(dpy, screen, "fixed");
    }

    XUnlockDisplay(dpy);

    if (!xf) { PROFILE_END_VAR(timer, "platform_x11:Platform_CreateFontOnDisplay"); return FH_NULL; }

    g_cached_font.xfont = xf;
    g_cached_font.dpy = dpy;
    g_cached_font_dpy = dpy;
    g_cached_font_size = size;
    g_cached_font_name = face_name;
    PROFILE_END_VAR(timer, "platform_x11:Platform_CreateFontOnDisplay");
    return g_cached_font;
}

pthread_mutex_t g_font_mtx = PTHREAD_MUTEX_INITIALIZER;

FontHandle Platform_CreateFont(int size, const char* face_name) {
    PROFILE_START(timer);
    pthread_mutex_lock(&g_font_mtx); 

    Display* dpy = get_display();
    if (!dpy) {
        pthread_mutex_unlock(&g_font_mtx);
        PROFILE_END_VAR(timer, "platform_x11:Platform_CreateFont");
        return FH_NULL;
    }

    FontHandle fh = Platform_CreateFontOnDisplay(dpy, size, face_name);

    pthread_mutex_unlock(&g_font_mtx); 
    PROFILE_END_VAR(timer, "platform_x11:Platform_CreateFont");
    return fh;
}

void Platform_DestroyFont(FontHandle font) {
    if (FH_TO_XFT(font)) {
        if (FH_TO_DPY(font) == g_cached_font_dpy && FH_TO_XFT(font) == FH_TO_XFT(g_cached_font)) {
            g_cached_font = FH_NULL;
            g_cached_font_dpy = NULL;
            g_cached_font_size = 0;
            g_cached_font_name = NULL;
        }
        XftFontClose(FH_TO_DPY(font), FH_TO_XFT(font));
    }
}

static int xft_text_width(Display* dpy, XftFont* xf, const char* text, int len) {
    PROFILE_START(timer);
    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy, xf, (FcChar8*)text, len, &ext);
    PROFILE_END_VAR(timer, "platform_x11:xft_text_width");
    return ext.xOff;
}

void Platform_GetTextExtents(FontHandle font, const char* text, int maxWidth, int* outWidth, int* outHeight) {
    PROFILE_START(timer);
    if (!FH_TO_XFT(font) || !text) {
        *outWidth = 0;
        *outHeight = 0;
        PROFILE_END_VAR(timer, "platform_x11:Platform_GetTextExtents");
        return;
    }
    Display* dpy = FH_TO_DPY(font);
    XftFont* xf = FH_TO_XFT(font);
    int lineHeight = xf->ascent + xf->descent;
    int spaceWidth = xft_text_width(dpy, xf, " ", 1);

    int lineWidth = 0;
    int totalHeight = lineHeight;
    const char* p = text;
    while (*p) {
        const char* word_start = p;
        while (*p && *p != ' ') p++;
        int wordWidth = xft_text_width(dpy, xf, word_start, p - word_start);
        int add = wordWidth + (lineWidth > 0 ? spaceWidth : 0);
        if (lineWidth > 0 && lineWidth + add > maxWidth) {
            totalHeight += lineHeight;
            lineWidth = wordWidth;
        } else {
            lineWidth += add;
        }
        while (*p == ' ') p++;
    }
    *outWidth = lineWidth;
    *outHeight = totalHeight;
    PROFILE_END_VAR(timer, "platform_x11:Platform_GetTextExtents");
}

long Platform_GetFileSize(char *filepath) {
    PROFILE_START(timer);
    FILE *fp = fopen(filepath, "rb"); 
    if (fp == NULL) {
        log_error("platform_x11:Platform_GetFileSize: returned -1"); PROFILE_END_VAR(timer, "platform_x11:Platform_GetFileSize");
        return -1; 
    }

    if (fseek(fp, 0L, SEEK_END) != 0) {
        fclose(fp); PROFILE_END_VAR(timer, "platform_x11:Platform_GetFileSize"); return -1;
    }

    long size = ftell(fp);

    fclose(fp);
    PROFILE_END_VAR(timer, "platform_x11:Platform_GetFileSize");
    return size;
}

#define MAX_WATCHED_DIRS 128

typedef struct {
    int fd;
    int wd;
    char root_path[256];
} InstantWatch;

static InstantWatch g_instant_watches[MAX_WATCHED_DIRS];
static int g_instant_count = 0;

char* Platform_RegisterDirWatcher(const char* dir_path, void (*callback)(void)) {
    PROFILE_START(timer);
    if (!dir_path) { log_error("platform_x11:Platform_RegisterDirWatcher: returned NULL"); PROFILE_END_VAR(timer, "platform_x11:Platform_RegisterDirWatcher"); return NULL; }

    for (int i = 0; i < g_instant_count; i++) {
        if (strcmp(g_instant_watches[i].root_path, dir_path) == 0) {
            int fd = g_instant_watches[i].fd;
            char buffer[1024 * (sizeof(struct inotify_event) + 16)];
            
            int flags = fcntl(fd, F_GETFL, 0);
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);

            int length = read(fd, buffer, sizeof(buffer));
            if (length > 0) {
                int j = 0;
                while (j < length) {
                    struct inotify_event *event = (struct inotify_event *)&buffer[j];
                    
                    if (callback) {
                        callback();
                    }

                    if (event->len > 0) {
                        char *found_name = malloc(event->len + 1);
                        if (found_name) {
                            strcpy(found_name, event->name);
                            PROFILE_END_VAR(timer, "platform_x11:Platform_RegisterDirWatcher");
                            return found_name;
                        }
                    }

                    char* generic_name = malloc(32);
                    if (generic_name) {
                        strcpy(generic_name, "modified_resource");
                        PROFILE_END_VAR(timer, "platform_x11:Platform_RegisterDirWatcher");
                        return generic_name;
                    }
                    PROFILE_END_VAR(timer, "platform_x11:Platform_RegisterDirWatcher");
                    return NULL;
                }
            }
            PROFILE_END_VAR(timer, "platform_x11:Platform_RegisterDirWatcher");
            return NULL; 
        }
    }

    if (g_instant_count < MAX_WATCHED_DIRS) {
        int fd = inotify_init();
        if (fd >= 0) {
            int wd = inotify_add_watch(fd, dir_path, IN_MODIFY | IN_ATTRIB | IN_CREATE | IN_DELETE);
            if (wd >= 0) {
                g_instant_watches[g_instant_count].fd = fd;
                g_instant_watches[g_instant_count].wd = wd;
                snprintf(g_instant_watches[g_instant_count].root_path, 256, "%s", dir_path);
                g_instant_count++;
            } else {
                close(fd);
            }
        }
    }

    PROFILE_END_VAR(timer, "platform_x11:Platform_RegisterDirWatcher");
    return NULL;
}

void Platform_CleanupDirWatchers(void) {
    for (int i=0;i<g_instant_count;i++) {
        if (g_instant_watches[i].fd >= 0) {
            close(g_instant_watches[i].fd);
            g_instant_watches[i].fd = -1;
        }
    }
    g_instant_count = 0;
}

char* Platform_WatchForCFileChanges(const char* dir_path, void (*callback)(void)) {
    PROFILE_START(timer);
    if (!dir_path) { log_error("platform_x11:Platform_WatchForCFileChanges: returned NULL"); PROFILE_END_VAR(timer, "platform_x11:Platform_WatchForCFileChanges"); return NULL; }

    for (int i = 0; i < g_instant_count; i++) {
        if (strcmp(g_instant_watches[i].root_path, dir_path) == 0) {
            int fd = g_instant_watches[i].fd;
            char buffer[1024 * (sizeof(struct inotify_event) + 16)];
            
            int flags = fcntl(fd, F_GETFL, 0);
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);

            int length = read(fd, buffer, sizeof(buffer));
            if (length > 0) {
                int j = 0;
                while (j < length) {
                    struct inotify_event *event = (struct inotify_event *)&buffer[j];
                    
                    if (event->len > 0) {
                        const char* name = event->name;
                        size_t name_len = strlen(name);
                        
                        if (name_len > 2 && strcmp(name + name_len - 2, ".c") == 0) {
                            if (callback) callback();
                            
                            char *found_name = malloc(event->len + 1);
                            if (found_name) {
                                strcpy(found_name, event->name);
                                PROFILE_END_VAR(timer, "platform_x11:Platform_WatchForCFileChanges");
                                return found_name;
                            }
                        }
                    }
                    
                    j += sizeof(struct inotify_event) + event->len;
                }
            }
            PROFILE_END_VAR(timer, "platform_x11:Platform_WatchForCFileChanges");
            return NULL; 
        }
    }

    if (g_instant_count < MAX_WATCHED_DIRS) {
        int fd = inotify_init();
        if (fd >= 0) {
            int wd = inotify_add_watch(fd, dir_path, IN_MODIFY | IN_ATTRIB | IN_CREATE | IN_DELETE);
            if (wd >= 0) {
                g_instant_watches[g_instant_count].fd = fd;
                g_instant_watches[g_instant_count].wd = wd;
                snprintf(g_instant_watches[g_instant_count].root_path, 256, "%s", dir_path);
                g_instant_count++;
            } else {
                close(fd);
            }
        }
    }

    PROFILE_END_VAR(timer, "platform_x11:Platform_WatchForCFileChanges");
    return NULL;
}

bool Platform_FocusWindow(const char* title) {
  return Wm_FocusByTitle(title);
}

static int vk_to_linux_key(int vk) {
    if (vk >= 'A' && vk <= 'Z') return KEY_A + (vk - 'A');
    if (vk >= '0' && vk <= '9') return KEY_1 + (vk - '1');
    if (vk == '0') return KEY_0;
    if (vk == VK_SPACE) return KEY_SPACE;
    if (vk == VK_RETURN) return KEY_ENTER;
    if (vk == VK_BACK) return KEY_BACKSPACE;
    if (vk == VK_TAB) return KEY_TAB;
    if (vk == VK_ESCAPE) return KEY_ESC;
    if (vk == VK_OEM_1) return KEY_SEMICOLON;
    if (vk == VK_OEM_2) return KEY_SLASH;
    if (vk == VK_OEM_4) return KEY_LEFTBRACE;
    if (vk == VK_OEM_6) return KEY_RIGHTBRACE;
    if (vk == VK_OEM_COMMA) return KEY_COMMA;
    if (vk == VK_OEM_PERIOD) return KEY_DOT;
    if (vk == VK_OEM_7) return KEY_APOSTROPHE;
    return 0;
}

char* Platform_GetActiveWindowTitle() {
  return Wm_GetActiveWindowTitle();
}

int Platform_GetIconSize(char* icon_path) {
    if (strstr(icon_path, "32x32")){
      return 32;
    }
    if (strstr(icon_path, "48x48")) {
      return 48;
    }
    if (strstr(icon_path, "64x64")) {
      return 64;
    }
    if (strstr(icon_path, "128x128")) {
      return 128;
    }
    if (strstr(icon_path, "256x256")) {
      return 256;
    }
    return 32;
}
typedef struct {
  char* name;
  char* path;
} NameToPath;

NameToPath supported_names_to_paths[] =  {
  {"sway","/home/diggy/Programming/Projects/Flint/resources/Sway.svg"},
  {"anki","/home/diggy/Programming/Projects/Flint/resources/Anki.png"},
  {"xdg-desktop-portal-gtk","/home/diggy/Programming/Projects/Flint/resources/xdg-desktop-portal-gtk.png"},
};

#define ICON_CACHE_FILE "data/icon_cache.txt"
#define MAX_ICON_CACHE 1024
static struct { char key[512]; char value[1024]; } g_icon_cache[MAX_ICON_CACHE];
static int g_icon_cache_count=0;
static bool g_icon_cache_loaded=false;
static void icon_cache_load(void){
    if(g_icon_cache_loaded) return; g_icon_cache_loaded=true;
    FILE* f=fopen(ICON_CACHE_FILE,"r");
    if(!f){
        char alt[1024]; const char* h=Platform_GetRealHome(); if(!h||!h[0]) h=getenv("HOME");
        if(h){ snprintf(alt,sizeof(alt),"%s/Programming/Projects/Flint/data/icon_cache.txt",h); f=fopen(alt,"r"); }
    }
    if(!f) return;
    char line[1536];
    while(fgets(line,sizeof(line),f) && g_icon_cache_count<MAX_ICON_CACHE){
        line[strcspn(line,"\r\n")]='\0';
        char* eq=strchr(line,'='); if(!eq) continue; *eq='\0';
        strncpy(g_icon_cache[g_icon_cache_count].key, line, sizeof(g_icon_cache[0].key)-1);
        strncpy(g_icon_cache[g_icon_cache_count].value, eq+1, sizeof(g_icon_cache[0].value)-1);
        if(access(g_icon_cache[g_icon_cache_count].value,F_OK)==0) g_icon_cache_count++;
    }
    fclose(f);
}
static const char* icon_cache_get(const char* key){
    icon_cache_load();
    for(int i=0;i<g_icon_cache_count;i++) if(strcmp(g_icon_cache[i].key,key)==0) return g_icon_cache[i].value;
    return NULL;
}
static void icon_cache_set(const char* key, const char* value){
    icon_cache_load();
    for(int i=0;i<g_icon_cache_count;i++) if(strcmp(g_icon_cache[i].key,key)==0) return;
    if(g_icon_cache_count>=MAX_ICON_CACHE) return;
    strncpy(g_icon_cache[g_icon_cache_count].key, key, sizeof(g_icon_cache[0].key)-1);
    strncpy(g_icon_cache[g_icon_cache_count].value, value, sizeof(g_icon_cache[0].value)-1);
    g_icon_cache_count++;
    FILE* f=fopen(ICON_CACHE_FILE,"a");
    if(!f){
        char alt[1024]; const char* h=Platform_GetRealHome(); if(!h||!h[0]) h=getenv("HOME");
        if(h){ snprintf(alt,sizeof(alt),"%s/Programming/Projects/Flint/data/icon_cache.txt",h); f=fopen(alt,"a"); }
        if(!f) return;
    }
    fprintf(f,"%s=%s\n",key,value);
    fclose(f);
}


char* Platform_GetIconPath(char* app_path) {
    if (!app_path)     log_error("Platform_GetIconPath: operation failed, returning error");
return NULL;
    for (int i = 0; i < (int)ARRAY_LEN(supported_names_to_paths); i++) {
        if (Platform_StringContainsCaseInsensitive(app_path, supported_names_to_paths[i].name)) {
            if (access(supported_names_to_paths[i].path, F_OK)==0) return strdup(supported_names_to_paths[i].path);
        }
    }
    if (app_path[0]=='/' && strchr(app_path, '.') && access(app_path, F_OK)==0) {
        if (strstr(app_path, ".png") || strstr(app_path, ".svg") || strstr(app_path, ".xpm")) return strdup(app_path);
    }
    char icon_name[512]={0};
    if (strstr(app_path, ".desktop")) {
        FILE* f=fopen(app_path,"r");
        if (f) {
            char line[1024];
            while(fgets(line,sizeof(line),f)){
                char* p = line;
                while(*p==' ' || *p=='\t') p++;
                if (Platform_StringContainsCaseInsensitive(p, "Icon=")){
                    p += 5;
                    while(*p==' ' || *p=='\t') p++;
                    p[strcspn(p,"\r\n")]='\0';
                    strncpy(icon_name, p, sizeof(icon_name)-1);
                    break;
                }
            }
            fclose(f);
        }
        if (!icon_name[0]) {
            const char* home_ph0 = Platform_GetRealHome();
            if (!home_ph0 || !home_ph0[0]) home_ph0 = getenv("HOME");
            char ph_path[1024];
            if (home_ph0 && home_ph0[0]) {
                snprintf(ph_path,sizeof(ph_path),"%s/Programming/Projects/Flint/resources/Placeholder.png",home_ph0);
                if(access(ph_path,F_OK)==0) return strdup(ph_path);
            }
        }
    } else {
        strncpy(icon_name, app_path, sizeof(icon_name)-1);
    }
    if (icon_name[0]=='/' && access(icon_name, F_OK)==0) {
        if (strstr(icon_name, ".png") || strstr(icon_name, ".svg") || strstr(icon_name, ".xpm")) return strdup(icon_name);
    }
    char bare[512]; strncpy(bare, icon_name, sizeof(bare)-1);
    char* dot=strrchr(bare,'.'); if(dot && (strcmp(dot,".png")==0||strcmp(dot,".svg")==0||strcmp(dot,".xpm")==0)) *dot='\0';
    if (bare[0]) {
        const char* cached = icon_cache_get(bare);
        if (cached && access(cached,F_OK)==0) return strdup(cached);
    }
    const char* sizes[]={"32x32","48x48","64x64","128x128","256x256","scalable",NULL};
    const char* bases[]={"/usr/share/icons/hicolor","/usr/share/pixmaps","/usr/share/icons/Adwaita","/usr/share/icons/breeze",NULL};
    static char found[1024];
    const char* home = Platform_GetRealHome();
    if (!home || !home[0]) home = getenv("HOME");
    if (home && home[0]) {
        for(int s=0;sizes[s];s++){
            char path[1024];
            const char* sz=sizes[s];
            while(*sz==' ') sz++;
            snprintf(path,sizeof(path),"%s/.local/share/icons/hicolor/%s/apps/%s.png",home,sz,bare);
            if(access(path,F_OK)==0) { strncpy(found,path,sizeof(found)-1); icon_cache_set(bare, found); return strdup(found); }
            snprintf(path,sizeof(path),"%s/.local/share/icons/hicolor/%s/apps/%s.svg",home,sz,bare);
            if(access(path,F_OK)==0) { strncpy(found,path,sizeof(found)-1); icon_cache_set(bare, found); return strdup(found); }
        }
    }
    for(int b=0;bases[b];b++){
        for(int s=0;sizes[s];s++){
            char path[1024];
            const char* sz=sizes[s];
            while(*sz==' ') sz++;
            snprintf(path,sizeof(path),"%s/%s/apps/%s.png",bases[b],sz,bare);
            if(access(path,F_OK)==0) { strncpy(found,path,sizeof(found)-1); icon_cache_set(bare, found); return strdup(found); }
            snprintf(path,sizeof(path),"%s/%s/apps/%s.svg",bases[b],sz,bare);
            if(access(path,F_OK)==0) { strncpy(found,path,sizeof(found)-1); icon_cache_set(bare, found); return strdup(found); }
            snprintf(path,sizeof(path),"%s/%s.svg",bases[b],bare);
            if(access(path,F_OK)==0) { strncpy(found,path,sizeof(found)-1); icon_cache_set(bare, found); return strdup(found); }
            snprintf(path,sizeof(path),"%s/%s.png",bases[b],bare);
            if(access(path,F_OK)==0) { strncpy(found,path,sizeof(found)-1); icon_cache_set(bare, found); return strdup(found); }
        }
        char direct[1024]; snprintf(direct,sizeof(direct),"%s/%s.png",bases[b],bare);
        if(access(direct,F_OK)==0) { strncpy(found,direct,sizeof(found)-1); icon_cache_set(bare, found); return strdup(found); }
        snprintf(direct,sizeof(direct),"%s/%s.svg",bases[b],bare);
        if(access(direct,F_OK)==0) { strncpy(found,direct,sizeof(found)-1); icon_cache_set(bare, found); return strdup(found); }
    }
    char pix[1024]; snprintf(pix,sizeof(pix),"/usr/share/pixmaps/%s.png",bare);
    if(access(pix,F_OK)==0) { strncpy(found,pix,sizeof(found)-1); icon_cache_set(bare, found); return strdup(found); }
    snprintf(pix,sizeof(pix),"/usr/share/pixmaps/%s.svg",bare);
    if(access(pix,F_OK)==0) { strncpy(found,pix,sizeof(found)-1); icon_cache_set(bare, found); return strdup(found); }
    const char* home_ph = Platform_GetRealHome();
    if (!home_ph || !home_ph[0]) home_ph = getenv("HOME");
    if (home_ph && home_ph[0]) {
        snprintf(pix,sizeof(pix),"%s/Programming/Projects/Flint/resources/Placeholder.png", home_ph);
        if(access(pix,F_OK)==0) return strdup(pix);
    }
    snprintf(pix,sizeof(pix),"resources/Placeholder.png");
    if(access(pix,F_OK)==0) return strdup(pix);
    return strdup(bare[0] ? bare : "placeholder");
}


int Platform_CopyToClipboard(const char* text) {
    PROFILE_START(timer);
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
        log_error("[PLATFORM] Error: Failed to find a valid Linux clipboard manager.");
        PROFILE_END_VAR(timer, "flapi_x11:fl_copy_into_clipboard_platform");
        return 0;
    }

    size_t len2 = strlen(text);
    fprintf(pipe, "%s", text);
    
    status = pclose(pipe);
    PROFILE_END_VAR(timer, "flapi_x11:fl_copy_into_clipboard_platform");
    return (status == 0) ? 1 : 0;
}

bool Platform_BuildFeature(const char* src, const char* out) {
    if (!src || !out)     log_error("Platform_BuildFeature: operation failed, returning error");
return false;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "gcc -I include -I/usr/include/freetype2 -w -g -fPIC -shared -o %s %s 2>&1", out, src);
    char* outStr = Platform_RunTerminalCommand(cmd, false);
    bool ok = false;
    if (outStr) {
        if (strlen(outStr)==0 || !strstr(outStr, "error:")) ok = true;
        log_info("[BUILD] %s", outStr);
        free(outStr);
    } else {
        ok = true;
    }
    return ok;
}
