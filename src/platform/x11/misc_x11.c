#define _GNU_SOURCE
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <dirent.h>
#include <errno.h>
#include <ctype.h>
#include <linux/input.h>

#include "core/context.h"
#include "core/modal.h"
#include "config.h"
#include "helpers.h"
#include "core/platform.h"
#include "core/timer.h"


static uid_t ProbeSessionUid(void){
    WinTimer timer = TimerStart();
    DIR* d = opendir("/tmp/.X11-unix");
    if (!d) { TimerEnd(timer, "misc_x11:ProbeSessionUid"); return (uid_t)-1; }
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] != 'X') continue;
        char path[512];
        snprintf(path, sizeof(path), "/tmp/.X11-unix/%s", e->d_name);
        struct stat st;
        if (stat(path, &st) == 0 && st.st_uid != 0) {
            closedir(d);
            TimerEnd(timer, "misc_x11:ProbeSessionUid");
            return st.st_uid;
        }
    }
    closedir(d);
    TimerEnd(timer, "misc_x11:ProbeSessionUid");
    return (uid_t)-1;
}

const char* Platform_GetRealUser(void){
    WinTimer timer = TimerStart();
    static char buf[256]={0};
    static bool done=false;
    if(done) { TimerEnd(timer, "misc_x11:Platform_GetRealUser"); return buf[0]?buf:"diggy"; }
    const char* u=getenv("SUDO_USER");
    if(u && u[0] && strcmp(u,"root")!=0){ strncpy(buf,u,sizeof(buf)-1); done=true; TimerEnd(timer, "misc_x11:Platform_GetRealUser"); return buf; }
    u=getenv("USER");
    if(u && u[0] && strcmp(u,"root")!=0){ strncpy(buf,u,sizeof(buf)-1); done=true; TimerEnd(timer, "misc_x11:Platform_GetRealUser"); return buf; }
    u=getenv("LOGNAME");
    if(u && u[0] && strcmp(u,"root")!=0){ strncpy(buf,u,sizeof(buf)-1); done=true; TimerEnd(timer, "misc_x11:Platform_GetRealUser"); return buf; }
    
    uid_t suid = ProbeSessionUid();
    if (suid != (uid_t)-1) {
        struct passwd* spw = getpwuid(suid);
        if (spw && spw->pw_name) { strncpy(buf,spw->pw_name,sizeof(buf)-1); done=true; TimerEnd(timer, "misc_x11:Platform_GetRealUser"); return buf; }
    }
    struct passwd* pw=getpwuid(getuid());
    if(pw && pw->pw_name && strcmp(pw->pw_name,"root")!=0){ strncpy(buf,pw->pw_name,sizeof(buf)-1); done=true; TimerEnd(timer, "misc_x11:Platform_GetRealUser"); return buf; }
    strncpy(buf,"diggy",sizeof(buf)-1); done=true; TimerEnd(timer, "misc_x11:Platform_GetRealUser"); return buf;
}

const char* Platform_GetRealHome(void){
    WinTimer timer = TimerStart();
    static char buf[512]={0};
    static bool done=false;
    if(done) { TimerEnd(timer, "misc_x11:Platform_GetRealHome"); return buf[0]?buf:"/home/diggy"; }
    const char* u=Platform_GetRealUser();
    struct passwd* pw=getpwnam(u);
    if(pw && pw->pw_dir){ strncpy(buf,pw->pw_dir,sizeof(buf)-1); done=true; TimerEnd(timer, "misc_x11:Platform_GetRealHome"); return buf; }
    pw=getpwuid(getuid());
    if(pw && pw->pw_dir){ strncpy(buf,pw->pw_dir,sizeof(buf)-1); done=true; TimerEnd(timer, "misc_x11:Platform_GetRealHome"); return buf; }
    const char* h=getenv("HOME");
    if(h && h[0]){ strncpy(buf,h,sizeof(buf)-1); done=true; TimerEnd(timer, "misc_x11:Platform_GetRealHome"); return buf; }
    snprintf(buf,sizeof(buf),"/home/%s",u); done=true; TimerEnd(timer, "misc_x11:Platform_GetRealHome"); return buf;
}

bool Platform_IsRunningAsRoot(void){
    WinTimer timer = TimerStart();
    TimerEnd(timer, "misc_x11:Platform_IsRunningAsRoot"); return getuid()==0;
}



const char* Platform_GetSessionDisplay(void){
    WinTimer timer = TimerStart();
    static char buf[64]={0};
    static bool done=false;
    if(done) { TimerEnd(timer, "misc_x11:Platform_GetSessionDisplay"); return buf[0]?buf:":0"; }
    const char* env=getenv("DISPLAY");
    if(env && env[0]){ snprintf(buf,sizeof(buf),"%s",env); done=true; TimerEnd(timer, "misc_x11:Platform_GetSessionDisplay"); return buf; }
    DIR* d = opendir("/tmp/.X11-unix");
    if (d) {
        struct dirent* e;
        while ((e = readdir(d)) != NULL) {
            if (e->d_name[0] != 'X') continue;
            char* end = NULL;
            long n = strtol(e->d_name + 1, &end, 10);
            if (end == e->d_name + 1 || *end != '\0') continue;
            char path[512];
            snprintf(path, sizeof(path), "/tmp/.X11-unix/%s", e->d_name);
            struct stat st;
            if (stat(path, &st) != 0) continue;
            
            if (st.st_uid == geteuid() || st.st_uid != 0) {
                snprintf(buf, sizeof(buf), ":%ld", n);
                break;
            }
        }
        closedir(d);
    }
    done=true;
    TimerEnd(timer, "misc_x11:Platform_GetSessionDisplay");
    return buf[0]?buf:":0";
}



const char* Platform_GetSessionXAuthority(void){
    WinTimer timer = TimerStart();
    static char buf[512]={0};
    static bool done=false;
    if(done) { TimerEnd(timer, "misc_x11:Platform_GetSessionXAuthority"); return buf[0]?buf:""; }
    const char* xa=getenv("XAUTHORITY");
    if(xa && xa[0]){ snprintf(buf,sizeof(buf),"%s",xa); done=true; TimerEnd(timer, "misc_x11:Platform_GetSessionXAuthority"); return buf; }
    snprintf(buf,sizeof(buf),"%s/.Xauthority",Platform_GetRealHome());
    if(access(buf,R_OK)==0){ done=true; TimerEnd(timer, "misc_x11:Platform_GetSessionXAuthority"); return buf; }
    buf[0]='\0'; done=true;
    TimerEnd(timer, "misc_x11:Platform_GetSessionXAuthority");
    return "";
}


const char* Platform_GetRuntimeDir(void){
    WinTimer timer = TimerStart();
    static char buf[256]={0};
    static bool done=false;
    if(done) { TimerEnd(timer, "misc_x11:Platform_GetRuntimeDir"); return buf[0]?buf:"/run/user/1000"; }
    const char* rt=getenv("XDG_RUNTIME_DIR");
    if(rt && rt[0]){ snprintf(buf,sizeof(buf),"%s",rt); done=true; TimerEnd(timer, "misc_x11:Platform_GetRuntimeDir"); return buf; }
    const char* u=Platform_GetRealUser();
    struct passwd* pw=getpwnam(u);
    if(pw){ snprintf(buf,sizeof(buf),"/run/user/%d",(int)pw->pw_uid); done=true; TimerEnd(timer, "misc_x11:Platform_GetRuntimeDir"); return buf; }
    snprintf(buf,sizeof(buf),"/run/user/%d",(int)getuid()); done=true;
    TimerEnd(timer, "misc_x11:Platform_GetRuntimeDir");
    return buf[0]?buf:"/run/user/1000";
}

static bool ResolvePathToken(const char* token, char* out, size_t out_size) {
    WinTimer timer = TimerStart();
    for (unsigned int i = 0; i < PATH_VAR_COUNT; i++) {
        if (strcmp(G_PATH_VARS[i].name, token) == 0) {
            const char* value =
#if defined(_WIN32)
                G_PATH_VARS[i].win32_value;
#else
                G_PATH_VARS[i].linux_value;
#endif
            if (value == NULL || value[0] == '\0') { TimerEnd(timer, "misc_x11:ResolvePathToken"); return false; }
            snprintf(out, out_size, "%s", value);
            TimerEnd(timer, "misc_x11:ResolvePathToken");
            return true;
        }
    }

    const char* home = getenv("HOME");

    if (strcmp(token, "HOME") == 0) {
        if (!home) { TimerEnd(timer, "misc_x11:ResolvePathToken"); return false; }
        snprintf(out, out_size, "%s", home);
        TimerEnd(timer, "misc_x11:ResolvePathToken");
        return true;
    }
    if (strcmp(token, "APPDATA") == 0) {
        const char* xdg = getenv("XDG_CONFIG_HOME");
        if (xdg && xdg[0] != '\0') {
            snprintf(out, out_size, "%s", xdg);
            TimerEnd(timer, "misc_x11:ResolvePathToken");
            return true;
        }
        if (!home) { TimerEnd(timer, "misc_x11:ResolvePathToken"); return false; }
        snprintf(out, out_size, "%s/.config", home);
        TimerEnd(timer, "misc_x11:ResolvePathToken");
        return true;
    }
    if (strcmp(token, "PROGRAM_FILES") == 0) {
        snprintf(out, out_size, "/usr");
        TimerEnd(timer, "misc_x11:ResolvePathToken");
        return true;
    }

    static const struct { const char* name; const char* subdir; } subdirs[] = {
        {"PICTURES", "Pictures"},
        {"DESKTOP", "Desktop"},
        {"DOCUMENTS", "Documents"},
        {"DOWNLOADS", "Downloads"},
    };
    for (size_t i = 0; i < ARRAY_LEN(subdirs); i++) {
        if (strcmp(token, subdirs[i].name) == 0) {
            if (!home) { TimerEnd(timer, "misc_x11:ResolvePathToken"); return false; }
            snprintf(out, out_size, "%s/%s", home, subdirs[i].subdir);
            TimerEnd(timer, "misc_x11:ResolvePathToken");
            return true;
        }
    }
    TimerEnd(timer, "misc_x11:ResolvePathToken");
    return false;
}

bool Platform_ExpandPath(const char* template_path, char* out, size_t out_size) {
    WinTimer timer = TimerStart();
    if (template_path == NULL || out_size == 0) { TimerEnd(timer, "misc_x11:Platform_ExpandPath"); return false; }

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
                    TimerEnd(timer, "misc_x11:Platform_ExpandPath");
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
    TimerEnd(timer, "misc_x11:Platform_ExpandPath");
    return true;
}

int ConvertLinuxKeyToWindowsVK(int key_code) {
    WinTimer timer = TimerStart();
    if (key_code >= KEY_1 && key_code <= KEY_0) {
        TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK");
        return '1' + (key_code - KEY_1);
    }

    switch (key_code) {
        case KEY_A: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'A';
        case KEY_B: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'B';
        case KEY_C: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'C';
        case KEY_D: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'D';
        case KEY_E: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'E';
        case KEY_F: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'F';
        case KEY_G: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'G';
        case KEY_H: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'H';
        case KEY_I: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'I';
        case KEY_J: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'J';
        case KEY_K: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'K';
        case KEY_L: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'L';
        case KEY_M: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'M';
        case KEY_N: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'N';
        case KEY_O: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'O';
        case KEY_P: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'P';
        case KEY_Q: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'Q';
        case KEY_R: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'R';
        case KEY_S: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'S';
        case KEY_T: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'T';
        case KEY_U: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'U';
        case KEY_V: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'V';
        case KEY_W: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'W';
        case KEY_X: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'X';
        case KEY_Y: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'Y';
        case KEY_Z: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'Z';
        case KEY_SPACE: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_SPACE;
        case KEY_ENTER: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_RETURN;
        case KEY_BACKSPACE: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_BACK;
        case KEY_TAB: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_TAB;
        case KEY_ESC: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_ESCAPE;
        case KEY_LEFT: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_LEFT;
        case KEY_UP: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_UP;
        case KEY_RIGHT: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_RIGHT;
        case KEY_DOWN: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_DOWN;
        case KEY_LEFTCTRL: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_LCONTROL;
        case KEY_RIGHTCTRL: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_RCONTROL;
        case KEY_LEFTALT: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_LMENU;
        case KEY_RIGHTALT: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_RMENU;
        case KEY_LEFTSHIFT: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_LSHIFT;
        case KEY_RIGHTSHIFT: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_RSHIFT;
        case KEY_LEFTMETA: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_LWIN;
        case KEY_RIGHTMETA: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_RWIN;
        case KEY_SEMICOLON: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_OEM_1;
        case KEY_SLASH: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_OEM_2;
        case KEY_LEFTBRACE: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_OEM_4;
        case KEY_COMMA: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_OEM_COMMA;
        case KEY_DOT: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_OEM_PERIOD;
        case KEY_APOSTROPHE: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_OEM_7;
        default: TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 0;
    }
    TimerEnd(timer, "misc_x11:ConvertLinuxKeyToWindowsVK");
}

char Platform_GetLastChar(MappedFile *file) {
    WinTimer timer = TimerStart();
    char lastChar = '\0';

    lseek(file->fd, -1, SEEK_END);

    read(file->fd, &lastChar, 1);

    TimerEnd(timer, "misc_x11:Platform_GetLastChar");
    return lastChar;
}

int Platform_FileAppend(MappedFile *mf, const char *text) {
    WinTimer timer = TimerStart();
    if (mf == NULL || text == NULL) {
        TimerEnd(timer, "misc_x11:Platform_FileAppend");
        return 0;
    }

    if (lseek(mf->fd, 0, SEEK_END) == -1) {
        perror("Platform_FileAppend seek failed");
        TimerEnd(timer, "misc_x11:Platform_FileAppend");
        return 0;
    }

    size_t textLen = strlen(text);
    ssize_t bytesWritten = write(mf->fd, text, textLen);
    if (bytesWritten == -1 || (size_t)bytesWritten != textLen) {
        perror("Platform_FileAppend write failed");
        TimerEnd(timer, "misc_x11:Platform_FileAppend");
        return 0;
    }

    struct stat sb;
    if (fstat(mf->fd, &sb) == -1) {
        perror("Platform_FileAppend fstat failed");
        TimerEnd(timer, "misc_x11:Platform_FileAppend");
        return 0;
    }
    mf->fileSize = sb.st_size;

    TimerEnd(timer, "misc_x11:Platform_FileAppend");
    return 1;
}

void Platform_FileClose(MappedFile *mf) {
    WinTimer timer = TimerStart();
    if (mf == NULL) {
        TimerEnd(timer, "misc_x11:Platform_FileClose");
        return;
    }

    if (mf->fd >= 0) {
        if (close(mf->fd) == -1) {
            perror("Platform_FileClose failed");
        }
        mf->fd = -1;
    }

    mf->fileSize = 0;
    TimerEnd(timer, "misc_x11:Platform_FileClose");
}

Line* Platform_FileGetLines(MappedFile *mf, size_t *outLineCount) {
    WinTimer timer = TimerStart();
    if (!mf || mf->fileSize == 0) {
        *outLineCount = 0;
        TimerEnd(timer, "misc_x11:Platform_FileGetLines");
        return NULL;
    }

    char *pBuffer = (char *)mmap(NULL, mf->fileSize, PROT_READ | PROT_WRITE, MAP_PRIVATE, mf->fd, 0);
    if (pBuffer == MAP_FAILED) {
        perror("mmap failed in FileGetLines");
        TimerEnd(timer, "misc_x11:Platform_FileGetLines");
        return NULL;
    }

    size_t maxLines = 1000;
    size_t count = 0;
    Line *list = (Line *)malloc(maxLines * sizeof(Line));
    if (list == NULL) {
        munmap(pBuffer, mf->fileSize);
        TimerEnd(timer, "misc_x11:Platform_FileGetLines");
        return NULL;
    }

    char *lineStart = pBuffer;
    size_t totalBytes = mf->fileSize;

    for (size_t i = 0; i < totalBytes; i++) {
        if (pBuffer[i] == '\n' || i == totalBytes - 1) {
            if (count >= maxLines) {
                size_t newMax = maxLines * 2;
                Line *tmp = (Line *)realloc(list, newMax * sizeof(Line));
                if (!tmp) {
                    for (size_t k = 0; k < count; k++) free(list[k].ptr);
                    free(list);
                    munmap(pBuffer, mf->fileSize);
                    *outLineCount = 0;
                    TimerEnd(timer, "misc_x11:Platform_FileGetLines");
                    return NULL;
                }
                list = tmp;
                maxLines = newMax;
            }
            size_t currentLineLen = &pBuffer[i] - lineStart;
            if (i == totalBytes - 1) currentLineLen++;

            char *copy = (char *)malloc(currentLineLen + 1);
            if (!copy) {
                for (size_t k = 0; k < count; k++) free(list[k].ptr);
                free(list);
                munmap(pBuffer, mf->fileSize);
                *outLineCount = 0;
                TimerEnd(timer, "misc_x11:Platform_FileGetLines");
                return NULL;
            }
            memcpy(copy, lineStart, currentLineLen);
            copy[currentLineLen] = '\0';

            list[count].ptr = copy;
            list[count].length = currentLineLen;
            count++;
            lineStart = &pBuffer[i + 1];
        }
    }

    munmap(pBuffer, mf->fileSize);

    *outLineCount = count;
    TimerEnd(timer, "misc_x11:Platform_FileGetLines");
    return list;
}

char** Platform_FileGetStrings(MappedFile *mf, size_t *outLineCount, NewlineInfo **outNewlines, size_t *outNewlineCount) {
    WinTimer timer = TimerStart();
    if (!mf || mf->fileSize == 0) {
        *outLineCount = 0;
        *outNewlines = NULL;
        *outNewlineCount = 0;
        TimerEnd(timer, "misc_x11:Platform_FileGetStrings");
        return NULL;
    }

    size_t maxLines = 1000;
    size_t count = 0;
    char **stringList = (char **)malloc(maxLines * sizeof(char*));
    if (stringList == NULL) { TimerEnd(timer, "misc_x11:Platform_FileGetStrings"); return NULL; }

    size_t maxNewlines = 2000;
    size_t newlineCount = 0;
    NewlineInfo *newlines = (NewlineInfo *)malloc(maxNewlines * sizeof(NewlineInfo));
    if (newlines == NULL) {
        free(stringList);
        TimerEnd(timer, "misc_x11:Platform_FileGetStrings");
        return NULL;
    }

    char *pBuffer = (char *)mmap(NULL, mf->fileSize, PROT_READ | PROT_WRITE, MAP_PRIVATE, mf->fd, 0);
    if (pBuffer == MAP_FAILED) {
        perror("mmap failed in FileGetStrings");
        free(stringList);
        free(newlines);
        TimerEnd(timer, "misc_x11:Platform_FileGetStrings");
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
                size_t newMax = maxNewlines * 2;
                NewlineInfo *tmp = (NewlineInfo *)realloc(newlines, newMax * sizeof(NewlineInfo));
                if (!tmp) {
                    munmap(pBuffer, mf->fileSize);
                    mf->pBuffer = NULL;
                    free(stringList);
                    free(newlines);
                    *outLineCount = 0;
                    *outNewlines = NULL;
                    *outNewlineCount = 0;
                    TimerEnd(timer, "misc_x11:Platform_FileGetStrings");
                    return NULL;
                }
                newlines = tmp;
                maxNewlines = newMax;
            }
            newlines[newlineCount++] = (NewlineInfo){ .index = i, .original = orig };
        }
        else if (pBuffer[i] == '\n') {
            char orig = pBuffer[i];
            pBuffer[i] = '\0';

            if (newlineCount >= maxNewlines) {
                size_t newMax = maxNewlines * 2;
                NewlineInfo *tmp = (NewlineInfo *)realloc(newlines, newMax * sizeof(NewlineInfo));
                if (!tmp) {
                    munmap(pBuffer, mf->fileSize);
                    mf->pBuffer = NULL;
                    free(stringList);
                    free(newlines);
                    *outLineCount = 0;
                    *outNewlines = NULL;
                    *outNewlineCount = 0;
                    TimerEnd(timer, "misc_x11:Platform_FileGetStrings");
                    return NULL;
                }
                newlines = tmp;
                maxNewlines = newMax;
            }
            newlines[newlineCount++] = (NewlineInfo){ .index = i, .original = orig };

            if (i + 1 < totalBytes) {
                if (count >= maxLines) {
                    size_t newMaxL = maxLines * 2;
                    char **tmp = (char **)realloc(stringList, newMaxL * sizeof(char*));
                    if (!tmp) {
                        munmap(pBuffer, mf->fileSize);
                        mf->pBuffer = NULL;
                        free(stringList);
                        free(newlines);
                        *outLineCount = 0;
                        *outNewlines = NULL;
                        *outNewlineCount = 0;
                        TimerEnd(timer, "misc_x11:Platform_FileGetStrings");
                        return NULL;
                    }
                    stringList = tmp;
                    maxLines = newMaxL;
                }
                stringList[count++] = &pBuffer[i + 1];
            }
        }
    }

    *outLineCount = count;
    *outNewlines = newlines;
    *outNewlineCount = newlineCount;
    TimerEnd(timer, "misc_x11:Platform_FileGetStrings");
    return stringList;
}

void Platform_CleanUpFile(MappedFile *mf, NewlineInfo *newlines, size_t newlineCount) {
    WinTimer timer = TimerStart();
    (void)newlineCount;
    if (!mf || !mf->pBuffer) { TimerEnd(timer, "misc_x11:Platform_CleanUpFile"); return; }

    munmap(mf->pBuffer, mf->fileSize);
    mf->pBuffer = NULL;

    if (newlines) {
        free(newlines);
    }
    TimerEnd(timer, "misc_x11:Platform_CleanUpFile");
}

bool Platform_StringContainsCaseInsensitive(const char* haystack, const char* needle) {
    WinTimer timer = TimerStart();
    if (!haystack || !needle) { TimerEnd(timer, "misc_x11:Platform_StringContainsCaseInsensitive"); return false; }
    TimerEnd(timer, "misc_x11:Platform_StringContainsCaseInsensitive");
    return strcasestr(haystack, needle) != NULL;
}

bool Platform_StringStartsWithCaseInsensitive(const char* str, const char* prefix) {
    WinTimer timer = TimerStart();
    if (!str || !prefix) { TimerEnd(timer, "misc_x11:Platform_StringStartsWithCaseInsensitive"); return false; }
    size_t plen = strlen(prefix);
    if (plen == 0) { TimerEnd(timer, "misc_x11:Platform_StringStartsWithCaseInsensitive"); return true; }
    TimerEnd(timer, "misc_x11:Platform_StringStartsWithCaseInsensitive");
    return strncasecmp(str, prefix, plen) == 0;
}

int Platform_ReplaceLineInMappedFile(MappedFile* mf, int line_index, const char* new_text) {
    WinTimer timer = TimerStart();
    if (!mf || !new_text || mf->fd < 0) { TimerEnd(timer, "misc_x11:Platform_ReplaceLineInMappedFile"); return 0; }

    char* old_data = (char*)malloc(mf->fileSize + 1);
    if (!old_data) { TimerEnd(timer, "misc_x11:Platform_ReplaceLineInMappedFile"); return 0; }

    lseek(mf->fd, 0, SEEK_SET);
    if (read(mf->fd, old_data, mf->fileSize) != (ssize_t)mf->fileSize) {
        free(old_data);
        TimerEnd(timer, "misc_x11:Platform_ReplaceLineInMappedFile");
        return 0;
    }
    old_data[mf->fileSize] = '\0';

    int scratch_fd = memfd_create("replace_scratch", MFD_CLOEXEC);
    if (scratch_fd == -1) {
        free(old_data);
        TimerEnd(timer, "misc_x11:Platform_ReplaceLineInMappedFile");
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
        TimerEnd(timer, "misc_x11:Platform_ReplaceLineInMappedFile");
        return 0;
    }
    size_t new_size = sb.st_size;

    char* transfer_buf = (char*)malloc(new_size);
    if (!transfer_buf) {
        close(scratch_fd);
        TimerEnd(timer, "misc_x11:Platform_ReplaceLineInMappedFile");
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
    TimerEnd(timer, "misc_x11:Platform_ReplaceLineInMappedFile");
    return 1;
}

int Platform_CollectRunningProcesses(char process_list[][128], int max_count) {
    WinTimer timer = TimerStart();
    if (!process_list || max_count <= 0) { TimerEnd(timer, "misc_x11:Platform_CollectRunningProcesses"); return 0; }
    int count = 0;
    DIR* proc = opendir("/proc");
    if (!proc) { TimerEnd(timer, "misc_x11:Platform_CollectRunningProcesses"); return 0; }
    struct dirent* entry;
    while ((entry = readdir(proc)) != NULL && count < max_count) {
        if (!isdigit((unsigned char)entry->d_name[0])) continue;
        char comm_path[512];
        snprintf(comm_path, sizeof(comm_path), "/proc/%s/comm", entry->d_name);
        FILE* f = fopen(comm_path, "r");
        if (!f) continue;
        char exe[256];
        if (!fgets(exe, sizeof(exe), f)) { fclose(f); continue; }
        fclose(f);
        size_t len = strlen(exe);
        while (len>0 && (exe[len-1]=='\n' || exe[len-1]=='\r')) exe[--len]='\0';
        if (len==0) continue;
        char stat_path[512];
        snprintf(stat_path, sizeof(stat_path), "/proc/%s/stat", entry->d_name);
        FILE* fstat = fopen(stat_path, "r");
        if (fstat) {
            char stat_buf[1024];
            if (fgets(stat_buf, sizeof(stat_buf), fstat)) {
                char* rparen = strrchr(stat_buf, ')');
                if (rparen) {
                    int ppid = 0;
                    if (sscanf(rparen+2, "%*c %d", &ppid)==1 && ppid==2) {
                        fclose(fstat);
                        continue;
                    }
                }
            }
            fclose(fstat);
        }
        bool exists=false;
        for (int i=0;i<count;i++) if (strcasecmp(process_list[i], exe)==0) {exists=true;break;}
        if (!exists) {
            strncpy(process_list[count], exe, 127);
            process_list[count][127]='\0';
            count++;
        }
    }
    closedir(proc);
    TimerEnd(timer, "misc_x11:Platform_CollectRunningProcesses");
    return count;
}

bool Platform_TerminateProcessByName(const char* exe_name) {
    WinTimer timer = TimerStart();
    if (!exe_name || !*exe_name) { TimerEnd(timer, "misc_x11:Platform_TerminateProcessByName"); return false; }
    char tmp[256];
    strncpy(tmp, exe_name, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1]='\0';
    char *ext = strrchr(tmp, '.');
    if (ext && strcmp(ext, ".exe") == 0) *ext = '\0';
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "pkill \"%s\"", tmp);
    Platform_RunTerminalCommand(cmd, false);
    snprintf(cmd, sizeof(cmd), "killall -9 \"%s\" 2>/dev/null; killall -9 -r \"%s\" 2>/dev/null", tmp, tmp);
    Platform_RunTerminalCommand(cmd, false);
    TimerEnd(timer, "misc_x11:Platform_TerminateProcessByName");
    return true;
}

bool Platform_LaunchBrowserAtUrl(char* url,bool new_window) {
    WinTimer timer = TimerStart();
    char command[2048];
    const char* real_user = Platform_GetRealUser();
    const char* display = getenv("DISPLAY");
    if (!display) display = ":0";
    const char* xauthority = getenv("XAUTHORITY");
    if (!xauthority) xauthority = "";
    const char* env_runtime = getenv("XDG_RUNTIME_DIR");
    char runtime_dir[256];
    if (env_runtime) snprintf(runtime_dir, sizeof(runtime_dir), "%s", env_runtime);
    else snprintf(runtime_dir, sizeof(runtime_dir), "/run/user/1000");
    if (new_window) {
      if (Platform_IsRunningAsRoot()) {
          snprintf(command, sizeof(command), 
                   "sudo -u %s DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s %s --new-window \"%s\" > /dev/null 2>&1 &",
                   real_user, display, xauthority, runtime_dir, UsedBrowser, url);
      } else {
          snprintf(command, sizeof(command), 
                   "DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s %s --new-window \"%s\" > /dev/null 2>&1 &",
                   display, xauthority, runtime_dir, UsedBrowser, url);
      }
      
      TimerEnd(timer, "misc_x11:Platform_LaunchBrowserAtUrl");
      return system(command) == 0;
    } else {
    if (Platform_IsRunningAsRoot()) {
        snprintf(command, sizeof(command), "sudo -u %s DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s xdg-open \"%s\" > /dev/null 2>&1 &", real_user, display, xauthority, runtime_dir, url);
    } else {
        snprintf(command, sizeof(command), "DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s xdg-open \"%s\" > /dev/null 2>&1 &", display, xauthority, runtime_dir, url);
    }
    TimerEnd(timer, "misc_x11:Platform_LaunchBrowserAtUrl");
    return system(command);
    }
    TimerEnd(timer, "misc_x11:Platform_LaunchBrowserAtUrl");
}
