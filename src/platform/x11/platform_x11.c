#define _GNU_SOURCE
#define _XOPEN_SOURCE 500
#include <pthread.h>
#include "deps/log.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include "platform/x11/display_x11.h"

#include <sys/stat.h> 
#include <sys/wait.h>
#include <poll.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <glob.h>
#include <png.h>
#include "platform/platform.h"
#include "config.h"
#include "config.h"
#include "platform/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_text.h"
#include "helpers/helpers_ui.h"
#include "core/context.h"
#include <stdio.h>
#include <stdlib.h>
#include "platform/lib.h"
#include <time.h>
#include <sys/inotify.h>
#include <pthread.h>
#include "core/timer.h"
#include <linux/uinput.h>
#include <linux/input.h>
#include <errno.h>
#include <sys/file.h>
#include <fontconfig/fontconfig.h>
#include "platform/x11/env_helpers_x11.h"
#include "wm/wm.h"
#if __has_include(<X11/extensions/XTest.h>)
#include <X11/extensions/XTest.h>
#endif
#define NOB_IMPLEMENTATION
#define NOB_NO_ECHO
#include "../../../nob.h"


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
    if(!working_dir) { log_error("working_dir is NULL extra_args='%s'", extra_args ? extra_args : "null"); PROFILE_END_VAR(timer, "platform_x11:Platform_LaunchTerminalAtPath"); return false; }
    if (!UsedTerminal || !UsedTerminal[0]) { log_error("UsedTerminal not configured working_dir='%s' extra_args='%s'", working_dir ? working_dir : "null", extra_args ? extra_args : "null"); PROFILE_END_VAR(timer, "platform_x11:Platform_LaunchTerminalAtPath"); return false; }

    X11SessionEnv env; Env_GetSessionEnv(&env);
    char disp_env[256], xauth_env[512], runtime_env[512], wayland_env[512];
    Env_BuildEnvStrings(&env, disp_env, xauth_env, runtime_env, wayland_env);

    pid_t pid = fork();
    if(pid < 0) { log_error("fork failed: %s", strerror(errno)); PROFILE_END_VAR(timer, "platform_x11:Platform_LaunchTerminalAtPath"); return false; }

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

        if(Env_IsRunningAsRoot()){
            if(extra_args && extra_args[0]){
                char cmd[2048];
                snprintf(cmd, sizeof(cmd), "%s --working-directory=\"%s\" %s", UsedTerminal, working_dir, extra_args);
                char* const argv[] = {"sudo","-u",(char*)env.real_user,"env",disp_env,xauth_env,runtime_env,wayland_env,"sh","-c",cmd,NULL};
                execvp(argv[0], argv);
            } else {
                char* const argv[] = {"sudo","-u",(char*)env.real_user,"env",disp_env,xauth_env,runtime_env,wayland_env,(char*)UsedTerminal,"--working-directory",(char*)working_dir,NULL};
                execvp(argv[0], argv);
                char* const argv2[] = {"sudo","-u",(char*)env.real_user,"env",disp_env,xauth_env,runtime_env,wayland_env,(char*)UsedTerminal,NULL};
                execvp(argv2[0], argv2);
            }
        } else {
            Env_ApplyToCurrentProcess(&env);
            if(extra_args && extra_args[0]){
                char cmd[2048];
                //temporary ill get to it later
                snprintf(cmd, sizeof(cmd), "%s --working-directory=\"%s\" -e %s", UsedTerminal, working_dir, extra_args);
                execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
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
    if (WIFEXITED(st) && WEXITSTATUS(st)==127) {
        log_error("failed to exec terminal '%s' (extra_args='%s', dir='%s') - check UsedTerminal config and PATH", UsedTerminal, extra_args ? extra_args : "", working_dir);
    } else if (WIFEXITED(st) && WEXITSTATUS(st)!=0) {
        log_warn("terminal '%s' exited with status %d", UsedTerminal, WEXITSTATUS(st));
    }
    log_info("Platform_LaunchTerminalAtPath At Path %s",working_dir);

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
    if (!path) { log_error("path is NULL verb='%s' async=%d", verb ? verb : "null", async); PROFILE_END_VAR(timer, "platform_x11:Platform_ShellExecuteWithVerb"); return false; }
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
        bool ok = daemon_spawn_sudo_env(real_user, display, xauthority, runtime_dir, argv);
        if (!ok) { log_error("daemon_spawn failed for '%s'", path); }

        PROFILE_END_VAR(timer, "platform_x11:Platform_ShellExecuteWithVerb");
        return ok;
    } else {
        char* const argv[] = {"xdg-open", (char*)path, NULL};
        bool ok = x11_daemon_exec_with_x11_env(argv, display, xauthority, runtime_dir);
        if (!ok) { log_error("daemon_exec failed for '%s'", path); }
        log_info("Platform_ShellExecuteWithVerb At Path %s ",path);

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
    if (!dpy) { log_error("no X11 display window_title='%s' atom_name='%s' command=%ld", window_title ? window_title : "null", atom_name ? atom_name : "null", command); PROFILE_END_VAR(timer, "platform_x11:Platform_SendX11Command"); return false; }

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
    log_info("Platform_SendX11Command Title %s Command %s ",window_title,command);

    PROFILE_END_VAR(timer, "platform_x11:Platform_SendX11Command");
    return ok;
}

bool Platform_SetX11Property(const char* window_title, const char* atom_name, long value) {
    PROFILE_START(timer);
    Display* dpy = get_display();
    if (!dpy) { log_error("no X11 display window_title='%s' atom_name='%s' value=%ld", window_title ? window_title : "null", atom_name ? atom_name : "null", value); PROFILE_END_VAR(timer, "platform_x11:Platform_SetX11Property"); return false; }

    Window win = FindWindowByTitle(dpy, window_title);
    bool ok = false;
    if (win != None) {
        XChangeProperty(dpy, win, XInternAtom(dpy, atom_name, False), XA_INTEGER, 32,
                        PropModeReplace, (unsigned char*)&value, 1);
        XFlush(dpy);
        ok = true;
    }
    log_info("Platform_SetX11Property Title %s value %i ",window_title,value);

    PROFILE_END_VAR(timer, "platform_x11:Platform_SetX11Property");
    return ok;
}

bool Platform_SetX11StringProperty(const char* window_title, const char* atom_name, const char* value) {
    PROFILE_START(timer);
    Display* dpy = get_display();
    if (!dpy) { log_error("no X11 display window_title='%s' atom_name='%s' value='%s'", window_title ? window_title : "null", atom_name ? atom_name : "null", value ? value : "null"); PROFILE_END_VAR(timer, "platform_x11:Platform_SetX11StringProperty"); return false; }

    Window win = FindWindowByTitle(dpy, window_title);
    bool ok = false;
    if (win != None && value != NULL) {
        XChangeProperty(dpy, win, XInternAtom(dpy, atom_name, False), XA_STRING, 8,
                        PropModeReplace, (const unsigned char*)value, (int)strlen(value));
        XFlush(dpy);
        ok = true;
    }
    log_info("Platform_SetX11StringProperty Title %s value %i",window_title,value);

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
        log_error("malloc failed command='%s'", command ? command : "null");

        PROFILE_END_VAR(timer, "platform_x11:Platform_RunCommandAndGetOutput");
        return NULL;
    }

    fp = popen(command, "r");
    if (fp == NULL) {
        log_error("popen failed cmd='%s' err=%d", command ? command : "null", errno);
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
    if (total_size == 0) log_error("popen empty cmd='%s'", command ? command : "");
    log_info("Platform_RunCommandAndGetOutput command='%s'", command ? command : "");;

    PROFILE_END_VAR(timer, "platform_x11:Platform_RunCommandAndGetOutput");
    return output;
}

static char* Platform_RunCommandDetachedCapture(char* command) {
    int pfd[2];
    if (pipe(pfd) != 0) { log_error("pipe failed cmd='%s' err=%d", command ? command : "null", errno); return strdup(""); }
    pid_t pid = fork();
    if (pid < 0) { log_error("fork failed cmd='%s' err=%d", command ? command : "null", errno); close(pfd[0]); close(pfd[1]); return strdup(""); }
    if (pid == 0) {
        setsid();
        pid_t pid2 = fork();
        if (pid2 < 0) _exit(1);
        if (pid2 > 0) _exit(0);
        close(pfd[0]);
        signal(SIGPIPE, SIG_IGN);
        dup2(pfd[1], STDOUT_FILENO);
        dup2(pfd[1], STDERR_FILENO);
        if (pfd[1] > 2) close(pfd[1]);
        int fd = open("/dev/null", O_RDONLY);
        if (fd >= 0) { dup2(fd, STDIN_FILENO); if (fd > 2) close(fd); }
        execl("/bin/sh", "sh", "-c", command, (char*)NULL);
        _exit(127);
    }
    close(pfd[1]);
    int st = 0;
    waitpid(pid, &st, 0);
    size_t cap = 8192;
    size_t total = 0;
    char* out = malloc(cap);
    if (!out) { close(pfd[0]); return strdup(""); }
    int flags = fcntl(pfd[0], F_GETFL, 0);
    if (flags >= 0) fcntl(pfd[0], F_SETFL, flags | O_NONBLOCK);
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    long quiet_ms = 350;
    long drain_ms = 5000;
    long deadline = quiet_ms;
    bool got_data = false;
    bool eof = false;
    char buf[16096];
    while (1) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed = (now.tv_sec - start.tv_sec) * 1000L + (now.tv_nsec - start.tv_nsec) / 1000000L;
        long remain = deadline - elapsed;
        if (remain <= 0) break;
        struct pollfd p = { pfd[0], POLLIN | POLLHUP | POLLERR, 0 };
        int r = poll(&p, 1, (int)remain);
        if (r < 0) { if (errno == EINTR) continue; break; }
        if (r == 0) break;
        ssize_t n;
        bool read_any = false;
        while ((n = read(pfd[0], buf, sizeof(buf))) > 0) {
            read_any = true;
            if (!got_data) { got_data = true; deadline = elapsed + drain_ms; if (deadline < quiet_ms) deadline = quiet_ms; }
            while (total + (size_t)n + 1 > cap) {
                cap *= 2;
                char* no = realloc(out, cap);
                if (!no) { free(out); close(pfd[0]); return strdup(""); }
                out = no;
            }
            memcpy(out + total, buf, (size_t)n);
            total += (size_t)n;
            clock_gettime(CLOCK_MONOTONIC, &now);
            elapsed = (now.tv_sec - start.tv_sec) * 1000L + (now.tv_nsec - start.tv_nsec) / 1000000L;
            if (got_data) deadline = elapsed + drain_ms;
        }
        if (n == 0) { eof = true; break; }
        if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            if (p.revents & POLLHUP) { eof = true; }
            break;
        }
        if (!read_any && (p.revents & POLLHUP)) { eof = true; break; }
    }
    close(pfd[0]);
    (void)eof;
    if (total + 1 > cap) {
        char* no = realloc(out, total + 1);
        if (!no) { free(out); return strdup(""); }
        out = no;
    }
    out[total] = '\0';
    if (total == 0) log_error("capture empty cmd='%s' eof=%d", command ? command : "null", eof);
    return out;
}

bool Platform_LaunchDetached(char* command) {
    if (!command || !command[0]) return false;
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        setsid();
        pid_t pid2 = fork();
        if (pid2 < 0) _exit(1);
        if (pid2 > 0) _exit(0);
        int fd = open("/dev/null", O_RDWR);
        if (fd >= 0) {
            dup2(fd, 0);
            dup2(fd, 1);
            dup2(fd, 2);
            if (fd > 2) close(fd);
        }
        execl("/bin/sh", "sh", "-c", command, (char*)NULL);
        _exit(127);
    }
    int st = 0;
    waitpid(pid, &st, 0);
    return true;
}

char* Platform_RunTerminalCommand(char* command,bool open_terminal) {
    PROFILE_START(timer);
    char timer_text[300];
    snprintf(timer_text,sizeof(timer_text),"platform_x11:Platform_RunTerminalCommand Command:%s",command);

    const char* real_user = Platform_GetRealUser();
    const char* real_home = Platform_GetRealHome();

    if (!open_terminal) {
        char* result = Platform_RunCommandDetachedCapture(command);
        PROFILE_END_VAR(timer, timer_text);
        return result;
    }

    char temp_file[MAX_PATH];
    char cache_dir[MAX_PATH];
    snprintf(cache_dir, sizeof(cache_dir), "%s/.cache", real_home);
    snprintf(temp_file, sizeof(temp_file), "%s/.cache/temp_cmd.sh", real_home);

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

    char runtime_dir[256];
    snprintf(runtime_dir,sizeof(runtime_dir),"%s",Platform_GetRuntimeDir());
    char extra[1024];

    snprintf(extra, sizeof(extra),
             "%s -c \"%s '%s'; exec %s\"",
             UsedShell, UsedShell, temp_file, UsedShell);

    Platform_LaunchTerminalAtPath(real_home, extra);
    log_info("ran terminal command '%s' open_terminal=%d", command, open_terminal);
    PROFILE_END_VAR(timer, timer_text);
    return strdup("");
}

bool Platform_LaunchFileManagerAtPath(char* path,char* args) {
     PROFILE_START(timer);
     bool ret = Platform_ShellExecuteWithVerb(path, "open", true);
     log_info("Platform_LaunchFileManagerAtPath path='%s' args='%s'", path ? path : "", args ? args : "");;

     PROFILE_END_VAR(timer, "platform_x11:Platform_LaunchFileManagerAtPath");
     return ret;
}

bool Platform_IsCtrlPressed(void) {
    PROFILE_START(timer);
    Display *display = get_display();
    if (!display) {
 PROFILE_END_VAR(timer, "platform_x11:Platform_IsCtrlPressed");
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
        log_error("query is NULL");

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

    char* out = Platform_RunTerminalCommand(command, false);
    if (!out) {
 PROFILE_END_VAR(timer, "platform_x11:Platform_QueryZoxidePath");
        return NULL;
    }
    out[strcspn(out, "\r\n")] = '\0';

    if (strstr(out, "no match found") || strlen(out) == 0) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "Query {%s} Doesnt Have A Zoxide Path", query);
        DisplayMessageSlot(buffer, TOP_CENTER_RELATIVE, 0, 0, 0, TimeOutForMessages, COLOR_ERROR);
        free(out);
        PROFILE_END_VAR(timer, "platform_x11:Platform_QueryZoxidePath");
        return NULL;
    }

    log_info("Platform_QueryZoxidePath query='%s'", query ? query : "");;


    PROFILE_END_VAR(timer, "platform_x11:Platform_QueryZoxidePath");
    return out;
}

void Platform_StartUniversalTimer(void) {
    PROFILE_START(timer);
    log_warn("not implemented yet");
    PROFILE_END_VAR(timer, "platform_x11:Platform_StartUniversalTimer");
}

void Platform_ResetUniversalTimer(void) {
    PROFILE_START(timer);
    log_warn("not implemented yet");
    PROFILE_END_VAR(timer, "platform_x11:Platform_ResetUniversalTimer");
}

void Platform_ModifyUniversalTimer(double seconds_to_add) {
    PROFILE_START(timer);
    (void)seconds_to_add;
    log_warn("not implemented yet");
    PROFILE_END_VAR(timer, "platform_x11:Platform_ModifyUniversalTimer");
}

double Platform_GetUniversalTimeSeconds(void) {
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "platform_x11:Platform_GetUniversalTimeSeconds");
    return 0.0;
}

void Platform_WaitForKeypressAsync(KeyCallback callback) {
    PROFILE_START(timer);
    (void)callback;
    log_warn("not implemented yet");
    PROFILE_END_VAR(timer, "platform_x11:Platform_WaitForKeypressAsync");
}



#define X11_FONT_CACHE_SLOTS 8

typedef struct {
    Display* dpy;
    int size;
    char* name;
    XftFont* xft;
    unsigned long last_used;
} X11FontSlot;

static X11FontSlot g_font_slots[X11_FONT_CACHE_SLOTS];
static unsigned long g_font_tick = 0;
static pthread_mutex_t g_font_cache_mtx = PTHREAD_MUTEX_INITIALIZER;

#define X11_FONT_RETIRED 16
static Display* g_font_retired_dpy[X11_FONT_RETIRED];
static XftFont* g_font_retired_xft[X11_FONT_RETIRED];
static int g_font_retired_next = 0;

static void x11_font_retire(Display* dpy, XftFont* xft) {
    g_font_retired_dpy[g_font_retired_next] = dpy;
    g_font_retired_xft[g_font_retired_next] = xft;
    g_font_retired_next = (g_font_retired_next + 1) % X11_FONT_RETIRED;
}

static bool x11_font_was_retired(Display* dpy, XftFont* xft) {
    for (int i = 0; i < X11_FONT_RETIRED; i++) {
        if (g_font_retired_xft[i] == xft && g_font_retired_dpy[i] == dpy) return true;
    }
    return false;
}

static bool x11_font_slot_matches(const X11FontSlot* s, Display* dpy, int size, const char* face_name) {
    if (!s->xft || s->dpy != dpy || s->size != size) return false;
    if (!s->name || !face_name) return s->name == face_name;
    return strcmp(s->name, face_name) == 0;
}

static void x11_font_slot_clear(X11FontSlot* s) {
    if (s->xft) {
        XftFontClose(s->dpy, s->xft);
        x11_font_retire(s->dpy, s->xft);
    }
    if (s->name) free(s->name);
    s->dpy = NULL;
    s->size = 0;
    s->name = NULL;
    s->xft = NULL;
    s->last_used = 0;
}

static bool x11_font_is_cached(Display* dpy, XftFont* xft) {
    for (int i = 0; i < X11_FONT_CACHE_SLOTS; i++) {
        if (g_font_slots[i].xft == xft && g_font_slots[i].dpy == dpy) return true;
    }
    return false;
}

FontHandle Platform_CreateFontOnDisplay(Display* dpy, int size, const char* face_name) {
    PROFILE_START(timer);
    if (!dpy) { PROFILE_END_VAR(timer, "platform_x11:Platform_CreateFontOnDisplay"); return FH_NULL; }

    pthread_mutex_lock(&g_font_cache_mtx);

    for (int i = 0; i < X11_FONT_CACHE_SLOTS; i++) {
        if (x11_font_slot_matches(&g_font_slots[i], dpy, size, face_name)) {
            g_font_slots[i].last_used = ++g_font_tick;
            FontHandle hit = { .xfont = g_font_slots[i].xft, .dpy = dpy };
            pthread_mutex_unlock(&g_font_cache_mtx);
            PROFILE_END_VAR(timer, "platform_x11:Platform_CreateFontOnDisplay");
            return hit;
        }
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

    if (!xf) {
        pthread_mutex_unlock(&g_font_cache_mtx);
        PROFILE_END_VAR(timer, "platform_x11:Platform_CreateFontOnDisplay");
        return FH_NULL;
    }

    int slot = -1;
    unsigned long oldest = ~0UL;
    for (int i = 0; i < X11_FONT_CACHE_SLOTS; i++) {
        if (!g_font_slots[i].xft) { slot = i; break; }
        if (g_font_slots[i].last_used < oldest) { oldest = g_font_slots[i].last_used; slot = i; }
    }
    if (slot < 0) {
        XftFontClose(dpy, xf);
        pthread_mutex_unlock(&g_font_cache_mtx);
        PROFILE_END_VAR(timer, "platform_x11:Platform_CreateFontOnDisplay");
        return FH_NULL;
    }
    x11_font_slot_clear(&g_font_slots[slot]);

    g_font_slots[slot].dpy = dpy;
    g_font_slots[slot].size = size;
    g_font_slots[slot].name = face_name ? strdup(face_name) : NULL;
    g_font_slots[slot].xft = xf;
    g_font_slots[slot].last_used = ++g_font_tick;

    FontHandle made = { .xfont = xf, .dpy = dpy };
    pthread_mutex_unlock(&g_font_cache_mtx);
    PROFILE_END_VAR(timer, "platform_x11:Platform_CreateFontOnDisplay");
    return made;
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
    log_info("Platform_CreateFont size=%d face_name='%s'", size, face_name ? face_name : "");;
 
    PROFILE_END_VAR(timer, "platform_x11:Platform_CreateFont");
    return fh;
}

void Platform_DestroyFont(FontHandle font) {
    XftFont* xf = FH_TO_XFT(font);
    if (!xf) return;
    pthread_mutex_lock(&g_font_cache_mtx);
    bool cached = x11_font_is_cached(FH_TO_DPY(font), xf);
    bool retired = cached ? false : x11_font_was_retired(FH_TO_DPY(font), xf);
    pthread_mutex_unlock(&g_font_cache_mtx);
    if (cached || retired) return;
    XftFontClose(FH_TO_DPY(font), xf);
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
        log_info("Platform_GetTextExtents text='%s' maxWidth=%d", text ? text : "", maxWidth);

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
 PROFILE_END_VAR(timer, "platform_x11:Platform_GetFileSize");
        return -1; 
    }

    if (fseek(fp, 0L, SEEK_END) != 0) {
        fclose(fp); PROFILE_END_VAR(timer, "platform_x11:Platform_GetFileSize"); return -1;
    }

    long size = ftell(fp);

    fclose(fp);
    log_info("Platform_GetFileSize filepath=%p", filepath);;

    PROFILE_END_VAR(timer, "platform_x11:Platform_GetFileSize");
    return size;
}

#define MAX_WATCH_ROOTS 32
#define MAX_WATCH_PATHS 8192
#define WATCH_MAX_DEPTH 24

typedef void (*DirChangeCallback)(const char* full_path, int added);

typedef struct {
    int fd;
    int path_count;
    int index;
    bool walking;
    bool ready;
    bool thread_started;
    pthread_t thread;
    char root_path[256];
    DirChangeCallback callback;
} WatchRoot;

typedef struct {
    int wd;
    int root_index;
    char path[512];
} WatchPathEntry;

static WatchRoot g_watch_roots[MAX_WATCH_ROOTS];
static int g_watch_root_count = 0;
static WatchPathEntry g_watch_paths[MAX_WATCH_PATHS];
static int g_watch_path_count = 0;
static pthread_mutex_t g_watch_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_watch_paths_mtx = PTHREAD_MUTEX_INITIALIZER;
static int g_watch_walking = 0;
static bool g_watch_quit = false;
static int g_cfile_fd = -1;
static char g_cfile_root[256] = {0};

static void watch_record_path(int wd, int root_index, const char* abs_path) {
    for (int i = 0; i < g_watch_path_count; i++) {
        if (g_watch_paths[i].wd == wd) {
            snprintf(g_watch_paths[i].path, sizeof(g_watch_paths[i].path), "%s", abs_path);
            g_watch_paths[i].root_index = root_index;
            return;
        }
    }
    if (g_watch_path_count >= MAX_WATCH_PATHS) return;
    g_watch_paths[g_watch_path_count].wd = wd;
    g_watch_paths[g_watch_path_count].root_index = root_index;
    snprintf(g_watch_paths[g_watch_path_count].path, sizeof(g_watch_paths[g_watch_path_count].path), "%s", abs_path);
    g_watch_path_count++;
}

static const WatchPathEntry* watch_find_path(int wd) {
    for (int i = 0; i < g_watch_path_count; i++) {
        if (g_watch_paths[i].wd == wd) return &g_watch_paths[i];
    }
    return NULL;
}

static void watch_forget_path(int wd) {
    for (int i = 0; i < g_watch_path_count; i++) {
        if (g_watch_paths[i].wd == wd) {
            g_watch_paths[i] = g_watch_paths[g_watch_path_count - 1];
            g_watch_path_count--;
            return;
        }
    }
}

static int watch_add_tree(int root_index, int fd, const char* abs_path, int depth) {
    if (g_watch_quit) return 0;
    if (depth > WATCH_MAX_DEPTH) return 0;
    int added = 0;
    int wd = inotify_add_watch(fd, abs_path,
        IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVED_TO | IN_MOVED_FROM | IN_DELETE_SELF | IN_IGNORED);
    if (wd >= 0) {
        watch_record_path(wd, root_index, abs_path);
        added++;
    }
    DIR* d = opendir(abs_path);
    if (!d) return added;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (g_watch_quit) break;
        if (e->d_name[0] == '.') continue;
        char child[512];
        snprintf(child, sizeof(child), "%s/%s", abs_path, e->d_name);
        struct stat st;
        if (lstat(child, &st) != 0) continue;
        if (!S_ISDIR(st.st_mode)) continue;
        added += watch_add_tree(root_index, fd, child, depth + 1);
    }
    closedir(d);
    return added;
}

static int watch_root_index_for(const char* dir_path) {
    for (int i = 0; i < g_watch_root_count; i++) {
        if (strcmp(g_watch_roots[i].root_path, dir_path) == 0) return i;
    }
    return -1;
}

static void* watch_register_worker(void* arg) {
    WatchRoot* root = (WatchRoot*)arg;
    pthread_mutex_lock(&g_watch_paths_mtx);
    if (!g_watch_quit && root->fd >= 0) {
        int n = watch_add_tree(root->index, root->fd, root->root_path, 0);
        root->path_count = n;
        log_info("dir watch registered root='%s' dirs=%d", root->root_path, n);
    }
    pthread_mutex_unlock(&g_watch_paths_mtx);

    pthread_mutex_lock(&g_watch_mtx);
    g_watch_walking--;
    root->walking = false;
    root->ready = true;
    pthread_mutex_unlock(&g_watch_mtx);
    return NULL;
}

char* Platform_RegisterDirWatcher(const char* dir_path, DirChangeCallback callback) {
    PROFILE_START(timer);
    if (!dir_path) { log_error("returned NULL dir_path='%s'", dir_path ? dir_path : "null"); PROFILE_END_VAR(timer, "platform_x11:Platform_RegisterDirWatcher"); return NULL; }

    pthread_mutex_lock(&g_watch_mtx);

    int ri = watch_root_index_for(dir_path);
    if (ri < 0) {
        if (g_watch_root_count >= MAX_WATCH_ROOTS) {
            pthread_mutex_unlock(&g_watch_mtx);
            PROFILE_END_VAR(timer, "platform_x11:Platform_RegisterDirWatcher");
            return NULL;
        }
        int fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (fd < 0) {
            pthread_mutex_unlock(&g_watch_mtx);
            PROFILE_END_VAR(timer, "platform_x11:Platform_RegisterDirWatcher");
            return NULL;
        }
        ri = g_watch_root_count++;
        g_watch_roots[ri].fd = fd;
        g_watch_roots[ri].path_count = 0;
        g_watch_roots[ri].index = ri;
        g_watch_roots[ri].callback = callback;
        g_watch_roots[ri].walking = true;
        g_watch_roots[ri].ready = false;
        g_watch_roots[ri].thread_started = false;
        snprintf(g_watch_roots[ri].root_path, sizeof(g_watch_roots[ri].root_path), "%s", dir_path);
        g_watch_walking++;

        if (pthread_create(&g_watch_roots[ri].thread, NULL, watch_register_worker, &g_watch_roots[ri]) == 0) {
            g_watch_roots[ri].thread_started = true;
            pthread_mutex_unlock(&g_watch_mtx);
            PROFILE_END_VAR(timer, "platform_x11:Platform_RegisterDirWatcher");
            return NULL;
        }

        log_warn("dir watch thread failed, walking inline root='%s'", dir_path);
        g_watch_walking--;
        g_watch_roots[ri].walking = false;
        g_watch_roots[ri].ready = true;
        pthread_mutex_unlock(&g_watch_mtx);

        pthread_mutex_lock(&g_watch_paths_mtx);
        int n = watch_add_tree(ri, fd, dir_path, 0);
        g_watch_roots[ri].path_count = n;
        pthread_mutex_unlock(&g_watch_paths_mtx);
        log_info("dir watch registered root='%s' dirs=%d", dir_path, n);
        PROFILE_END_VAR(timer, "platform_x11:Platform_RegisterDirWatcher");
        return NULL;
    }

    if (callback) g_watch_roots[ri].callback = callback;
    bool ready = g_watch_roots[ri].ready;
    int fd = g_watch_roots[ri].fd;
    pthread_mutex_unlock(&g_watch_mtx);

    if (!ready) {
        PROFILE_END_VAR(timer, "platform_x11:Platform_RegisterDirWatcher");
        return NULL;
    }

    if (pthread_mutex_trylock(&g_watch_paths_mtx) != 0) {
        PROFILE_END_VAR(timer, "platform_x11:Platform_RegisterDirWatcher");
        return NULL;
    }

    char buffer[64 * 1024];
    char* last_name = NULL;
    for (;;) {
        int length = read(fd, buffer, sizeof(buffer));
        if (length <= 0) break;
        int j = 0;
        while (j < length) {
            struct inotify_event* event = (struct inotify_event*)&buffer[j];
            j += sizeof(struct inotify_event) + event->len;
            if (event->mask & IN_IGNORED) {
                watch_forget_path(event->wd);
                continue;
            }
            if (event->len == 0) continue;
            const WatchPathEntry* pe = watch_find_path(event->wd);
            if (!pe) continue;
            char full[640];
            snprintf(full, sizeof(full), "%s/%s", pe->path, event->name);
            if (event->mask & IN_ISDIR) {
                if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
                    struct stat st;
                    if (lstat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
                        watch_add_tree(ri, fd, full, 1);
                    }
                }
                continue;
            }
            int added = (event->mask & (IN_CREATE | IN_MOVED_TO | IN_CLOSE_WRITE)) ? 1 : 0;
            if (g_watch_roots[ri].callback) g_watch_roots[ri].callback(full, added);
            free(last_name);
            last_name = strdup(event->name);
        }
    }
    pthread_mutex_unlock(&g_watch_paths_mtx);
    PROFILE_END_VAR(timer, "platform_x11:Platform_RegisterDirWatcher");
    return last_name;
}

void Platform_CleanupDirWatchers(void) {
    pthread_mutex_lock(&g_watch_mtx);
    g_watch_quit = true;
    pthread_mutex_unlock(&g_watch_mtx);

    for (int i = 0; i < g_watch_root_count; i++) {
        if (g_watch_roots[i].thread_started) {
            pthread_join(g_watch_roots[i].thread, NULL);
            g_watch_roots[i].thread_started = false;
        }
    }

    pthread_mutex_lock(&g_watch_mtx);
    for (int i = 0; i < g_watch_root_count; i++) {
        if (g_watch_roots[i].fd >= 0) {
            close(g_watch_roots[i].fd);
            g_watch_roots[i].fd = -1;
        }
    }
    g_watch_root_count = 0;
    g_watch_walking = 0;
    g_watch_quit = false;
    pthread_mutex_unlock(&g_watch_mtx);

    pthread_mutex_lock(&g_watch_paths_mtx);
    g_watch_path_count = 0;
    pthread_mutex_unlock(&g_watch_paths_mtx);

    if (g_cfile_fd >= 0) {
        close(g_cfile_fd);
        g_cfile_fd = -1;
    }
    g_cfile_root[0] = '\0';
}

char* Platform_WatchForCFileChanges(const char* dir_path) {
    PROFILE_START(timer);
    if (!dir_path) { log_error("returned NULL dir_path='%s'", dir_path ? dir_path : "null"); PROFILE_END_VAR(timer, "platform_x11:Platform_WatchForCFileChanges"); return NULL; }

    if (g_cfile_fd < 0 || strcmp(g_cfile_root, dir_path) != 0) {
        if (g_cfile_fd >= 0) {
            close(g_cfile_fd);
            g_cfile_fd = -1;
        }
        int fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (fd < 0) {
            PROFILE_END_VAR(timer, "platform_x11:Platform_WatchForCFileChanges");
            return NULL;
        }
        int wd = inotify_add_watch(fd, dir_path, IN_MODIFY | IN_ATTRIB | IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVED_TO | IN_MOVE_SELF);
        if (wd < 0) {
            close(fd);
            PROFILE_END_VAR(timer, "platform_x11:Platform_WatchForCFileChanges");
            return NULL;
        }
        g_cfile_fd = fd;
        snprintf(g_cfile_root, sizeof(g_cfile_root), "%s", dir_path);
        PROFILE_END_VAR(timer, "platform_x11:Platform_WatchForCFileChanges");
        return NULL;
    }

    char buffer[8 * 1024];
    int length = read(g_cfile_fd, buffer, sizeof(buffer));
    if (length > 0) {
        int j = 0;
        while (j < length) {
            struct inotify_event* event = (struct inotify_event*)&buffer[j];
            j += sizeof(struct inotify_event) + event->len;
            if (event->len == 0) continue;
            const char* name = event->name;
            size_t name_len = strlen(name);
            if (name_len > 2 && strcmp(name + name_len - 2, ".c") == 0) {
                char* found_name = strdup(event->name);
                if (found_name) {
                    PROFILE_END_VAR(timer, "platform_x11:Platform_WatchForCFileChanges");
                    return found_name;
                }
            }
        }
    }
    PROFILE_END_VAR(timer, "platform_x11:Platform_WatchForCFileChanges");
    return NULL;
}

bool Platform_FocusWindow(const char* title) {
  return Wm_FocusByTitle(title);
}

bool Platform_CloseActiveWindow(void) {
  return Wm_CloseActiveWindow();
}

bool Platform_ToggleFullscreenActive(void) {
  return Wm_ToggleFullscreenActive();
}

bool Platform_MoveActiveWindowToMonitor(bool right) {
  return Wm_MoveActiveWindowToMonitor(right);
}

bool Platform_MoveActiveWindowLeft(void) {
  return Wm_MoveActiveWindowLeft();
}

bool Platform_MoveActiveWindowRight(void) {
  return Wm_MoveActiveWindowRight();
}

bool Platform_FocusWindowLeft(void) {
  return Wm_FocusWindowLeft();
}

bool Platform_FocusWindowRight(void) {
  return Wm_FocusWindowRight();
}

bool Platform_MinimizeActiveWindow(void) {
  return Wm_MinimizeActiveWindow();
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
  {"sway","/home/dog/Programming/Projects/Flint/resources/Sway.svg"},
  {"anki","/home/dog/Programming/Projects/Flint/resources/Anki.png"},
  {"xdg-desktop-portal-gtk","/home/dog/Programming/Projects/Flint/resources/xdg-desktop-portal-gtk.png"},
};

#define MAX_ICON_CACHE 1024
static struct { char key[512]; char value[1024]; } g_icon_cache[MAX_ICON_CACHE];
static int g_icon_cache_count=0;
static bool g_icon_cache_loaded=false;
static bool icon_cache_path(char* out, size_t n) {
    return Platform_GetFeaturePath("core", "icon_cache.txt", out, n);
}
static void icon_cache_load(void){
    if(g_icon_cache_loaded) return; g_icon_cache_loaded=true;
    char cf[1024];
    static char content[MAX_ICON_CACHE * 1600];
    if (!icon_cache_path(cf, sizeof(cf))) return;
    if (File_ReadAll(cf, content, (long)sizeof(content)) <= 0) return;
    char* cur = content;
    while (*cur && g_icon_cache_count < MAX_ICON_CACHE) {
        char* nl = strchr(cur, '\n');
        if (nl) *nl = '\0';
        char* line = cur;
        cur = nl ? nl + 1 : cur + strlen(cur);
        line[strcspn(line,"\r\n")]='\0';
        char* eq=strchr(line,'='); if(!eq) continue; *eq='\0';
        strncpy(g_icon_cache[g_icon_cache_count].key, line, sizeof(g_icon_cache[0].key)-1);
        g_icon_cache[g_icon_cache_count].key[sizeof(g_icon_cache[0].key)-1]='\0';
        strncpy(g_icon_cache[g_icon_cache_count].value, eq+1, sizeof(g_icon_cache[0].value)-1);
        g_icon_cache[g_icon_cache_count].value[sizeof(g_icon_cache[0].value)-1]='\0';
        if(access(g_icon_cache[g_icon_cache_count].value,F_OK)==0) g_icon_cache_count++;
    }
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
    char cf[1024];
    FILE* f=NULL;
    if (icon_cache_path(cf, sizeof(cf))) f=fopen(cf,"a");
    if(!f) return;
    fprintf(f,"%s=%s\n",key,value);
    fclose(f);
}


char* Platform_GetActiveWindowAppId(void) {
  return Wm_GetActiveWindowAppId();
}

char* Platform_GetIconPath(char* app_path) {
    if (!app_path) { log_error("operation failed, returning error app_path='%s'", app_path ? app_path : "null"); return NULL; }
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
    log_info("Platform_GetIconPath app_path='%s'", app_path ? app_path : "");;
    return strdup(bare[0] ? bare : "placeholder");
}

#include "platform/x11/clipboard_x11.h"
#include "platform/x11/clipboard_backend_x11.h"

int Platform_CopyToClipboard(const char *text) {
    if (!text) return 0;
    long long t0 = Platform_GetWallClockMs();
    int wr = 0;
    int xr = 0;
    log_info("COPY BEGIN pid=%d len=%d text='%s' WAYLAND_DISPLAY=%s HYPRLAND_INSTANCE_SIGNATURE=%s",
        (int)getpid(), (int)strlen(text), text,
        getenv("WAYLAND_DISPLAY") ? getenv("WAYLAND_DISPLAY") : "(unset)",
        getenv("HYPRLAND_INSTANCE_SIGNATURE") ? "set" : "(unset)");
    if (Platform_ClipboardAvailable()) {
        long long tw = Platform_GetWallClockMs();
        wr = Platform_ClipboardSetText(text);
        log_info("COPY step1 wayland ret=%d ms=%lld", wr, Platform_GetWallClockMs() - tw);
    } else {
        log_info("COPY step1 wayland SKIPPED (no WAYLAND_DISPLAY)");
    }
    long long tx = Platform_GetWallClockMs();
    xr = x11_clipboard_set_text(text);
    log_info("COPY step2 x11 ret=%d ms=%lld", xr, Platform_GetWallClockMs() - tx);
    if (!wr && !xr) log_warn("Platform_CopyToClipboard failed both backends");
    log_info("COPY END total_ms=%lld len=%d wr=%d xr=%d", Platform_GetWallClockMs() - t0, (int)strlen(text), wr, xr);
    return (wr || xr);
}

int Platform_CopyImageToClipboard(const unsigned char *png, size_t len) {
    if (!png || len == 0) return 0;
    int wr = 0;
    int xr = 0;
    if (Platform_ClipboardAvailable()) {
        wr = Platform_ClipboardSetImage(png, len);
        log_info("Platform_CopyImageToClipboard wayland=%d", wr);
    }
    xr = x11_clipboard_set_image(png, len);
    log_info("Platform_CopyImageToClipboard x11=%d", xr);
    if (!wr && !xr) log_warn("Platform_CopyImageToClipboard failed both backends");
    return (wr || xr);
}


char* Platform_RunArgvCapture(const char* const argv[], const char* stdin_data, int* out_status) {
    if (out_status) *out_status = -1;
    if (!argv || !argv[0]) return NULL;
    int pin[2] = {-1, -1};
    if (stdin_data && stdin_data[0]) {
        if (pipe(pin) != 0) { log_error("Platform_RunArgvCapture pipe failed: %s", strerror(errno)); return NULL; }
    }
    int pout[2] = {-1, -1};
    if (pipe(pout) != 0) {
        log_error("Platform_RunArgvCapture pipe failed: %s", strerror(errno));
        if (pin[0] >= 0) { close(pin[0]); close(pin[1]); }
        return NULL;
    }
    pid_t pid = fork();
    if (pid < 0) {
        log_error("Platform_RunArgvCapture fork failed: %s", strerror(errno));
        if (pin[0] >= 0) { close(pin[0]); close(pin[1]); }
        close(pout[0]); close(pout[1]);
        return NULL;
    }
    if (pid == 0) {
        setsid();
        if (pin[0] >= 0) { dup2(pin[0], STDIN_FILENO); close(pin[0]); close(pin[1]); }
        else {
            int devnull = open("/dev/null", O_RDONLY);
            if (devnull >= 0) { dup2(devnull, STDIN_FILENO); if (devnull > 2) close(devnull); }
        }
        dup2(pout[1], STDOUT_FILENO);
        dup2(pout[1], STDERR_FILENO);
        close(pout[0]); close(pout[1]);
        signal(SIGPIPE, SIG_DFL);
        execvp(argv[0], (char* const*)argv);
        _exit(127);
    }
    if (pin[0] >= 0) { close(pin[0]); pin[0] = -1; }
    close(pout[1]);
    if (pin[1] >= 0) {
        signal(SIGPIPE, SIG_IGN);
        size_t len = strlen(stdin_data);
        size_t off = 0;
        while (off < len) {
            ssize_t w = write(pin[1], stdin_data + off, len - off);
            if (w <= 0) { log_warn("stdin write stopped off=%d/%d err=%s", (int)off, (int)len, strerror(errno)); break; }
            off += (size_t)w;
        }
        if (len == 0 || stdin_data[len - 1] != '\n') {
            ssize_t w = write(pin[1], "\n", 1);
            (void)w;
        }
        close(pin[1]);
        pin[1] = -1;
        signal(SIGPIPE, SIG_DFL);
    }
    size_t cap = 4096, len = 0;
    char* out = malloc(cap);
    if (!out) { close(pout[0]); waitpid(pid, NULL, 0); return NULL; }
    for (;;) {
        if (len + 1024 + 1 > cap) {
            size_t ncap = cap * 2;
            char* n = realloc(out, ncap);
            if (!n) { free(out); close(pout[0]); waitpid(pid, NULL, 0); return NULL; }
            out = n;
            cap = ncap;
        }
        ssize_t r = read(pout[0], out + len, 1024);
        if (r <= 0) break;
        len += (size_t)r;
    }
    close(pout[0]);
    out[len] = '\0';
    int st = 0;
    while (waitpid(pid, &st, 0) < 0 && errno == EINTR) {}
    if (out_status) *out_status = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
    return out;
}

bool Platform_BuildFeature(const char* src, const char* out) {
    if (!src || !out) { log_error("operation failed, returning error src='%s' out='%s'", src ? src : "null", out ? out : "null"); return false; }
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s.tmp.%d", out, (int)getpid());
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "gcc -std=gnu17 -I include -I/usr/include/freetype2 -O3 -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-discarded-qualifiers -Wno-stringop-truncation -Wno-misleading-indentation -fPIC -shared -o %s %s 2>&1", tmp, src);
    char* outStr = Platform_RunTerminalCommand(cmd, false);
    bool ok = false;
    if (outStr) {
        if (strlen(outStr)==0 || (!strstr(outStr, "error:") && !strstr(outStr, "undefined reference") && !strstr(outStr, "failed"))) ok = true;
        log_info("%s", outStr);
        free(outStr);
    } else {
        ok = true;
    }
    if (!ok) { unlink(tmp); return false; }
    if (rename(tmp, out) != 0) { unlink(tmp); log_error("rename %s -> %s failed: %s", tmp, out, strerror(errno)); return false; }
    log_info("Platform_BuildFeature src='%s' out='%s'", src ? src : "", out ? out : "");;
    return true;
}

PlWindowList Platform_GetWindowTitles() {
 return Wm_GetWindowTitles();
}

PlWindowList Platform_GetWindowNames() {
  return Wm_GetWindowNames();
}

static int g_singleton_lock_fd = -1;

bool Platform_TryAcquireSingleInstance(void) {
    g_singleton_lock_fd = open("/tmp/flint.lock", O_RDWR | O_CREAT, 0666);
    if (g_singleton_lock_fd < 0) {
        log_error("Cannot open singleton lock /tmp/flint.lock (%s). If it is owned by another user, remove it: sudo rm /tmp/flint.lock", strerror(errno));
        return false;
    }
    if (flock(g_singleton_lock_fd, LOCK_EX | LOCK_NB) != 0) {
        log_info("Another instance is already running. Exiting.");
        close(g_singleton_lock_fd);
        g_singleton_lock_fd = -1;
        return false;
    }
    fcntl(g_singleton_lock_fd, F_SETFD, FD_CLOEXEC);
    return true;
}

bool Platform_InitDisplaySafe(void) {
    const char* dd = getenv("DISPLAY");
    if (!dd || !dd[0]) {
        const char* live = Platform_GetSessionDisplay();
        if (live && live[0]) setenv("DISPLAY", live, 1);
    }
    const char* xa = getenv("XAUTHORITY");
    if (xa && xa[0] && access(xa, R_OK) != 0) unsetenv("XAUTHORITY");
    const char* xa2 = getenv("XAUTHORITY");
    if (!xa2 || !xa2[0]) {
        const char* live = Platform_GetSessionXAuthority();
        if (live && live[0]) setenv("XAUTHORITY", live, 1);
    }
    const char* wd = getenv("WAYLAND_DISPLAY");
    if (wd && wd[0] && wd[0] != '/') {
        const char* rtd = getenv("XDG_RUNTIME_DIR");
        if (!rtd || !rtd[0]) rtd = Platform_GetRuntimeDir();
        char sock[512];
        snprintf(sock, sizeof(sock), "%s/%s", rtd ? rtd : "", wd);
        struct stat st;
        if (stat(sock, &st) != 0) unsetenv("WAYLAND_DISPLAY");
    }
    if (!getenv("WAYLAND_DISPLAY") || !getenv("WAYLAND_DISPLAY")[0]) {
        const char* rtd2 = Platform_GetRuntimeDir();
        if (rtd2 && rtd2[0]) {
            char pat[512];
            snprintf(pat, sizeof(pat), "%s/wayland-*", rtd2);
            glob_t g;
            memset(&g, 0, sizeof(g));
            if (glob(pat, 0, NULL, &g) == 0) {
                for (size_t i = 0; i < g.gl_pathc; i++) {
                    const char* base = strrchr(g.gl_pathv[i], '/');
                    base = base ? base + 1 : g.gl_pathv[i];
                    size_t bl = strlen(base);
                    if (bl > 5 && strcmp(base + bl - 5, ".lock") == 0) continue;
                    struct stat st2;
                    if (stat(g.gl_pathv[i], &st2) == 0) {
                        setenv("WAYLAND_DISPLAY", base, 1);
                        break;
                    }
                }
            }
            globfree(&g);
        }
    }
    if (!init_display()) {
        log_info("Continuing without X11 display (headless mode)");
        return false;
    }
    return true;
}

void Platform_CleanupDisplaySafe(void) {
    Platform_ClipboardShutdown();
    x11_clipboard_shutdown();
    cleanup_display();
}

void Platform_InitMainThreadContext(void) {
}

void Platform_FixWorkingDirectory(void) {
}
void CleanupPerFeatureVimMap(void) {
}

void Platform_CleanupPlatformResources(void) {
    CleanupPerFeatureVimMap();
    {
        extern void x11_default_apps_free_cache(void);
        x11_default_apps_free_cache();
    }
    {
        extern void x11_img_cache_free_all(void);
        x11_img_cache_free_all();
    }
    {
        extern void x11_wm_cleanup_cursor_display(void);
        x11_wm_cleanup_cursor_display();
    }
    extern void FcFini(void);
    FcFini();
}

void Platform_StartNotificationDaemon(void) {
}

int Platform_InitializeTrayIcon(void) {
  return 0; 
}
void Platform_InitUI(void) {
}
void Platform_ShutdownUI(void) {
}
PlPos Platform_GetCursorPos(void) {
    PROFILE_START(timer);
    PlPos p = Wm_GetCursorPosition();
    PROFILE_END_VAR(timer, "platform_x11:Platform_GetCursorPos");
    return p;
}

static unsigned char x11_mask_extract(unsigned long pixel, unsigned long mask) {
    if (!mask) return 0;
    int shift = 0;
    while (!(mask & 1ul)) { mask >>= 1; shift++; }
    unsigned long bits = 0;
    for (unsigned long m = mask; m; m >>= 1) bits++;
    unsigned long v = (pixel >> shift) & ((1ul << bits) - 1ul);
    if (bits == 0) return 0;
    if (bits >= 8) return (unsigned char)(v >> (bits - 8));
    return (unsigned char)((v * 255ul) / ((1ul << bits) - 1ul));
}

PlColor Platform_GetPixelColor(int x, int y) {
    PROFILE_START(timer);
    PlColor out = {0, 0, 0};
    Display* dpy = get_display();
    if (!dpy) { PROFILE_END_VAR(timer, "platform_x11:Platform_GetPixelColor"); return out; }
    Window root = DefaultRootWindow(dpy);
    XWindowAttributes attr;
    if (!XGetWindowAttributes(dpy, root, &attr) || !attr.visual) {
        PROFILE_END_VAR(timer, "platform_x11:Platform_GetPixelColor");
        return out;
    }
    XImage* img = XGetImage(dpy, root, x, y, 1, 1, AllPlanes, ZPixmap);
    if (!img) { PROFILE_END_VAR(timer, "platform_x11:Platform_GetPixelColor"); return out; }
    unsigned long pixel = XGetPixel(img, 0, 0);
    out.r = x11_mask_extract(pixel, attr.visual->red_mask);
    out.g = x11_mask_extract(pixel, attr.visual->green_mask);
    out.b = x11_mask_extract(pixel, attr.visual->blue_mask);
    XDestroyImage(img);
    PROFILE_END_VAR(timer, "platform_x11:Platform_GetPixelColor");
    return out;
}

bool Platform_MoveToPos(int x, int y) {
    PROFILE_START(timer);
    Display* dpy = get_display();
    if (!dpy) { PROFILE_END_VAR(timer, "platform_x11:Platform_MoveToPos"); return false; }
#if __has_include(<X11/extensions/XTest.h>)
    XTestFakeMotionEvent(dpy, -1, x, y, CurrentTime);
    XSync(dpy, False);
    PROFILE_END_VAR(timer, "platform_x11:Platform_MoveToPos");
    return true;
#else
    XWarpPointer(dpy, None, DefaultRootWindow(dpy), 0, 0, 0, 0, x, y);
    XSync(dpy, False);
    PROFILE_END_VAR(timer, "platform_x11:Platform_MoveToPos");
    return true;
#endif
}

bool Platform_Click(void) {
    PROFILE_START(timer);
    Display* dpy = get_display();
    if (!dpy) { PROFILE_END_VAR(timer, "platform_x11:Platform_Click"); return false; }
#if __has_include(<X11/extensions/XTest.h>)
    usleep(60000);
    XTestFakeButtonEvent(dpy, 1, True, CurrentTime);
    XSync(dpy, False);
    usleep(30000);
    XTestFakeButtonEvent(dpy, 1, False, CurrentTime);
    XSync(dpy, False);
    PROFILE_END_VAR(timer, "platform_x11:Platform_Click");
    return true;
#else
    PROFILE_END_VAR(timer, "platform_x11:Platform_Click");
    return false;
#endif
}

char* Platform_GetWorkingDir(void) {
    char* raw = Platform_RunTerminalCommand("pwd", false);
    if (!raw) return NULL;
    
    size_t len = strlen(raw);
    while (len > 0 && (raw[len - 1] == '\n' || raw[len - 1] == '\r')) {
        raw[--len] = '\0';
    }
    
    return raw;
}

void Platform_InitDataFolder() {
    char dir_path[300];
    snprintf(dir_path,sizeof(dir_path),"%s/.local/share/flint",Platform_GetRealHome());
    nob_mkdir_if_not_exists(dir_path);
}

bool Platform_GetFeaturePath(const char* feature, const char* name, char* out, size_t out_len) {
    if (!feature || !feature[0] || !name || !name[0] || !out || out_len == 0) return false;
    char dir_path[512];
    snprintf(dir_path,sizeof(dir_path),"%s/.local/share/flint",Platform_GetRealHome());
    nob_mkdir_if_not_exists(dir_path);
    char feat_path[600];
    snprintf(feat_path,sizeof(feat_path),"%s/%s",dir_path,feature);
    nob_mkdir_if_not_exists(feat_path);
    snprintf(out,out_len,"%s/%s",feat_path,name);
    return true;
}

static void sanitize_feature_stem(const char *src, char *out, size_t out_len) {
    if (!out || out_len == 0) return;
    out[0] = '\0';
    size_t w = 0;
    for (const char *p = src; *p && w + 1 < out_len; p++) {
        char c = *p;
        if (c >= 'A' && c <= 'Z') out[w++] = (char)(c + 32);
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_') out[w++] = c;
        else if (c == ' ') out[w++] = '_';
    }
    out[w] = '\0';
    if (w == 0 || strcmp(out, ".") == 0 || strcmp(out, "..") == 0) out[0] = '\0';
}

static bool lib_basename_stem(const char *base, char *out, size_t out_len) {
    char work[256];
    size_t len = strlen(base);
    if (len == 0 || len >= sizeof(work)) return false;
    memcpy(work, base, len + 1);
    for (;;) {
        if (len < 4) break;
        size_t dot_at = 0;
        for (size_t i = len; i > 0; i--) {
            if (work[i - 1] == '.') { dot_at = i - 1; break; }
        }
        if (dot_at == 0) break;
        if (strncmp(work + dot_at + 1, "hot", 3) != 0) break;
        bool digits = false;
        for (size_t i = dot_at + 4; i < len; i++) {
            if (work[i] < '0' || work[i] > '9') { digits = false; break; }
            digits = true;
        }
        if (!digits) break;
        len = dot_at;
        work[len] = '\0';
    }
    if (len >= 3 && strcmp(work + len - 3, ".so") == 0) len -= 3;
    else if (len >= 4 && strcmp(work + len - 4, ".dll") == 0) len -= 4;
    else if (len >= 5 && strcmp(work + len - 5, ".dylib") == 0) len -= 5;
    else return false;
    if (len == 0 || len >= out_len) return false;
    memcpy(out, work, len);
    out[len] = '\0';
    return true;
}

static bool feature_stem_from_caller(void *caller, char *out, size_t out_len) {
    if (!caller || !out || out_len == 0) return false;
    out[0] = '\0';
    char mod_path[512];
    if (!Platform_GetModuleOwnerPath(caller, mod_path, sizeof(mod_path))) return false;
    const char *base = strrchr(mod_path, '/');
    base = base ? base + 1 : mod_path;
    char stem[128];
    if (!lib_basename_stem(base, stem, sizeof(stem))) return false;
    sanitize_feature_stem(stem, out, out_len);
    return out[0] != '\0';
}

bool Platform_ResolveDataPath(void *caller, const char* name, char* out, size_t out_len) {
    if (!name || !name[0] || !out || out_len == 0) return false;
    for (const char* p = name; *p; p++) {
        if (*p == '/' || *p == '\\') return false;
    }
    if (strstr(name, "..")) return false;
    const char* dot = strrchr(name, '.');
    size_t stemlen = dot ? (size_t)(dot - name) : strlen(name);
    if (stemlen > 0 && stemlen < 64) {
        int stemok = 1;
        for (size_t i = 0; i < stemlen; i++) {
            char c = name[i];
            if (c >= 'A' && c <= 'Z') continue;
            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_') continue;
            stemok = 0;
            break;
        }
        if (stemok) {
            char stem[64];
            for (size_t i = 0; i < stemlen; i++) {
                char c = name[i];
                stem[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
            }
            stem[stemlen] = '\0';
            char dir_path[512];
            snprintf(dir_path, sizeof(dir_path), "%s/.local/share/flint/%s", Platform_GetRealHome(), stem);
            struct stat st;
            if (stat(dir_path, &st) == 0) {
                return Platform_GetFeaturePath(stem, name, out, out_len);
            }
        }
    }
    char from_caller[128];
    if (feature_stem_from_caller(caller, from_caller, sizeof(from_caller))) {
        return Platform_GetFeaturePath(from_caller, name, out, out_len);
    }
    const char* feat = FlintContext_Get()->window.current_feature;
    if (!feat || !feat[0]) return false;
    char safe[128];
    sanitize_feature_stem(feat, safe, sizeof(safe));
    if (safe[0] == '\0') return false;
    return Platform_GetFeaturePath(safe, name, out, out_len);
}

bool Platform_WriteTempFile(const char* name, const void* data, size_t len, char* out_path, size_t out_len) {
    if (!name || !name[0] || !out_path || out_len == 0) return false;
    if (!data && len > 0) return false;
    for (const char* p = name; *p; p++) {
        if (*p == '/' || *p == '\\') return false;
    }
    if (strstr(name, "..")) return false;
    const char* dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = "/tmp";
    snprintf(out_path, out_len, "%s/%s", dir, name);
    out_path[out_len - 1] = '\0';
    FILE* f = fopen(out_path, "wb");
    if (!f) return false;
    bool ok = true;
    if (len > 0 && fwrite(data, 1, len, f) != len) ok = false;
    if (fclose(f) != 0) ok = false;
    if (!ok) remove(out_path);
    return ok;
}

