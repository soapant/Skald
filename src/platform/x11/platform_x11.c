#define _GNU_SOURCE
#include <pthread.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include "x11_display.h"

#include <sys/mman.h>
#include <sys/stat.h> 
#include <sys/wait.h>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "platform/platform.h"
#include "config.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>

// Helpers to correctly resolve real user/home even when launched via
// `sudo true; nohup ./build/main &` where SUDO_USER is not set for the nohup child.
const char* Platform_GetRealUser(void){
    static char buf[256]={0};
    static bool done=false;
    if(done) return buf[0]?buf:"diggy";
    const char* u=getenv("SUDO_USER");
    if(u && u[0] && strcmp(u,"root")!=0){ strncpy(buf,u,sizeof(buf)-1); done=true; return buf; }
    u=getenv("USER");
    if(u && u[0]){ strncpy(buf,u,sizeof(buf)-1); done=true; return buf; }
    u=getenv("LOGNAME");
    if(u && u[0]){ strncpy(buf,u,sizeof(buf)-1); done=true; return buf; }
    struct passwd* pw=getpwuid(getuid());
    if(pw && pw->pw_name){ strncpy(buf,pw->pw_name,sizeof(buf)-1); done=true; return buf; }
    strncpy(buf,"diggy",sizeof(buf)-1); done=true; return buf;
}
const char* Platform_GetRealHome(void){
    static char buf[512]={0};
    static bool done=false;
    if(done) return buf[0]?buf:"/home/diggy";
    const char* u=Platform_GetRealUser();
    struct passwd* pw=getpwnam(u);
    if(pw && pw->pw_dir){ strncpy(buf,pw->pw_dir,sizeof(buf)-1); done=true; return buf; }
    pw=getpwuid(getuid());
    if(pw && pw->pw_dir){ strncpy(buf,pw->pw_dir,sizeof(buf)-1); done=true; return buf; }
    const char* h=getenv("HOME");
    if(h && h[0]){ strncpy(buf,h,sizeof(buf)-1); done=true; return buf; }
    snprintf(buf,sizeof(buf),"/home/%s",u); done=true; return buf;
}
bool Platform_IsRunningAsRoot(void){ return getuid()==0; }

// Keep old static names for internal callers that still use them
static const char* GetRealUser(void){ return Platform_GetRealUser(); }
static const char* GetRealHome(void){ return Platform_GetRealHome(); }
static bool IsRunningAsRoot(void){ return Platform_IsRunningAsRoot(); }

bool Platform_LaunchTerminal(const char* working_dir, const char* extra_args){
    if(!working_dir) return false;
    const char* real_user = Platform_GetRealUser();
    const char* display = getenv("DISPLAY"); if(!display) display=":0";
    const char* xauthority = getenv("XAUTHORITY"); if(!xauthority) xauthority="";
    char runtime_dir[256];
    const char* env_rt=getenv("XDG_RUNTIME_DIR");
    if(env_rt) snprintf(runtime_dir,sizeof(runtime_dir),"%s",env_rt);
    else snprintf(runtime_dir,sizeof(runtime_dir),"/run/user/1000");
    const char* term = (UsedTerminal && strlen(UsedTerminal)>0) ? UsedTerminal : "alacritty";
    char disp_env[256], xauth_env[512], runtime_env[512];
    snprintf(disp_env,sizeof(disp_env),"DISPLAY=%s",display);
    snprintf(xauth_env,sizeof(xauth_env),"XAUTHORITY=%s",xauthority);
    snprintf(runtime_env,sizeof(runtime_env),"XDG_RUNTIME_DIR=%s",runtime_dir);
    // Build terminal command: terminal [--working-directory=dir] [extra_args]
    // extra_args already contains leading space if needed, e.g. " -e yazi \"/path\""
    pid_t pid=fork();
    if(pid<0) return false;
    if(pid==0){
        setsid();
        pid_t pid2=fork();
        if(pid2<0) _exit(1);
        if(pid2>0) _exit(0);
        int fd=open("/dev/null",O_RDWR); if(fd>=0){dup2(fd,0);dup2(fd,1);dup2(fd,2); if(fd>2) close(fd);}
        if(Platform_IsRunningAsRoot()){
            if(extra_args && extra_args[0]){
                char cmd[2048];
                snprintf(cmd,sizeof(cmd),"%s --working-directory=\"%s\" %s", term, working_dir, extra_args);
                char* const argv[]={"sudo","-u",(char*)real_user,"env",disp_env,xauth_env,runtime_env,"sh","-c",cmd,NULL};
                execvp(argv[0],argv);
            } else {
                char* const argv[]={"sudo","-u",(char*)real_user,"env",disp_env,xauth_env,runtime_env,(char*)term,"--working-directory", (char*)working_dir, NULL};
                execvp(argv[0],argv);
                // fallback without --working-directory
                char* const argv2[]={"sudo","-u",(char*)real_user,"env",disp_env,xauth_env,runtime_env,(char*)term,NULL};
                execvp(argv2[0],argv2);
            }
        } else {
            setenv("DISPLAY",display,1); setenv("XAUTHORITY",xauthority,1); setenv("XDG_RUNTIME_DIR",runtime_dir,1);
            if(extra_args && extra_args[0]){
                char cmd[2048];
                snprintf(cmd,sizeof(cmd),"%s --working-directory=\"%s\" %s", term, working_dir, extra_args);
                execl("/bin/sh","sh","-c",cmd,(char*)NULL);
            } else {
                execlp(term,term,"--working-directory",working_dir,(char*)NULL);
                execlp(term,term,(char*)NULL);
            }
        }
        _exit(127);
    }
    int st=0; waitpid(pid,&st,0);
    return true;
}

static bool daemon_spawn_sudo_env(const char* real_user, const char* display, const char* xauthority, const char* runtime_dir, char* const argv[]) {
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        setsid();
        pid_t pid2 = fork();
        if (pid2 < 0) _exit(1);
        if (pid2 > 0) _exit(0);
        // grandchild - detach stdio
        int fd = open("/dev/null", O_RDWR);
        if (fd >= 0) {
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            if (fd > 2) close(fd);
        }
        // set env for the sudo child (sudo -u will preserve if we use env)
        setenv("DISPLAY", display, 1);
        setenv("XAUTHORITY", xauthority, 1);
        setenv("XDG_RUNTIME_DIR", runtime_dir, 1);
        execvp(argv[0], argv);
        _exit(127);
    }
    // parent waits for intermediate child to avoid zombie
    int st=0; waitpid(pid, &st, 0);
    return true;
}

bool Platform_ShellExecuteWithVerb(const char* path, const char* verb, bool async) {
    if (!path) return false;
    (void)verb;
    const char* real_user = GetRealUser();
    const char* display = getenv("DISPLAY");
    if (!display) display = ":0";
    const char* xauthority = getenv("XAUTHORITY");
    if (!xauthority) xauthority = "";
    char runtime_dir[256];
    const char* env_runtime = getenv("XDG_RUNTIME_DIR");
    if (env_runtime) snprintf(runtime_dir, sizeof(runtime_dir), "%s", env_runtime);
    else snprintf(runtime_dir, sizeof(runtime_dir), "/run/user/1000");

    if (!async) {
        if (IsRunningAsRoot()){
            char command[2048];
            snprintf(command, sizeof(command),
                     "sudo -u %s DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s xdg-open \"%s\" > /dev/null 2>&1",
                     real_user, display, xauthority, runtime_dir, path);
            int status = system(command);
            return (status == 0);
        } else {
            char command[2048];
            snprintf(command, sizeof(command),
                     "DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s xdg-open \"%s\" > /dev/null 2>&1",
                     display, xauthority, runtime_dir, path);
            int status = system(command);
            return (status == 0);
        }
    }
    // daemonized async: fork+setsid double-fork so app outlives main
    char disp_env[256], xauth_env[512], runtime_env[512];
    snprintf(disp_env, sizeof(disp_env), "DISPLAY=%s", display);
    snprintf(xauth_env, sizeof(xauth_env), "XAUTHORITY=%s", xauthority);
    snprintf(runtime_env, sizeof(runtime_env), "XDG_RUNTIME_DIR=%s", runtime_dir);
    if (IsRunningAsRoot()){
        char* const argv[] = {"sudo","-u", (char*)real_user, "env", disp_env, xauth_env, runtime_env, "xdg-open", (char*)path, NULL};
        return daemon_spawn_sudo_env(real_user, display, xauthority, runtime_dir, argv);
    } else {
        // running as regular user (nohup case) - no sudo needed
        pid_t pid=fork();
        if(pid<0) return false;
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
        return true;
    }
}



static Window FindWindowByTitleRecursive(Display* dpy, Window w, const char* title) {
    char* name = NULL;
    if (XFetchName(dpy, w, &name) && name != NULL) {
        bool match = (strcmp(name, title) == 0);
        XFree(name);
        if (match) return w;
    }

    Window root, parent, *children = NULL;
    unsigned int n = 0;
    if (XQueryTree(dpy, w, &root, &parent, &children, &n)) {
        for (unsigned int i = 0; i < n; i++) {
            Window found = FindWindowByTitleRecursive(dpy, children[i], title);
            if (found != None) {
                if (children) XFree(children);
                return found;
            }
        }
        if (children) XFree(children);
    }
    return None;
}

static Window FindWindowByTitle(Display* dpy, const char* title) {
    return FindWindowByTitleRecursive(dpy, DefaultRootWindow(dpy), title);
}

bool Platform_SendX11Command(const char* window_title, const char* atom_name, long command) {
    Display* dpy = get_display();
    if (!dpy) return false;

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
    return ok;
}

bool Platform_SetX11Property(const char* window_title, const char* atom_name, long value) {
    Display* dpy = get_display();
    if (!dpy) return false;

    Window win = FindWindowByTitle(dpy, window_title);
    bool ok = false;
    if (win != None) {
        XChangeProperty(dpy, win, XInternAtom(dpy, atom_name, False), XA_INTEGER, 32,
                        PropModeReplace, (unsigned char*)&value, 1);
        XFlush(dpy);
        ok = true;
    }
    return ok;
}

bool Platform_SetX11StringProperty(const char* window_title, const char* atom_name, const char* value) {
    Display* dpy = get_display();
    if (!dpy) return false;

    Window win = FindWindowByTitle(dpy, window_title);
    bool ok = false;
    if (win != None && value != NULL) {
        XChangeProperty(dpy, win, XInternAtom(dpy, atom_name, False), XA_STRING, 8,
                        PropModeReplace, (const unsigned char*)value, (int)strlen(value));
        XFlush(dpy);
        ok = true;
    }
    return ok;
}

static bool ResolvePathToken(const char* token, char* out, size_t out_size) {
    for (unsigned int i = 0; i < PATH_VAR_COUNT; i++) {
        if (strcmp(G_PATH_VARS[i].name, token) == 0) {
            const char* value =
#if defined(_WIN32)
                G_PATH_VARS[i].win32_value;
#else
                G_PATH_VARS[i].linux_value;
#endif
            if (value == NULL || value[0] == '\0') return false;
            snprintf(out, out_size, "%s", value);
            return true;
        }
    }

    const char* home = getenv("HOME");

    if (strcmp(token, "HOME") == 0) {
        if (!home) return false;
        snprintf(out, out_size, "%s", home);
        return true;
    }
    if (strcmp(token, "APPDATA") == 0) {
        const char* xdg = getenv("XDG_CONFIG_HOME");
        if (xdg && xdg[0] != '\0') {
            snprintf(out, out_size, "%s", xdg);
            return true;
        }
        if (!home) return false;
        snprintf(out, out_size, "%s/.config", home);
        return true;
    }
    if (strcmp(token, "PROGRAM_FILES") == 0) {
        snprintf(out, out_size, "/usr");
        return true;
    }

    static const struct { const char* name; const char* subdir; } subdirs[] = {
        {"PICTURES", "Pictures"},
        {"DESKTOP", "Desktop"},
        {"DOCUMENTS", "Documents"},
        {"DOWNLOADS", "Downloads"},
    };
    for (size_t i = 0; i < sizeof(subdirs) / sizeof(subdirs[0]); i++) {
        if (strcmp(token, subdirs[i].name) == 0) {
            if (!home) return false;
            snprintf(out, out_size, "%s/%s", home, subdirs[i].subdir);
            return true;
        }
    }
    return false;
}

bool Platform_ExpandPath(const char* template_path, char* out, size_t out_size) {
    if (template_path == NULL || out_size == 0) return false;

    size_t o = 0;
    const char* p = template_path;
    while (*p != '\0' && o < out_size - 1) {
        if (*p == '{') {
            const char* close = strchr(p + 1, '}');
            if (close != NULL) {
                size_t token_len = (size_t)(close - p - 1);
                if (token_len > 0 && token_len < 64) {
                    char token[64];
                    memcpy(token, p + 1, token_len);
                    token[token_len] = '\0';
                    char resolved[1024];
                    if (ResolvePathToken(token, resolved, sizeof(resolved))) {
                        size_t rlen = strlen(resolved);
                        size_t room = out_size - 1 - o;
                        size_t copy = (rlen < room) ? rlen : room;
                        memcpy(out + o, resolved, copy);
                        o += copy;
                        p = close + 1;
                        continue;
                    }
                    fprintf(stderr, "[PATH] '%s' is not set, ignoring: %s\n", token, template_path);
                    return false;
                }
            }
        }
        out[o++] = *p++;
    }
    out[o] = '\0';

    if (o >= 4) {
        size_t i = o - 4;
        if (out[i] == '.' &&
            (out[i+1] == 'e' || out[i+1] == 'E') &&
            (out[i+2] == 'x' || out[i+2] == 'X') &&
            (out[i+3] == 'e' || out[i+3] == 'E')) {
            out[i] = '\0';
        }
    }
    return true;
}

void Platform_HandleShowCurrentTime(void) {
    FILE *fp;
    char buffer[1024];

    fp = popen("date", "r");

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        printf("%s \n", buffer); 
    }

    SubjectiveMessageDisplay(buffer,70,TimeOutForMessages * 3,COLOR_TEXT);

}

bool Platform_LaunchApp(const char* path) {
    if (!path) return false;
    char expanded[1024];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) return false;
    printf("Launching App Of Path %s \n",path);
    const char* real_user = GetRealUser();
    const char* display = getenv("DISPLAY");
    if (!display) display = ":0";
    const char* xauthority = getenv("XAUTHORITY");
    if (!xauthority) xauthority = "";
    char runtime_dir[256];
    const char* env_runtime = getenv("XDG_RUNTIME_DIR");
    if (env_runtime) snprintf(runtime_dir, sizeof(runtime_dir), "%s", env_runtime);
    else snprintf(runtime_dir, sizeof(runtime_dir), "/run/user/1000");

    if (strstr(expanded, ".desktop") != NULL) {
        // Robust .desktop handling: check Terminal=true and launch via terminal if needed
        bool is_terminal = false;
        char exec_line[1024] = {0};
        char workdir[1024] = {0};
        FILE* df = fopen(expanded, "r");
        if (df){
            char line[1024];
            while (fgets(line, sizeof(line), df)){
                // trim leading space
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
        // Strip field codes %f %F %u %U %i %c %k etc. from Exec
        if(exec_line[0]){
            char cleaned[1024]={0};
            size_t o=0;
            for(size_t i=0; exec_line[i] && o<sizeof(cleaned)-1; i++){
                if(exec_line[i]=='%' && exec_line[i+1]){
                    char c=exec_line[i+1];
                    if(c=='f'||c=='F'||c=='u'||c=='U'||c=='i'||c=='c'||c=='k'||c=='v'||c=='m'){
                        i++; // skip %x
                        // also skip following space if any to avoid double spaces
                        while(exec_line[i+1]==' ') i++;
                        continue;
                    }
                }
                cleaned[o++]=exec_line[i];
            }
            cleaned[o]='\0';
            // trim
            char* s=cleaned;
            while(*s==' '||*s=='\t') s++;
            char* e=s+strlen(s)-1;
            while(e>s && (*e==' '||*e=='\t')) *e--='\0';
            strncpy(exec_line, s, sizeof(exec_line)-1);
        }
        if(is_terminal && exec_line[0]){
            // Launch via terminal (alacritty via UsedTerminal)
            const char* term = (UsedTerminal && strlen(UsedTerminal)>0) ? UsedTerminal : "alacritty";
            const char* wd = workdir[0] ? workdir : NULL;
            // Build extra args: -e sh -c 'exec_line; exec $SHELL' or just -e <exec>
            // Use Platform_LaunchTerminal for proper daemon + env handling
            char extra[2048];
            // If workdir specified, use it, else no workdir
            // We need to run the exec_line inside terminal
            snprintf(extra, sizeof(extra), "-e sh -c '%s; exec ${SHELL:-/bin/sh}'", exec_line);
            // Platform_LaunchTerminal will daemonize and handle sudo/nohup
            // For .desktop terminal apps, we want to keep terminal open after command
            if(wd) return Platform_LaunchTerminal(wd, extra);
            else {
                // Use home as fallback workdir
                const char* home = GetRealHome();
                return Platform_LaunchTerminal(home, extra);
            }
        }
        // Non-terminal .desktop: try gtk-launch first
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
            if(IsRunningAsRoot()){
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
            // If gtk-launch failed (e.g., Terminal app without proper handling), fallback to Exec via terminal
            // The is_terminal check above already handled Terminal=true, but some desktops have Terminal=false yet still need terminal
            // If we are here and is_terminal was false, consider fallback to direct Exec if gtk-launch returns non-zero
            // Check exit status - if failed, try direct Exec
            if(WIFEXITED(st) && WEXITSTATUS(st)!=0 && exec_line[0]){
                // Fallback: try Exec directly (maybe path is direct binary, not desktop id)
                printf("[LAUNCH] gtk-launch failed for %s, trying direct Exec: %s\n", app_id, exec_line);
                if(is_terminal){
                    // already handled above, but just in case
                    char extra2[2048];
                    snprintf(extra2, sizeof(extra2), "-e sh -c '%s; exec ${SHELL:-/bin/sh}'", exec_line);
                    return Platform_LaunchTerminal(GetRealHome(), extra2);
                } else {
                    // Try xdg-open fallback is not needed, try sh -c
                    pid_t pid3=fork();
                    if(pid3==0){
                        setsid(); pid_t p2=fork(); if(p2<0) _exit(1); if(p2>0) _exit(0);
                        int fd2=open("/dev/null",O_RDWR); if(fd2>=0){dup2(fd2,0);dup2(fd2,1);dup2(fd2,2); if(fd2>2) close(fd2);}
                        if(IsRunningAsRoot()){
                            char* const av2[]={"sudo","-u",(char*)real_user,"env",disp_env,xauth_env,runtime_env,"sh","-c",exec_line,NULL};
                            execvp(av2[0],av2);
                        } else {
                            setenv("DISPLAY",display,1); setenv("XAUTHORITY",xauthority,1); setenv("XDG_RUNTIME_DIR",runtime_dir,1);
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
        char disp_env[256], xauth_env[512], runtime_env[512];
        snprintf(disp_env, sizeof(disp_env), "DISPLAY=%s", display);
        snprintf(xauth_env, sizeof(xauth_env), "XAUTHORITY=%s", xauthority);
        snprintf(runtime_env, sizeof(runtime_env), "XDG_RUNTIME_DIR=%s", runtime_dir);
        if(IsRunningAsRoot()){
            char* const argv[] = {"sudo","-u",(char*)real_user,"env",disp_env,xauth_env,runtime_env,"sh","-c",expanded,NULL};
            return daemon_spawn_sudo_env(real_user, display, xauthority, runtime_dir, argv);
        } else {
            pid_t pid=fork();
            if(pid<0) return false;
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
            return true;
        }
    }
}


bool Platform_LaunchAppAsAdmin(const char* path) {
    if (!path) return false;

    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) return false;
    path = expanded;

    pid_t pid = fork();
    if (pid < 0) return false;

    if (pid == 0) {
        char *args[] = {"pkexec", (char*)path, NULL};
        
        execvp("pkexec", args);
        
        char *sudo_args[] = {"sudo", (char*)path, NULL};
        execvp("sudo", sudo_args);

        exit(1); 
    }

    return true;
}

bool Platform_IsCtrlPressed(void) {
    Display *display = get_display();
    if (!display) {
        return false; 
    }

    Window root, child;
    int root_x, root_y, win_x, win_y;
    unsigned int mask = 0;

    XQueryPointer(display, DefaultRootWindow(display), 
                  &root, &child, &root_x, &root_y, &win_x, &win_y, &mask);

    return (mask & ControlMask) != 0;
}

void Linux_GetActiveWindowTitle(char* buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) return;
    buffer[0] = '\0';

    Display *display = get_display();
    if (!display) return;

    Window focus = None;
    int revert_to = 0;
    XGetInputFocus(display, &focus, &revert_to);

    if (focus != None && focus != PointerRoot) {
        char *name = NULL;
        if (XFetchName(display, focus, &name) && name) {
            strncpy(buffer, name, buffer_size - 1);
            buffer[buffer_size - 1] = '\0';
            XFree(name);
        }
    }
}

int ConvertLinuxKeyToWindowsVK(int key_code) {
    if (key_code >= KEY_1 && key_code <= KEY_0) {
        return '1' + (key_code - KEY_1);
    }

    switch (key_code) {
        case KEY_A: return 'A';
        case KEY_B: return 'B';
        case KEY_C: return 'C';
        case KEY_D: return 'D';
        case KEY_E: return 'E';
        case KEY_F: return 'F';
        case KEY_G: return 'G';
        case KEY_H: return 'H';
        case KEY_I: return 'I';
        case KEY_J: return 'J';
        case KEY_K: return 'K';
        case KEY_L: return 'L';
        case KEY_M: return 'M';
        case KEY_N: return 'N';
        case KEY_O: return 'O';
        case KEY_P: return 'P';
        case KEY_Q: return 'Q';
        case KEY_R: return 'R';
        case KEY_S: return 'S';
        case KEY_T: return 'T';
        case KEY_U: return 'U';
        case KEY_V: return 'V';
        case KEY_W: return 'W';
        case KEY_X: return 'X';
        case KEY_Y: return 'Y';
        case KEY_Z: return 'Z';
        case KEY_SPACE: return VK_SPACE;
        case KEY_ENTER: return VK_RETURN;
        case KEY_BACKSPACE: return VK_BACK;
        case KEY_TAB: return VK_TAB;
        case KEY_ESC: return VK_ESCAPE;
        case KEY_LEFT: return VK_LEFT;
        case KEY_UP: return VK_UP;
        case KEY_RIGHT: return VK_RIGHT;
        case KEY_DOWN: return VK_DOWN;
        case KEY_LEFTCTRL: return VK_LCONTROL;
        case KEY_RIGHTCTRL: return VK_RCONTROL;
        case KEY_LEFTALT: return VK_LMENU;
        case KEY_RIGHTALT: return VK_RMENU;
        case KEY_LEFTSHIFT: return VK_LSHIFT;
        case KEY_RIGHTSHIFT: return VK_RSHIFT;
        case KEY_LEFTMETA: return VK_LWIN;
        case KEY_RIGHTMETA: return VK_RWIN;
        case KEY_SEMICOLON: return VK_OEM_1;
        case KEY_SLASH: return VK_OEM_2;
        case KEY_LEFTBRACE: return VK_OEM_4;
        case KEY_COMMA: return VK_OEM_COMMA;
        case KEY_DOT: return VK_OEM_PERIOD;
        case KEY_APOSTROPHE: return VK_OEM_7;
        default: return 0;
    }
}

char* Platform_QueryZoxidePath(const char* query) {
    if (!query) {
        return NULL;
    }

    char clean_query[512];
    strncpy(clean_query, query, sizeof(clean_query) - 1);
    clean_query[sizeof(clean_query) - 1] = '\0';
    clean_query[strcspn(clean_query, "\r\n")] = '\0';

    const char* real_user = GetRealUser();
    const char* real_home = GetRealHome();
    // Use real_home for HOME and _ZO_DATA_DIR, handle both running-as-root and running-as-user
    char command[1024];
    if (IsRunningAsRoot()){
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
        return NULL;
    }

    return strdup(temp_path);
}

char Platform_GetLastChar(MappedFile *file) {
    char lastChar = '\0';

    lseek(file->fd, -1, SEEK_END);
    
    read(file->fd, &lastChar, 1);

    return lastChar;
}

int Platform_FileInit(const char* filePath, MappedFile *mf) {
    if (!mf || !filePath) return 0;

    mf->pBuffer = NULL;
    mf->fileSize = 0;

    mf->fd = open(filePath, O_RDWR | O_CREAT, 0644);
    if (mf->fd == -1) {
        perror("open failed in Platform_FileInit");
        return 0;
    }

    struct stat sb;
    if (fstat(mf->fd, &sb) == -1) {
        perror("fstat failed");
        close(mf->fd);
        mf->fd = -1;
        return 0;
    }
    
    mf->fileSize = sb.st_size;
    return 1;
}

int Platform_FileAppend(MappedFile *mf, const char *text) {
    if (mf == NULL || text == NULL) {
        return 0;
    }

    if (lseek(mf->fd, 0, SEEK_END) == -1) {
        perror("Platform_FileAppend seek failed");
        return 0;
    }

    size_t textLen = strlen(text);
    ssize_t bytesWritten = write(mf->fd, text, textLen);
    if (bytesWritten == -1 || (size_t)bytesWritten != textLen) {
        perror("Platform_FileAppend write failed");
        return 0;
    }

    struct stat sb;
    if (fstat(mf->fd, &sb) == -1) {
        perror("Platform_FileAppend fstat failed");
        return 0;
    }
    mf->fileSize = sb.st_size;

    return 1;
}

void Platform_FileClose(MappedFile *mf) {
    if (mf == NULL) {
        return;
    }

    if (mf->fd >= 0) {
        if (close(mf->fd) == -1) {
            perror("Platform_FileClose failed");
        }
        mf->fd = -1;
    }

    mf->fileSize = 0;
}

Line* Platform_FileGetLines(MappedFile *mf, size_t *outLineCount) {
    if (!mf || mf->fileSize == 0) {
        *outLineCount = 0;
        return NULL;
    }

    char *pBuffer = (char *)mmap(NULL, mf->fileSize, PROT_READ | PROT_WRITE, MAP_PRIVATE, mf->fd, 0);
    if (pBuffer == MAP_FAILED) {
        perror("mmap failed in FileGetLines");
        return NULL;
    }

    size_t maxLines = 1000;
    size_t count = 0;
    Line *list = (Line *)malloc(maxLines * sizeof(Line));
    if (list == NULL) {
        munmap(pBuffer, mf->fileSize);
        return NULL;
    }

    char *lineStart = pBuffer;
    size_t totalBytes = mf->fileSize;

    for (size_t i = 0; i < totalBytes; i++) {
        if (pBuffer[i] == '\n' || i == totalBytes - 1) {
            if (count >= maxLines) {
                maxLines *= 2;
                list = (Line *)realloc(list, maxLines * sizeof(Line));
            }
            size_t currentLineLen = &pBuffer[i] - lineStart;
            if (i == totalBytes - 1) currentLineLen++; 
            
            list[count].ptr = (char *)malloc(currentLineLen + 1);
            memcpy(list[count].ptr, lineStart, currentLineLen);
            list[count].ptr[currentLineLen] = '\0';
            
            list[count].length = currentLineLen;
            count++;
            lineStart = &pBuffer[i + 1];
        }
    }

    munmap(pBuffer, mf->fileSize);

    *outLineCount = count;
    return list;
}

char** Platform_FileGetStrings(MappedFile *mf, size_t *outLineCount, NewlineInfo **outNewlines, size_t *outNewlineCount) {
    if (!mf || mf->fileSize == 0) {
        *outLineCount = 0;
        *outNewlines = NULL;
        *outNewlineCount = 0;
        return NULL;
    }

    size_t maxLines = 1000;
    size_t count = 0;
    char **stringList = (char **)malloc(maxLines * sizeof(char*));
    if (stringList == NULL) return NULL;

    size_t maxNewlines = 2000;
    size_t newlineCount = 0;
    NewlineInfo *newlines = (NewlineInfo *)malloc(maxNewlines * sizeof(NewlineInfo));
    if (newlines == NULL) {
        free(stringList);
        return NULL;
    }

    char *pBuffer = (char *)mmap(NULL, mf->fileSize, PROT_READ | PROT_WRITE, MAP_PRIVATE, mf->fd, 0);
    if (pBuffer == MAP_FAILED) {
        perror("mmap failed in FileGetStrings");
        free(stringList);
        free(newlines);
        return NULL;
    }

    mf->pBuffer = pBuffer; 

    stringList[count++] = pBuffer;
    size_t totalBytes = mf->fileSize;

    for (size_t i = 0; i < totalBytes; i++) {
        if (pBuffer[i] == '\r') {
            char orig = pBuffer[i];
            pBuffer[i] = '\0';
            
            if (newlineCount >= maxNewlines) {
                maxNewlines *= 2;
                newlines = (NewlineInfo *)realloc(newlines, maxNewlines * sizeof(NewlineInfo));
            }
            newlines[newlineCount++] = (NewlineInfo){ .index = i, .original = orig };
        }
        else if (pBuffer[i] == '\n') {
            char orig = pBuffer[i];
            pBuffer[i] = '\0';
            
            if (newlineCount >= maxNewlines) {
                maxNewlines *= 2;
                newlines = (NewlineInfo *)realloc(newlines, maxNewlines * sizeof(NewlineInfo));
            }
            newlines[newlineCount++] = (NewlineInfo){ .index = i, .original = orig };

            if (i + 1 < totalBytes) {
                if (count >= maxLines) {
                    maxLines *= 2;
                    stringList = (char **)realloc(stringList, maxLines * sizeof(char*));
                }
                stringList[count++] = &pBuffer[i + 1];
            }
        }
    }
    
    *outLineCount = count;
    *outNewlines = newlines; 
    *outNewlineCount = newlineCount;
    return stringList;
}

void Platform_CleanUpFile(MappedFile *mf, NewlineInfo *newlines, size_t newlineCount) {
    (void)newlineCount; 
    if (!mf || !mf->pBuffer) return;

    munmap(mf->pBuffer, mf->fileSize);
    mf->pBuffer = NULL;

    if (newlines) {
        free(newlines);
    }
}

void Platform_SanitizeFile(const char *filePath) {
    int fd = open(filePath, O_RDWR);
    if (fd == -1) return;

    struct stat sb;
    if (fstat(fd, &sb) == -1 || sb.st_size == 0) {
        close(fd);
        return;
    }

    size_t fileSize = sb.st_size;
    char *buf = (char *)malloc(fileSize);
    if (!buf) { 
        close(fd); 
        return; 
    }

    ssize_t bytesRead = read(fd, buf, fileSize);
    if (bytesRead <= 0) {
        free(buf);
        close(fd);
        return;
    }

    size_t cleanCount = 0;
    for (ssize_t i = 0; i < bytesRead; i++) {
        if (buf[i] != '\0') {
            buf[cleanCount++] = buf[i];
        }
    }

    if (cleanCount < (size_t)bytesRead) {
        lseek(fd, 0, SEEK_SET);
        if (write(fd, buf, cleanCount) != -1) {
            ftruncate(fd, cleanCount); 
            printf("[SANITIZE] Cleaned %zu null bytes from %s\n", (size_t)bytesRead - cleanCount, filePath);
        }
    }

    free(buf);
    close(fd);
}

int Platform_ReplaceLineInMappedFile(MappedFile* mf, int line_index, const char* new_text) {
    if (!mf || !new_text || mf->fd < 0) return 0;

    char* old_data = (char*)malloc(mf->fileSize + 1);
    if (!old_data) return 0;

    lseek(mf->fd, 0, SEEK_SET);
    if (read(mf->fd, old_data, mf->fileSize) != (ssize_t)mf->fileSize) {
        free(old_data);
        return 0;
    }
    old_data[mf->fileSize] = '\0';

    int scratch_fd = memfd_create("replace_scratch", MFD_CLOEXEC);
    if (scratch_fd == -1) {
        free(old_data);
        return 0;
    }

    char* line_start = old_data;
    int current_line = 0;
    size_t total_bytes = mf->fileSize;

    for (size_t i = 0; i <= total_bytes; i++) {
        if (old_data[i] == '\n' || i == total_bytes) {
            size_t line_len = &old_data[i] - line_start;

            if (current_line == line_index) {
                write(scratch_fd, new_text, strlen(new_text));
            } else {
                write(scratch_fd, line_start, line_len);
            }

            if (old_data[i] == '\n') {
                write(scratch_fd, "\n", 1);
            }

            current_line++;
            line_start = &old_data[i + 1];
            
            if (i == total_bytes) break;
        }
    }

    free(old_data);

    struct stat sb;
    if (fstat(scratch_fd, &sb) == -1) {
        close(scratch_fd);
        return 0;
    }
    size_t new_size = sb.st_size;

    char* transfer_buf = (char*)malloc(new_size);
    if (!transfer_buf) {
        close(scratch_fd);
        return 0;
    }

    lseek(scratch_fd, 0, SEEK_SET);
    read(scratch_fd, transfer_buf, new_size);
    close(scratch_fd); 

    ftruncate(mf->fd, 0); 
    lseek(mf->fd, 0, SEEK_SET);
    write(mf->fd, transfer_buf, new_size);
    free(transfer_buf);

    mf->fileSize = new_size;
    return 1;
}


int Platform_CopyToClipboard(const char* text) {
    if (!text) return 0;

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
        printf("PICKED WAYLAND (display=%s runtime=%s)\n", wayland_display, runtime_dir);
    } else {
        snprintf(cmd, sizeof(cmd), "xclip -selection clipboard");
        printf("PICKED X11\n");
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

    size_t len = strlen(text);
    size_t max = 8000;  // Truncate to avoid X11 BadLength error (80KB approx safe limit)
    if (len > max) {
        fprintf(stderr, "[PLATFORM] Clipboard truncating %zu -> %zu bytes\n", len, max);
        // Write first part, then ellipsis marker
        fprintf(pipe, "%.8000s…", text);
    } else {
        fprintf(pipe, "%s", text);
    }
    
    int status = pclose(pipe);

    snprintf(buffer,sizeof(buffer),"Copied %s",text);
    SubjectiveMessageDisplay(buffer,140,TimeOutForMessages,COLOR_TEXT);

    return (status == 0) ? 1 : 0;
}

void Platform_StartUniversalTimer(void) {
    fprintf(stderr, "[PLATFORM] Platform_StartUniversalTimer not implemented yet\n");
}

void Platform_ResetUniversalTimer(void) {
    fprintf(stderr, "[PLATFORM] Platform_ResetUniversalTimer not implemented yet\n");
}

void Platform_ModifyUniversalTimer(double seconds_to_add) {
    (void)seconds_to_add;
    fprintf(stderr, "[PLATFORM] Platform_ModifyUniversalTimer not implemented yet\n");
}

double Platform_GetUniversalTimeSeconds(void) {
    return 0.0;
}

void Platform_SaveRegionToPNG(int x1, int y1, int x2, int y2, const char* fullPath) {
    (void)x1; (void)y1; (void)x2; (void)y2; (void)fullPath;
    fprintf(stderr, "[PLATFORM] Platform_SaveRegionToPNG not implemented yet\n");
}

void Platform_WaitForKeypressAsync(KeyCallback callback) {
    (void)callback;
    fprintf(stderr, "[PLATFORM] Platform_WaitForKeypressAsync not implemented yet on Linux\n");
}



FontHandle Platform_CreateFontOnDisplay(Display* dpy, int size, const char* face_name) {
    FontHandle fh = FH_NULL;
    if (!dpy) return fh;
    int screen = DefaultScreen(dpy);

    // Lock the display so other threads cannot write to the X socket simultaneously
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

    // Always unlock when finished
    XUnlockDisplay(dpy);

    if (!xf) return fh;

    fh.xfont = xf;
    fh.dpy = dpy;
    return fh;
}

pthread_mutex_t g_font_mtx = PTHREAD_MUTEX_INITIALIZER;

FontHandle Platform_CreateFont(int size, const char* face_name) {
    pthread_mutex_lock(&g_font_mtx); 

    Display* dpy = get_display();
    if (!dpy) {
        pthread_mutex_unlock(&g_font_mtx);
        return FH_NULL;
    }

    FontHandle fh = Platform_CreateFontOnDisplay(dpy, size, face_name);

    pthread_mutex_unlock(&g_font_mtx); // <-- Unlock when done
    return fh;
}

void Platform_DestroyFont(FontHandle font) {
    if (FH_TO_XFT(font)) {
        XftFontClose(FH_TO_DPY(font), FH_TO_XFT(font));
    }
}

static int xft_text_width(Display* dpy, XftFont* xf, const char* text, int len) {
    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy, xf, (FcChar8*)text, len, &ext);
    return ext.xOff;
}

void Platform_GetTextExtents(FontHandle font, const char* text, int maxWidth, int* outWidth, int* outHeight) {
    if (!FH_TO_XFT(font) || !text) {
        *outWidth = 0;
        *outHeight = 0;
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
}
