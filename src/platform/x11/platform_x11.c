#define _GNU_SOURCE
#define _XOPEN_SOURCE 500
#include <pthread.h>
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
#include "core/platform.h"
#include "config.h"
#include "helpers.h"
#include "core/context.h"
#include "core/modal.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <pthread.h>
#include "core/timer.h"

bool Platform_LaunchTerminalAtPath(const char* working_dir, const char* extra_args){
    WinTimer timer = TimerStart();
    if(!working_dir) { TimerEnd(timer, "platform_x11:Platform_LaunchTerminalAtPath"); return false; }

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
    if(pid < 0) { TimerEnd(timer, "platform_x11:Platform_LaunchTerminalAtPath"); return false; }

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
    TimerEnd(timer, "platform_x11:Platform_LaunchTerminalAtPath");
    return true;
}

static bool daemon_spawn_sudo_env(const char* real_user, const char* display, const char* xauthority, const char* runtime_dir, char* const argv[]) {
    WinTimer timer = TimerStart();
    pid_t pid = fork();
    if (pid < 0) { TimerEnd(timer, "platform_x11:daemon_spawn_sudo_env"); return false; }
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
        setenv("DISPLAY", display, 1);
        setenv("XAUTHORITY", xauthority, 1);
        setenv("XDG_RUNTIME_DIR", runtime_dir, 1);
        execvp(argv[0], argv);
        _exit(127);
    }
    int st=0; waitpid(pid, &st, 0);
    TimerEnd(timer, "platform_x11:daemon_spawn_sudo_env");
    return true;
}

bool Platform_ShellExecuteWithVerb(const char* path, const char* verb, bool async) {
    WinTimer timer = TimerStart();
    if (!path) { TimerEnd(timer, "platform_x11:Platform_ShellExecuteWithVerb"); return false; }
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
            int status = system(command);
            TimerEnd(timer, "platform_x11:Platform_ShellExecuteWithVerb");
            return (status == 0);
        } else {
            char command[2048];
            snprintf(command, sizeof(command),
                     "DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s xdg-open \"%s\" > /dev/null 2>&1",
                     display, xauthority, runtime_dir, path);
            int status = system(command);
            TimerEnd(timer, "platform_x11:Platform_ShellExecuteWithVerb");
            return (status == 0);
        }
    }
    char disp_env[256], xauth_env[512], runtime_env[512];
    snprintf(disp_env, sizeof(disp_env), "DISPLAY=%s", display);
    snprintf(xauth_env, sizeof(xauth_env), "XAUTHORITY=%s", xauthority);
    snprintf(runtime_env, sizeof(runtime_env), "XDG_RUNTIME_DIR=%s", runtime_dir);
    if (Platform_IsRunningAsRoot()){
        char* const argv[] = {"sudo","-u", (char*)real_user, "env", disp_env, xauth_env, runtime_env, "xdg-open", (char*)path, NULL};
        TimerEnd(timer, "platform_x11:Platform_ShellExecuteWithVerb");
        return daemon_spawn_sudo_env(real_user, display, xauthority, runtime_dir, argv);
    } else {
        pid_t pid=fork();
        if(pid<0) { TimerEnd(timer, "platform_x11:Platform_ShellExecuteWithVerb"); return false; }
        if(pid==0){
            setsid();
            pid_t pid2=fork();
            if(pid2<0) _exit(1);
            if(pid2>0) _exit(0);
            int fd=open("/dev/null",O_RDWR); if(fd>=0){dup2(fd,0);dup2(fd,1);dup2(fd,2); if(fd>2) close(fd);}
            setenv("DISPLAY",display,1); setenv("XAUTHORITY",xauthority,1); setenv("XDG_RUNTIME_DIR",runtime_dir,1);
            execlp("xdg-open","xdg-open",path,(char*)NULL);
            _exit(127);
        }
        int st=0; waitpid(pid,&st,0);
        TimerEnd(timer, "platform_x11:Platform_ShellExecuteWithVerb");
        return true;
    }
    TimerEnd(timer, "platform_x11:Platform_ShellExecuteWithVerb");
}



static Window FindWindowByTitleRecursive(Display* dpy, Window w, const char* title) {
    WinTimer timer = TimerStart();
    char* name = NULL;
    if (XFetchName(dpy, w, &name) && name != NULL) {
        bool match = (strcmp(name, title) == 0);
        XFree(name);
        if (match) { TimerEnd(timer, "platform_x11:FindWindowByTitleRecursive"); return w; }
    }

    Window root, parent, *children = NULL;
    unsigned int n = 0;
    if (XQueryTree(dpy, w, &root, &parent, &children, &n)) {
        for (unsigned int i = 0; i < n; i++) {
            Window found = FindWindowByTitleRecursive(dpy, children[i], title);
            if (found != None) {
                if (children) XFree(children);
                TimerEnd(timer, "platform_x11:FindWindowByTitleRecursive");
                return found;
            }
        }
        if (children) XFree(children);
    }
    TimerEnd(timer, "platform_x11:FindWindowByTitleRecursive");
    return None;
}

static Window FindWindowByTitle(Display* dpy, const char* title) {
    WinTimer timer = TimerStart();
    TimerEnd(timer, "platform_x11:FindWindowByTitle");
    return FindWindowByTitleRecursive(dpy, DefaultRootWindow(dpy), title);
}

bool Platform_SendX11Command(const char* window_title, const char* atom_name, long command) {
    WinTimer timer = TimerStart();
    Display* dpy = get_display();
    if (!dpy) { TimerEnd(timer, "platform_x11:Platform_SendX11Command"); return false; }

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
    TimerEnd(timer, "platform_x11:Platform_SendX11Command");
    return ok;
}

bool Platform_SetX11Property(const char* window_title, const char* atom_name, long value) {
    WinTimer timer = TimerStart();
    Display* dpy = get_display();
    if (!dpy) { TimerEnd(timer, "platform_x11:Platform_SetX11Property"); return false; }

    Window win = FindWindowByTitle(dpy, window_title);
    bool ok = false;
    if (win != None) {
        XChangeProperty(dpy, win, XInternAtom(dpy, atom_name, False), XA_INTEGER, 32,
                        PropModeReplace, (unsigned char*)&value, 1);
        XFlush(dpy);
        ok = true;
    }
    TimerEnd(timer, "platform_x11:Platform_SetX11Property");
    return ok;
}

bool Platform_SetX11StringProperty(const char* window_title, const char* atom_name, const char* value) {
    WinTimer timer = TimerStart();
    Display* dpy = get_display();
    if (!dpy) { TimerEnd(timer, "platform_x11:Platform_SetX11StringProperty"); return false; }

    Window win = FindWindowByTitle(dpy, window_title);
    bool ok = false;
    if (win != None && value != NULL) {
        XChangeProperty(dpy, win, XInternAtom(dpy, atom_name, False), XA_STRING, 8,
                        PropModeReplace, (const unsigned char*)value, (int)strlen(value));
        XFlush(dpy);
        ok = true;
    }
    TimerEnd(timer, "platform_x11:Platform_SetX11StringProperty");
    return ok;
}

char* Platform_RunCommandAndGetOutput(char* command) {
    WinTimer timer = TimerStart();
    FILE* fp;
    char buffer[4096];
    char* output = malloc(1);
    size_t total_size = 0;

    if (output == NULL) {
        TimerEnd(timer, "platform_x11:Platform_RunCommandAndGetOutput");
        return NULL;
    }

    output[0] = '\0';

    fp = popen(command, "r");
    if (fp == NULL) {
        free(output);
        TimerEnd(timer, "platform_x11:Platform_RunCommandAndGetOutput");
        return strdup("");
    }

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        size_t len = strlen(buffer);

        char* new_output = realloc(output, total_size + len + 1);
        if (new_output == NULL) {
            free(output);
            pclose(fp);
            TimerEnd(timer, "platform_x11:Platform_RunCommandAndGetOutput");
            return strdup("");
        }

        output = new_output;

        memcpy(output + total_size, buffer, len);
        total_size += len;
        output[total_size] = '\0';
    }

    pclose(fp);
    TimerEnd(timer, "platform_x11:Platform_RunCommandAndGetOutput");
    return output;
}

char* Platform_RunTerminalCommand(char* command,bool open_terminal) {
    WinTimer timer = TimerStart();
    if (command == NULL || strlen(command) == 0) { TimerEnd(timer, "platform_x11:Platform_RunTerminalCommand"); return NULL; }
    
    char clean_cmd[512];
    strncpy(clean_cmd, command, sizeof(clean_cmd) - 1);
    clean_cmd[sizeof(clean_cmd) - 1] = '\0';

    const char* real_user = Platform_GetRealUser();
    const char* real_home = Platform_GetRealHome();
    char temp_file[MAX_PATH];
    char cache_dir[MAX_PATH];
    snprintf(cache_dir, sizeof(cache_dir), "%s/.cache", real_home);
    snprintf(temp_file, sizeof(temp_file), "%s/.cache/temp_cmd.sh", real_home);
    char mkdir_cmd[MAX_PATH + 64];
    if (Platform_IsRunningAsRoot())
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\" && chown %s:%s \"%s\"", cache_dir, real_user, real_user, cache_dir);
    else
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", cache_dir);
    system(mkdir_cmd);

    FILE* f = fopen(temp_file, "w");
    if (f) {
        fprintf(f, "#!/bin/%s\n",UsedShell);
        fprintf(f, "%s\n", clean_cmd);
        fclose(f);
        if (Platform_IsRunningAsRoot()){
            char chown_file[MAX_PATH + 64];
            snprintf(chown_file, sizeof(chown_file), "chown %s:%s \"%s\" && chmod 755 \"%s\"", real_user, real_user, temp_file, temp_file);
            system(chown_file);
        } else {
            chmod(temp_file, 0755);
        }
   }

    const char* display = Platform_GetSessionDisplay();
    const char* xauthority = Platform_GetSessionXAuthority();
    char runtime_dir[256];
    snprintf(runtime_dir,sizeof(runtime_dir),"%s",Platform_GetRuntimeDir());
    char extra[1024];

    if (open_terminal) {
        snprintf(extra, sizeof(extra),
                 "%s -c \"%s '%s'; exec %s\"",
                 UsedShell, UsedShell, temp_file, UsedShell);

        Platform_LaunchTerminalAtPath(real_home, extra);
        TimerEnd(timer, "platform_x11:Platform_RunTerminalCommand");
        return strdup("");
    } else {
        snprintf(extra, sizeof(extra),
                 "%s -c \"%s '%s'\"",
                 UsedShell, UsedShell, temp_file);

        char* result = Platform_RunCommandAndGetOutput(extra);
        TimerEnd(timer, "platform_x11:Platform_RunTerminalCommand");
        return result;
    }
}

bool Platform_LaunchFileManagerAtPath(char* path,char* args) {
     WinTimer timer = TimerStart();
     bool ret = Platform_ShellExecuteWithVerb(path, "open", true);
     TimerEnd(timer, "platform_x11:Platform_LaunchFileManagerAtPath");
     return ret;
}

bool Platform_LaunchApp(const char* path) {
    WinTimer timer = TimerStart();
    if (!path) { TimerEnd(timer, "platform_x11:Platform_LaunchApp"); return false; }
    char expanded[1024];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) { TimerEnd(timer, "platform_x11:Platform_LaunchApp"); return false; }
    printf("Launching App Of Path %s \n",path);
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
                if (strncasecmp(p, "Terminal=", 9)==0){
                    char* v=p+9;
                    while(*v==' '||*v=='\t') v++;
                    v[strcspn(v, "\r\n")]='\0';
                    if(strcasecmp(v,"true")==0 || strcmp(v,"1")==0) is_terminal=true;
                } else if (strncasecmp(p, "Exec=", 5)==0){
                    char* v=p+5;
                    while(*v==' '||*v=='\t') v++;
                    v[strcspn(v, "\r\n")]='\0';
                    strncpy(exec_line, v, sizeof(exec_line)-1);
                } else if (strncasecmp(p, "Path=", 5)==0){
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
            if(wd) { TimerEnd(timer, "platform_x11:Platform_LaunchApp"); return Platform_LaunchTerminalAtPath(wd, extra); }
            else {
                const char* home = Platform_GetRealHome();
                TimerEnd(timer, "platform_x11:Platform_LaunchApp");
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
                printf("[LAUNCH] gtk-launch failed for %s, trying direct Exec: %s\n", app_id, exec_line);
                if(is_terminal){
                    char extra2[2048];
                    snprintf(extra2, sizeof(extra2), "-e sh -c '%s; exec ${SHELL:-/bin/sh}'", exec_line);
                    TimerEnd(timer, "platform_x11:Platform_LaunchApp");
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
                    } else if(pid3>0){ int st2=0; waitpid(pid3,&st2,0); TimerEnd(timer, "platform_x11:Platform_LaunchApp"); return WIFEXITED(st2) && WEXITSTATUS(st2)==0; }
                }
            }
            TimerEnd(timer, "platform_x11:Platform_LaunchApp");
            return true;
        }
        TimerEnd(timer, "platform_x11:Platform_LaunchApp");
        return false;
    } else {
        char disp_env[256], xauth_env[512], runtime_env[512];
        snprintf(disp_env, sizeof(disp_env), "DISPLAY=%s", display);
        snprintf(xauth_env, sizeof(xauth_env), "XAUTHORITY=%s", xauthority);
        snprintf(runtime_env, sizeof(runtime_env), "XDG_RUNTIME_DIR=%s", runtime_dir);
        if(Platform_IsRunningAsRoot()){
            char* const argv[] = {"sudo","-u",(char*)real_user,"env",disp_env,xauth_env,runtime_env,"sh","-c",expanded,NULL};
            TimerEnd(timer, "platform_x11:Platform_LaunchApp");
            return daemon_spawn_sudo_env(real_user, display, xauthority, runtime_dir, argv);
        } else {
            pid_t pid=fork();
            if(pid<0) { TimerEnd(timer, "platform_x11:Platform_LaunchApp"); return false; }
            if(pid==0){
                setsid();
                pid_t pid2=fork();
                if(pid2<0) _exit(1);
                if(pid2>0) _exit(0);
                int fd=open("/dev/null",O_RDWR); if(fd>=0){dup2(fd,0);dup2(fd,1);dup2(fd,2); if(fd>2) close(fd);}
                setenv("DISPLAY",display,1); setenv("XAUTHORITY",xauthority,1); setenv("XDG_RUNTIME_DIR",runtime_dir,1);
                execl("/bin/sh","sh","-c",expanded,(char*)NULL);
                _exit(127);
            }
            int st=0; waitpid(pid,&st,0);
            TimerEnd(timer, "platform_x11:Platform_LaunchApp");
            return true;
        }
    }
    TimerEnd(timer, "platform_x11:Platform_LaunchApp");
}


bool Platform_LaunchAppAsAdmin(const char* path) {
    WinTimer timer = TimerStart();
    if (!path) { TimerEnd(timer, "platform_x11:Platform_LaunchAppAsAdmin"); return false; }

    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) { TimerEnd(timer, "platform_x11:Platform_LaunchAppAsAdmin"); return false; }
    path = expanded;

    pid_t pid = fork();
    if (pid < 0) { TimerEnd(timer, "platform_x11:Platform_LaunchAppAsAdmin"); return false; }

    if (pid == 0) {
        char *args[] = {"pkexec", (char*)path, NULL};
        
        execvp("pkexec", args);
        
        char *sudo_args[] = {"sudo", (char*)path, NULL};
        execvp("sudo", sudo_args);

        exit(1); 
    }

    TimerEnd(timer, "platform_x11:Platform_LaunchAppAsAdmin");
    return true;
}

bool Platform_IsCtrlPressed(void) {
    WinTimer timer = TimerStart();
    Display *display = get_display();
    if (!display) {
        TimerEnd(timer, "platform_x11:Platform_IsCtrlPressed");
        return false; 
    }

    Window root, child;
    int root_x, root_y, win_x, win_y;
    unsigned int mask = 0;

    XQueryPointer(display, DefaultRootWindow(display), 
                  &root, &child, &root_x, &root_y, &win_x, &win_y, &mask);

    TimerEnd(timer, "platform_x11:Platform_IsCtrlPressed");
    return (mask & ControlMask) != 0;
}

char* Platform_QueryZoxidePath(const char* query) {
    WinTimer timer = TimerStart();
    if (!query) {
        TimerEnd(timer, "platform_x11:Platform_QueryZoxidePath");
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
        TimerEnd(timer, "platform_x11:Platform_QueryZoxidePath");
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
        TimerEnd(timer, "platform_x11:Platform_QueryZoxidePath");
        return NULL;
    }

    TimerEnd(timer, "platform_x11:Platform_QueryZoxidePath");
    return strdup(temp_path);
}

void Platform_StartUniversalTimer(void) {
    WinTimer timer = TimerStart();
    fprintf(stderr, "[PLATFORM] Platform_StartUniversalTimer not implemented yet\n");
    TimerEnd(timer, "platform_x11:Platform_StartUniversalTimer");
}

void Platform_ResetUniversalTimer(void) {
    WinTimer timer = TimerStart();
    fprintf(stderr, "[PLATFORM] Platform_ResetUniversalTimer not implemented yet\n");
    TimerEnd(timer, "platform_x11:Platform_ResetUniversalTimer");
}

void Platform_ModifyUniversalTimer(double seconds_to_add) {
    WinTimer timer = TimerStart();
    (void)seconds_to_add;
    fprintf(stderr, "[PLATFORM] Platform_ModifyUniversalTimer not implemented yet\n");
    TimerEnd(timer, "platform_x11:Platform_ModifyUniversalTimer");
}

double Platform_GetUniversalTimeSeconds(void) {
    WinTimer timer = TimerStart();
    TimerEnd(timer, "platform_x11:Platform_GetUniversalTimeSeconds");
    return 0.0;
}

void Platform_SaveRegionToPNG(int x1, int y1, int x2, int y2, const char* fullPath) {
    WinTimer timer = TimerStart();
    (void)x1; (void)y1; (void)x2; (void)y2; (void)fullPath;
    fprintf(stderr, "[PLATFORM] Platform_SaveRegionToPNG not implemented yet\n");
    TimerEnd(timer, "platform_x11:Platform_SaveRegionToPNG");
}

void Platform_WaitForKeypressAsync(KeyCallback callback) {
    WinTimer timer = TimerStart();
    (void)callback;
    fprintf(stderr, "[PLATFORM] Platform_WaitForKeypressAsync not implemented yet on Linux\n");
    TimerEnd(timer, "platform_x11:Platform_WaitForKeypressAsync");
}



FontHandle Platform_CreateFontOnDisplay(Display* dpy, int size, const char* face_name) {
    WinTimer timer = TimerStart();
    FontHandle fh = FH_NULL;
    if (!dpy) { TimerEnd(timer, "platform_x11:Platform_CreateFontOnDisplay"); return fh; }
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

    if (!xf) { TimerEnd(timer, "platform_x11:Platform_CreateFontOnDisplay"); return fh; }

    fh.xfont = xf;
    fh.dpy = dpy;
    TimerEnd(timer, "platform_x11:Platform_CreateFontOnDisplay");
    return fh;
}

pthread_mutex_t g_font_mtx = PTHREAD_MUTEX_INITIALIZER;

FontHandle Platform_CreateFont(int size, const char* face_name) {
    WinTimer timer = TimerStart();
    pthread_mutex_lock(&g_font_mtx); 

    Display* dpy = get_display();
    if (!dpy) {
        pthread_mutex_unlock(&g_font_mtx);
        TimerEnd(timer, "platform_x11:Platform_CreateFont");
        return FH_NULL;
    }

    FontHandle fh = Platform_CreateFontOnDisplay(dpy, size, face_name);

    pthread_mutex_unlock(&g_font_mtx); 
    TimerEnd(timer, "platform_x11:Platform_CreateFont");
    return fh;
}

void Platform_DestroyFont(FontHandle font) {
    WinTimer timer = TimerStart();
    if (FH_TO_XFT(font)) {
        XftFontClose(FH_TO_DPY(font), FH_TO_XFT(font));
    }
    TimerEnd(timer, "platform_x11:Platform_DestroyFont");
}

static int xft_text_width(Display* dpy, XftFont* xf, const char* text, int len) {
    WinTimer timer = TimerStart();
    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy, xf, (FcChar8*)text, len, &ext);
    TimerEnd(timer, "platform_x11:xft_text_width");
    return ext.xOff;
}

void Platform_GetTextExtents(FontHandle font, const char* text, int maxWidth, int* outWidth, int* outHeight) {
    WinTimer timer = TimerStart();
    if (!FH_TO_XFT(font) || !text) {
        *outWidth = 0;
        *outHeight = 0;
        TimerEnd(timer, "platform_x11:Platform_GetTextExtents");
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
    TimerEnd(timer, "platform_x11:Platform_GetTextExtents");
}

long Platform_GetFileSize(char *filepath) {
    WinTimer timer = TimerStart();
    FILE *fp = fopen(filepath, "rb"); 
    if (fp == NULL) {
        TimerEnd(timer, "platform_x11:Platform_GetFileSize");
        return -1; 
    }

    if (fseek(fp, 0L, SEEK_END) != 0) {
        fclose(fp); TimerEnd(timer, "platform_x11:Platform_GetFileSize"); return -1;
    }

    long size = ftell(fp);

    fclose(fp);
    TimerEnd(timer, "platform_x11:Platform_GetFileSize");
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
    WinTimer timer = TimerStart();
    if (!dir_path) { TimerEnd(timer, "platform_x11:Platform_RegisterDirWatcher"); return NULL; }

    for (int i = 0; i < g_instant_count; i++) {
        if (strcmp(g_instant_watches[i].root_path, dir_path) == 0) {
            int fd = g_instant_watches[i].fd;
            char buffer[1024 * (sizeof(struct inotify_event) + 16)];
            
            // Set the file descriptor to non-blocking so it returns immediately
            int flags = fcntl(fd, F_GETFL, 0);
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);

            int length = read(fd, buffer, sizeof(buffer));
            if (length > 0) {
                int j = 0;
                while (j < length) {
                    struct inotify_event *event = (struct inotify_event *)&buffer[j];
                    
                    // Run the user's callback immediately on change detection
                    if (callback) {
                        callback();
                    }

                    // Dynamically allocate and return the altered object filename
                    if (event->len > 0) {
                        char *found_name = malloc(event->len + 1);
                        if (found_name) {
                            strcpy(found_name, event->name);
                            TimerEnd(timer, "platform_x11:Platform_RegisterDirWatcher");
                            return found_name;
                        }
                    }
                    // Fallback if event name length field is zero but an attribute shifted
                    char* generic_name = malloc(32);
                    if (generic_name) {
                        strcpy(generic_name, "modified_resource");
                        TimerEnd(timer, "platform_x11:Platform_RegisterDirWatcher");
                        return generic_name;
                    }
                    TimerEnd(timer, "platform_x11:Platform_RegisterDirWatcher");
                    return NULL;
                }
            }
            TimerEnd(timer, "platform_x11:Platform_RegisterDirWatcher");
            return NULL; 
        }
    }

    // Lazy initialization configuration setup on the first loop encounter
    if (g_instant_count < MAX_WATCHED_DIRS) {
        int fd = inotify_init();
        if (fd >= 0) {
            // Monitor modifications, attribute adjustments (mtime changes on inner folders), creation, and deletions
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

    TimerEnd(timer, "platform_x11:Platform_RegisterDirWatcher");
    return NULL;
}

