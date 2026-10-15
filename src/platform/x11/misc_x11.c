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
#include <ctype.h>
#include <time.h>
#include <linux/input.h>

#include "config.h"
#include "config.h"
#include "platform/platform.h"
#include "helpers/helpers_text.h"
#include "platform/platform.h"
#include "core/timer.h"
#include "deps/log.h"
#include "wm/wm.h"
#include "platform/x11/clipboard_x11.h"
#include "platform/x11/clipboard_backend_x11.h"


static uid_t ProbeSessionUid(void){
        PROFILE_START(_pt);
PROFILE_START(timer);
    DIR* d = opendir("/tmp/.X11-unix");
    if (!d) { PROFILE_END_VAR(timer, "misc_x11:ProbeSessionUid"); return (uid_t)-1; }
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] != 'X') continue;
        char path[512];
        snprintf(path, sizeof(path), "/tmp/.X11-unix/%s", e->d_name);
        struct stat st;
        if (stat(path, &st) == 0 && st.st_uid != 0) {
            closedir(d);
            PROFILE_END_VAR(timer, "misc_x11:ProbeSessionUid");
            return st.st_uid;
        }
    }
    closedir(d);
    PROFILE_END_VAR(timer, "misc_x11:ProbeSessionUid");
    { PROFILE_END_VAR(_pt, "ProbeSessionUid"); return (uid_t)-1; }
}

const char* Platform_GetRealUser(void){
        PROFILE_START(_pt);
PROFILE_START(timer);
    static char buf[256]={0};
    const char* u=getenv("SUDO_USER");
    if(u && u[0] && strcmp(u,"root")!=0){ strncpy(buf,u,sizeof(buf)-1); PROFILE_END_VAR(timer, "misc_x11:Platform_GetRealUser"); return buf; }
    u=getenv("USER");
    if(u && u[0] && strcmp(u,"root")!=0){ strncpy(buf,u,sizeof(buf)-1); PROFILE_END_VAR(timer, "misc_x11:Platform_GetRealUser"); return buf; }
    u=getenv("LOGNAME");
    if(u && u[0] && strcmp(u,"root")!=0){ strncpy(buf,u,sizeof(buf)-1); PROFILE_END_VAR(timer, "misc_x11:Platform_GetRealUser"); return buf; }
    
    uid_t suid = ProbeSessionUid();
    if (suid != (uid_t)-1) {
        struct passwd* spw = getpwuid(suid);
        if (spw && spw->pw_name) { strncpy(buf,spw->pw_name,sizeof(buf)-1);  PROFILE_END_VAR(timer, "misc_x11:Platform_GetRealUser"); return buf; }
    }
    struct passwd* pw=getpwuid(getuid());
    if(pw && pw->pw_name && strcmp(pw->pw_name,"root")!=0){ strncpy(buf,pw->pw_name,sizeof(buf)-1);  log_info("Platform_GetRealUser completed");
PROFILE_END_VAR(timer, "misc_x11:Platform_GetRealUser"); return buf; }
    { PROFILE_END_VAR(_pt, "Platform_GetRealUser"); return NULL; }
}

const char* Platform_GetRealHome(void){
        PROFILE_START(_pt);
PROFILE_START(timer);
    static char buf[512]={0};
    const char* u=Platform_GetRealUser();
    struct passwd* pw=getpwnam(u);
    if(pw && pw->pw_dir){ strncpy(buf,pw->pw_dir,sizeof(buf)-1); PROFILE_END_VAR(timer, "misc_x11:Platform_GetRealHome"); return buf; }
    pw=getpwuid(getuid());
    if(pw && pw->pw_dir){ strncpy(buf,pw->pw_dir,sizeof(buf)-1); PROFILE_END_VAR(timer, "misc_x11:Platform_GetRealHome"); return buf; }
    const char* h=getenv("HOME");
    if(h && h[0]){ strncpy(buf,h,sizeof(buf)-1); PROFILE_END_VAR(timer, "misc_x11:Platform_GetRealHome"); return buf; }
    snprintf(buf,sizeof(buf),"/home/%s",u); log_info("Platform_GetRealHome completed");
PROFILE_END_VAR(timer, "misc_x11:Platform_GetRealHome"); { PROFILE_END_VAR(_pt, "Platform_GetRealHome"); return buf; }
}

bool Platform_IsRunningAsRoot(void){
        PROFILE_START(_pt);
PROFILE_START(timer);
        log_info("Platform_IsRunningAsRoot completed");
PROFILE_END_VAR(timer, "misc_x11:Platform_IsRunningAsRoot"); { PROFILE_END_VAR(_pt, "Platform_IsRunningAsRoot"); return getuid()==0; }
}



const char* Platform_GetSessionDisplay(void){
        PROFILE_START(_pt);
PROFILE_START(timer);
    static char buf[64]={0};
    const char* env=getenv("DISPLAY");
    if(env && env[0]){ snprintf(buf,sizeof(buf),"%s",env);PROFILE_END_VAR(timer, "misc_x11:Platform_GetSessionDisplay"); return buf; }
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
        log_info("Platform_GetSessionDisplay completed");
PROFILE_END_VAR(timer, "misc_x11:Platform_GetSessionDisplay");
    { PROFILE_END_VAR(_pt, "Platform_GetSessionDisplay"); return buf[0]?buf:":0"; }
}



const char* Platform_GetSessionXAuthority(void){
        PROFILE_START(_pt);
PROFILE_START(timer);
    static char buf[512]={0};
    struct stat xst;
    const char* xa=getenv("XAUTHORITY");
    if(xa && xa[0] && stat(xa,&xst)==0 && xst.st_size>0 && access(xa,R_OK)==0){ snprintf(buf,sizeof(buf),"%s",xa);PROFILE_END_VAR(timer, "misc_x11:Platform_GetSessionXAuthority"); return buf; }
    char rtd[256];
    const char* rt=getenv("XDG_RUNTIME_DIR");
    if(rt && rt[0]) snprintf(rtd,sizeof(rtd),"%s",rt);
    else snprintf(rtd,sizeof(rtd),"/run/user/%d",(int)getuid());
    DIR* d = opendir(rtd);
    if (d) {
        struct dirent* e;
        time_t best_mtime = 0;
        char best[512]={0};
        while ((e = readdir(d)) != NULL) {
            if (strncmp(e->d_name, "xauth_", 6) != 0) continue;
            char path[512];
            snprintf(path, sizeof(path), "%s/%s", rtd, e->d_name);
            struct stat st;
            if (stat(path, &st) != 0) continue;
            if (st.st_size == 0 || access(path, R_OK) != 0) continue;
            if (st.st_mtime >= best_mtime) { best_mtime = st.st_mtime; snprintf(best, sizeof(best), "%s", path); }
        }
        closedir(d);
        if (best[0]) { snprintf(buf,sizeof(buf),"%s",best);PROFILE_END_VAR(timer, "misc_x11:Platform_GetSessionXAuthority"); return buf; }
    }
    snprintf(buf,sizeof(buf),"%s/.Xauthority",Platform_GetRealHome());
    if(stat(buf,&xst)==0 && xst.st_size>0 && access(buf,R_OK)==0){ PROFILE_END_VAR(timer, "misc_x11:Platform_GetSessionXAuthority"); return buf; }
    buf[0]='\0';
        log_info("Platform_GetSessionXAuthority completed");
PROFILE_END_VAR(timer, "misc_x11:Platform_GetSessionXAuthority");
    { PROFILE_END_VAR(_pt, "Platform_GetSessionXAuthority"); return ""; }
}


const char* Platform_GetRuntimeDir(void){
        PROFILE_START(_pt);
PROFILE_START(timer);
    static char buf[256]={0};
    const char* rt=getenv("XDG_RUNTIME_DIR");
    if(rt && rt[0]){ snprintf(buf,sizeof(buf),"%s",rt); PROFILE_END_VAR(timer, "misc_x11:Platform_GetRuntimeDir"); return buf; }
    const char* u=Platform_GetRealUser();
    struct passwd* pw=getpwnam(u);
    if(pw){ snprintf(buf,sizeof(buf),"/run/user/%d",(int)pw->pw_uid); PROFILE_END_VAR(timer, "misc_x11:Platform_GetRuntimeDir"); return buf; }
    snprintf(buf,sizeof(buf),"/run/user/%d",(int)getuid());
        log_info("Platform_GetRuntimeDir completed");
PROFILE_END_VAR(timer, "misc_x11:Platform_GetRuntimeDir");
    { PROFILE_END_VAR(_pt, "Platform_GetRuntimeDir"); return buf[0]?buf:"/run/user/1000"; }
}

bool Platform_ExpandPath(const char* template_path, char* out, size_t out_size) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    if (template_path == NULL || out_size == 0) { PROFILE_END_VAR(timer, "misc_x11:Platform_ExpandPath"); return false; }

    if (template_path[0] == '~' && (template_path[1] == '/' || template_path[1] == '\0')) {
        const char* home = Platform_GetRealHome();
        if (home && home[0]) {
            size_t hn = strlen(home);
            size_t total = 0;
            if (hn < out_size) { memcpy(out, home, hn); total = hn; }
            const char* rest = template_path + 1;
            while (*rest && total < out_size - 1) out[total++] = *rest++;
            out[total] = '\0';
            if (total >= 4) {
                size_t i = total - 4;
                if (out[i] == '.' &&
                    (out[i+1] == 'e' || out[i+1] == 'E') &&
                    (out[i+2] == 'x' || out[i+2] == 'X') &&
                    (out[i+3] == 'e' || out[i+3] == 'E')) {
                    out[i] = '\0';
                }
            }
            PROFILE_END_VAR(timer, "misc_x11:Platform_ExpandPath");
            return true;
        }
    }

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
                    PROFILE_END_VAR(timer, "misc_x11:Platform_ExpandPath");
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
    PROFILE_END_VAR(timer, "misc_x11:Platform_ExpandPath");
    { PROFILE_END_VAR(_pt, "Platform_ExpandPath"); return true; }
}

int ConvertLinuxKeyToWindowsVK(int key_code) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    if (key_code >= KEY_1 && key_code <= KEY_0) {
        PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK");
        return '1' + (key_code - KEY_1);
    }

    switch (key_code) {
        case KEY_A: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'A';
        case KEY_B: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'B';
        case KEY_C: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'C';
        case KEY_D: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'D';
        case KEY_E: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'E';
        case KEY_F: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'F';
        case KEY_G: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'G';
        case KEY_H: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'H';
        case KEY_I: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'I';
        case KEY_J: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'J';
        case KEY_K: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'K';
        case KEY_L: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'L';
        case KEY_M: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'M';
        case KEY_N: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'N';
        case KEY_O: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'O';
        case KEY_P: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'P';
        case KEY_Q: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'Q';
        case KEY_R: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'R';
        case KEY_S: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'S';
        case KEY_T: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'T';
        case KEY_U: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'U';
        case KEY_V: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'V';
        case KEY_W: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'W';
        case KEY_X: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'X';
        case KEY_Y: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'Y';
        case KEY_Z: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 'Z';
        case KEY_SPACE: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_SPACE;
        case KEY_ENTER: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_RETURN;
        case KEY_BACKSPACE: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_BACK;
        case KEY_TAB: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_TAB;
        case KEY_ESC: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_ESCAPE;
        case KEY_LEFT: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_LEFT;
        case KEY_UP: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_UP;
        case KEY_RIGHT: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_RIGHT;
        case KEY_DOWN: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_DOWN;
        case KEY_LEFTCTRL: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_LCONTROL;
        case KEY_RIGHTCTRL: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_RCONTROL;
        case KEY_LEFTALT: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_LMENU;
        case KEY_RIGHTALT: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_RMENU;
        case KEY_LEFTSHIFT: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_LSHIFT;
        case KEY_RIGHTSHIFT: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_RSHIFT;
        case KEY_LEFTMETA: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_LWIN;
        case KEY_RIGHTMETA: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_RWIN;
        case KEY_SEMICOLON: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_OEM_1;
        case KEY_SLASH: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_OEM_2;
        case KEY_LEFTBRACE: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_OEM_4;
        case KEY_RIGHTBRACE: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_OEM_6;
        case KEY_COMMA: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_OEM_COMMA;
        case KEY_DOT: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_OEM_PERIOD;
        case KEY_APOSTROPHE: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_OEM_7;
        case KEY_MINUS: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_OEM_MINUS;
        case KEY_EQUAL: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_OEM_PLUS;
        case KEY_GRAVE: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_OEM_3;
        case KEY_BACKSLASH: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return VK_OEM_5;
        default: PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK"); return 0;
    }
    PROFILE_END_VAR(timer, "misc_x11:ConvertLinuxKeyToWindowsVK");
}

int Platform_FileAppend(MappedFile *mf, const char *text) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    if (mf == NULL || text == NULL) {
        PROFILE_END_VAR(timer, "misc_x11:Platform_FileAppend");
        return 0;
    }

    if (lseek(mf->fd, 0, SEEK_END) == -1) {
        perror("Platform_FileAppend seek failed");
        PROFILE_END_VAR(timer, "misc_x11:Platform_FileAppend");
        return 0;
    }

    size_t textLen = strlen(text);
    ssize_t bytesWritten = write(mf->fd, text, textLen);
    if (bytesWritten == -1 || (size_t)bytesWritten != textLen) {
        perror("Platform_FileAppend write failed");
        PROFILE_END_VAR(timer, "misc_x11:Platform_FileAppend");
        return 0;
    }

    struct stat sb;
    if (fstat(mf->fd, &sb) == -1) {
        perror("Platform_FileAppend fstat failed");
        PROFILE_END_VAR(timer, "misc_x11:Platform_FileAppend");
        return 0;
    }
    mf->fileSize = sb.st_size;

        log_info("Platform_FileAppend mf=%p text=%p", mf, text);;
PROFILE_END_VAR(timer, "misc_x11:Platform_FileAppend");
    { PROFILE_END_VAR(_pt, "Platform_FileAppend"); return 1; }
}

void Platform_FileClose(MappedFile *mf) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    if (mf == NULL) {
        PROFILE_END_VAR(timer, "misc_x11:Platform_FileClose");
        return;
    }

    if (mf->fd >= 0) {
        if (close(mf->fd) == -1) {
            perror("Platform_FileClose failed");
        }
        mf->fd = -1;
    }

    mf->fileSize = 0;
        log_info("Platform_FileClose mf=%p", mf);;
PROFILE_END_VAR(timer, "misc_x11:Platform_FileClose");
}

Line* Platform_FileGetLines(MappedFile *mf, size_t *outLineCount) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    if (!mf || mf->fileSize == 0) {
        *outLineCount = 0;
        PROFILE_END_VAR(timer, "misc_x11:Platform_FileGetLines");
        return NULL;
    }

    char *pBuffer = (char *)mmap(NULL, mf->fileSize, PROT_READ | PROT_WRITE, MAP_PRIVATE, mf->fd, 0);
    if (pBuffer == MAP_FAILED) {
        perror("mmap failed in FileGetLines");
        PROFILE_END_VAR(timer, "misc_x11:Platform_FileGetLines");
        return NULL;
    }

    size_t maxLines = 1000;
    size_t count = 0;
    Line *list = (Line *)malloc(maxLines * sizeof(Line));
    if (list == NULL) {
        munmap(pBuffer, mf->fileSize);
        PROFILE_END_VAR(timer, "misc_x11:Platform_FileGetLines");
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
                    PROFILE_END_VAR(timer, "misc_x11:Platform_FileGetLines");
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
                PROFILE_END_VAR(timer, "misc_x11:Platform_FileGetLines");
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
        log_info("Platform_FileGetLines mf=%p outLineCount=%d", mf, outLineCount);;
PROFILE_END_VAR(timer, "misc_x11:Platform_FileGetLines");
    { PROFILE_END_VAR(_pt, "Platform_FileGetLines"); return list; }
}

char** Platform_FileGetStrings(MappedFile *mf, size_t *outLineCount, NewlineInfo **outNewlines, size_t *outNewlineCount) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    if (!mf || mf->fileSize == 0) {
        *outLineCount = 0;
        *outNewlines = NULL;
        *outNewlineCount = 0;
        PROFILE_END_VAR(timer, "misc_x11:Platform_FileGetStrings");
        return NULL;
    }

    size_t maxLines = 1000;
    size_t count = 0;
    char **stringList = (char **)malloc(maxLines * sizeof(char*));
    if (stringList == NULL) { PROFILE_END_VAR(timer, "misc_x11:Platform_FileGetStrings"); return NULL; }

    size_t maxNewlines = 2000;
    size_t newlineCount = 0;
    NewlineInfo *newlines = (NewlineInfo *)malloc(maxNewlines * sizeof(NewlineInfo));
    if (newlines == NULL) {
        free(stringList);
        PROFILE_END_VAR(timer, "misc_x11:Platform_FileGetStrings");
        return NULL;
    }

    char *pBuffer = (char *)mmap(NULL, mf->fileSize, PROT_READ | PROT_WRITE, MAP_PRIVATE, mf->fd, 0);
    if (pBuffer == MAP_FAILED) {
        perror("mmap failed in FileGetStrings");
        free(stringList);
        free(newlines);
        PROFILE_END_VAR(timer, "misc_x11:Platform_FileGetStrings");
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
                    PROFILE_END_VAR(timer, "misc_x11:Platform_FileGetStrings");
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
                    PROFILE_END_VAR(timer, "misc_x11:Platform_FileGetStrings");
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
                        PROFILE_END_VAR(timer, "misc_x11:Platform_FileGetStrings");
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
        log_info("Platform_FileGetStrings mf=%p outLineCount=%d outNewlines=%p outNewlineCount=%d", mf, outLineCount, outNewlines, outNewlineCount);;
PROFILE_END_VAR(timer, "misc_x11:Platform_FileGetStrings");
    { PROFILE_END_VAR(_pt, "Platform_FileGetStrings"); return stringList; }
}

void Platform_CleanUpFile(MappedFile *mf, NewlineInfo *newlines, size_t newlineCount) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    (void)newlineCount;
    if (!mf || !mf->pBuffer) { PROFILE_END_VAR(timer, "misc_x11:Platform_CleanUpFile"); return; }

    munmap(mf->pBuffer, mf->fileSize);
    mf->pBuffer = NULL;

    if (newlines) {
        free(newlines);
    }
    log_info("Platform_CleanUpFile mf=%p newlines=%p newlineCount=%d", mf, newlines, newlineCount);;
PROFILE_END_VAR(timer, "misc_x11:Platform_CleanUpFile");
}

bool Platform_StringContainsCaseInsensitive(const char* haystack, const char* needle) {
        PROFILE_START(_pt);
if (!haystack || !needle) { PROFILE_END_VAR(_pt, "Platform_StringContainsCaseInsensitive"); return false; }
    { PROFILE_END_VAR(_pt, "Platform_StringContainsCaseInsensitive"); return strcasestr(haystack, needle) != NULL; }
}

bool Platform_StringStartsWithCaseInsensitive(const char* str, const char* prefix) {
        PROFILE_START(_pt);
if (!str || !prefix) { PROFILE_END_VAR(_pt, "Platform_StringStartsWithCaseInsensitive"); return false; }
    size_t plen = strlen(prefix);
    if (plen == 0) { PROFILE_END_VAR(_pt, "Platform_StringStartsWithCaseInsensitive"); return true; }
    { PROFILE_END_VAR(_pt, "Platform_StringStartsWithCaseInsensitive"); return strncasecmp(str, prefix, plen) == 0; }
}

long long Platform_GetWallClockMs(void) {
        PROFILE_START(_pt);
struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    { PROFILE_END_VAR(_pt, "Platform_GetWallClockMs"); return (long long)ts.tv_sec * 1000LL + (long long)ts.tv_nsec / 1000000LL; }
}

int Platform_ReplaceLineInMappedFile(MappedFile* mf, int line_index, const char* new_text) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    if (!mf || !new_text || mf->fd < 0) { PROFILE_END_VAR(timer, "misc_x11:Platform_ReplaceLineInMappedFile"); return 0; }

    char* old_data = (char*)malloc(mf->fileSize + 1);
    if (!old_data) { PROFILE_END_VAR(timer, "misc_x11:Platform_ReplaceLineInMappedFile"); return 0; }

    lseek(mf->fd, 0, SEEK_SET);
    if (read(mf->fd, old_data, mf->fileSize) != (ssize_t)mf->fileSize) {
        free(old_data);
        PROFILE_END_VAR(timer, "misc_x11:Platform_ReplaceLineInMappedFile");
        return 0;
    }
    old_data[mf->fileSize] = '\0';

    int scratch_fd = memfd_create("replace_scratch", MFD_CLOEXEC);
    if (scratch_fd == -1) {
        free(old_data);
        PROFILE_END_VAR(timer, "misc_x11:Platform_ReplaceLineInMappedFile");
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
        PROFILE_END_VAR(timer, "misc_x11:Platform_ReplaceLineInMappedFile");
        return 0;
    }
    size_t new_size = sb.st_size;

    char* transfer_buf = (char*)malloc(new_size);
    if (!transfer_buf) {
        close(scratch_fd);
        PROFILE_END_VAR(timer, "misc_x11:Platform_ReplaceLineInMappedFile");
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
        log_info("Platform_ReplaceLineInMappedFile mf=%p line_index=%d new_text='%s'", mf, line_index, new_text ? new_text : "");;
PROFILE_END_VAR(timer, "misc_x11:Platform_ReplaceLineInMappedFile");
    { PROFILE_END_VAR(_pt, "Platform_ReplaceLineInMappedFile"); return 1; }
}


bool Platform_TerminateProcessByName(const char* exe_name) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    if (!exe_name || !*exe_name) { PROFILE_END_VAR(timer, "misc_x11:Platform_TerminateProcessByName"); return false; }
    char tmp[256];
    strncpy(tmp, exe_name, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1]='\0';
    char *ext = strrchr(tmp, '.');
    if (ext && strcmp(ext, ".exe") == 0) *ext = '\0';

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "sh -c 'pkill -9 \"%s\" 2>/dev/null; pkill -9 -f \"%s\" 2>/dev/null; pkill -9 -f \"build/[f]lint\" 2>/dev/null; pkill \"%s\" 2>/dev/null; killall -9 \"%s\" 2>/dev/null; killall -9 -r \"%s\" 2>/dev/null; killall \"%s\" 2>/dev/null' &", tmp, tmp, tmp, tmp, tmp, tmp);

        log_info("Platform_TerminateProcessByName exe_name='%s'", exe_name ? exe_name : "");;
PROFILE_END_VAR(timer, "misc_x11:Platform_TerminateProcessByName");
    { PROFILE_END_VAR(_pt, "Platform_TerminateProcessByName"); return Platform_RunTerminalCommand(cmd,false); }
}

static bool x11_cmd_exists(const char* cmd) {
        PROFILE_START(_pt);
if (!cmd || !cmd[0]) { PROFILE_END_VAR(_pt, "x11_cmd_exists"); return false; }
    if (strchr(cmd, '/')) { PROFILE_END_VAR(_pt, "x11_cmd_exists"); return access(cmd, X_OK) == 0; }
    const char* path = getenv("PATH");
    if (!path || !path[0]) path = "/usr/bin:/bin:/usr/local/bin";
    char buf[4096];
    snprintf(buf, sizeof(buf), "%s", path);
    char* save = NULL;
    for (char* dir = strtok_r(buf, ":", &save); dir; dir = strtok_r(NULL, ":", &save)) {
        char full[4352];
        snprintf(full, sizeof(full), "%s/%s", dir, cmd);
        if (access(full, X_OK) == 0) return true;
    }
    { PROFILE_END_VAR(_pt, "x11_cmd_exists"); return false; }
}

bool Platform_LaunchBrowserAtUrl(char* url,bool new_window) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    char command[2048];
    char full_url[2048];
    char browser_tok[512];
    size_t tok_len = UsedBrowser ? strcspn(UsedBrowser, " \t") : 0;
    if (tok_len >= sizeof(browser_tok)) tok_len = sizeof(browser_tok) - 1;
    if (UsedBrowser) memcpy(browser_tok, UsedBrowser, tok_len);
    browser_tok[tok_len] = '\0';
    bool have_browser = tok_len > 0 && x11_cmd_exists(browser_tok);
    if (Platform_StringStartsWithCaseInsensitive(url,"https://")) {
        snprintf(full_url, sizeof(full_url), "%s", url);
    } else {
        snprintf(full_url, sizeof(full_url), "https://%s", url);
    }
    const char* real_user = Platform_GetRealUser();
    const char* display = getenv("DISPLAY");
    if (!display) display = ":0";
    const char* xauthority = getenv("XAUTHORITY");
    if (!xauthority) xauthority = "";
    const char* env_runtime = getenv("XDG_RUNTIME_DIR");
    char runtime_dir[256];
    if (env_runtime) snprintf(runtime_dir, sizeof(runtime_dir), "%s", env_runtime);
    else snprintf(runtime_dir, sizeof(runtime_dir), "/run/user/1000");
    if (new_window && have_browser) {
      if (Platform_IsRunningAsRoot()) {
          snprintf(command, sizeof(command), 
                   "sudo -u %s DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s %s --new-window \"%s\" > /dev/null 2>&1 &",
                   real_user, display, xauthority, runtime_dir, UsedBrowser, full_url);
      } else {
          snprintf(command, sizeof(command), 
                   "DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s %s --new-window \"%s\" > /dev/null 2>&1 &",
                   display, xauthority, runtime_dir, UsedBrowser, full_url);
      }
      
      PROFILE_END_VAR(timer, "misc_x11:Platform_LaunchBrowserAtfull_url");
      return Platform_RunTerminalCommand(command,false);
    } else {
    if (Platform_IsRunningAsRoot()) {
        snprintf(command, sizeof(command), "sudo -u %s DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s xdg-open \"%s\" > /dev/null 2>&1 &", real_user, display, xauthority, runtime_dir, full_url);
    } else {
        snprintf(command, sizeof(command), "DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s xdg-open \"%s\" > /dev/null 2>&1 &", display, xauthority, runtime_dir, full_url);
    }
    PROFILE_END_VAR(timer, "misc_x11:Platform_LaunchBrowserAtfull_url");
    return Platform_RunTerminalCommand(command,false);
    }
    log_info("Platform_LaunchBrowserAtfull_url full_url='%s' new_window=%d",full_url, new_window);;
    PROFILE_END_VAR(timer, "misc_x11:Platform_LaunchBrowserAtfull_url");
}

bool Platform_QueryBrowser(char* query) {
        PROFILE_START(_pt);
if (!query || !query[0]) { PROFILE_END_VAR(_pt, "Platform_QueryBrowser"); return false; }

    char browser_tok[512];
    size_t tok_len = UsedBrowser ? strcspn(UsedBrowser, " \t") : 0;
    if (tok_len >= sizeof(browser_tok)) tok_len = sizeof(browser_tok) - 1;
    if (UsedBrowser) memcpy(browser_tok, UsedBrowser, tok_len);
    browser_tok[tok_len] = '\0';
    const char* browser = (tok_len > 0 && x11_cmd_exists(browser_tok)) ? UsedBrowser : "xdg-open";
    PROFILE_START(timer);
    char command[2048];
    char url[2048];
    
    char encoded_query[2048] = {0};
    char* src = query;
    char* dst = encoded_query;
    while (*src && (dst - encoded_query) < sizeof(encoded_query) - 1) {
        if (*src == ' ') {
            *dst++ = '%';
            *dst++ = '2';
            *dst++ = '0';
        } else if (*src == '&') {
            *dst++ = '%';
            *dst++ = '2';
            *dst++ = '6';
        } else if (*src == '?') {
            *dst++ = '%';
            *dst++ = '3';
            *dst++ = 'F';
        } else {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
    

    if (Platform_StringContainsCaseInsensitive(UsedSearchEngine, "google")) {
        snprintf(url, sizeof(url), "https://www.google.com/search?q=%s", encoded_query);
    } else if (Platform_StringContainsCaseInsensitive(UsedSearchEngine, "duckduckgo")) {
        snprintf(url, sizeof(url), "https://duckduckgo.com/?q=%s", encoded_query);
    } else if (Platform_StringContainsCaseInsensitive(UsedSearchEngine, "bing")) {
        snprintf(url, sizeof(url), "https://www.bing.com/search?q=%s", encoded_query);
    } else if (Platform_StringContainsCaseInsensitive(UsedSearchEngine, "brave")) {
        snprintf(url, sizeof(url), "https://search.brave.com/search?q=%s", encoded_query);
    } else if (Platform_StringContainsCaseInsensitive(UsedSearchEngine, "startpage")) {
        snprintf(url, sizeof(url), "https://www.startpage.com/sp/search?query=%s", encoded_query);
    } else if (Platform_StringContainsCaseInsensitive(UsedSearchEngine, "yandex")) {
        snprintf(url, sizeof(url), "https://yandex.com/search/?text=%s", encoded_query);
    } else if (Platform_StringContainsCaseInsensitive(UsedSearchEngine, "ecosia")) {
        snprintf(url, sizeof(url), "https://www.ecosia.org/search?q=%s", encoded_query);
    } else if (Platform_StringContainsCaseInsensitive(UsedSearchEngine, "qwant")) {
        snprintf(url, sizeof(url), "https://www.qwant.com/?q=%s", encoded_query);
    } else {
        snprintf(url, sizeof(url), "https://duckduckgo.com/?q=%s", encoded_query);
    }
    
    const char* real_user = Platform_GetRealUser();
    const char* display = getenv("DISPLAY");
    if (!display) display = ":0";
    const char* xauthority = getenv("XAUTHORITY");
    if (!xauthority) xauthority = "";
    const char* env_runtime = getenv("XDG_RUNTIME_DIR");
    char runtime_dir[256];
    if (env_runtime) snprintf(runtime_dir, sizeof(runtime_dir), "%s", env_runtime);
    else snprintf(runtime_dir, sizeof(runtime_dir), "/run/user/1000");
    
    if (Platform_IsRunningAsRoot()) {
        snprintf(command, sizeof(command), 
                 "sudo -u %s DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s %s\"%s\" > /dev/null 2>&1 &",
                 real_user, display, xauthority, runtime_dir, browser, url);
    } else {
        snprintf(command, sizeof(command), 
                 "DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s %s \"%s\" > /dev/null 2>&1 &",
                 display, xauthority, runtime_dir, browser, url);
    }
    
      log_info("Platform_QueryBrowser query='%s'", query ? query : "");;
    PROFILE_END_VAR(timer, "misc_x11:Platform_QueryBrowser");
    { PROFILE_END_VAR(_pt, "Platform_QueryBrowser"); return Platform_RunTerminalCommand(command,false); }
}

char* Platform_GetClipboardEntry() {
        PROFILE_START(_pt);
static long long next_in_ms = 0;
    static int fail_streak = 0;
    static unsigned long last_owner = 0;
    static bool owner_seen = false;
    long long now = Platform_GetWallClockMs();
    unsigned long owner = x11_clipboard_owner();
    if (!owner_seen || owner != last_owner) {
        owner_seen = true;
        last_owner = owner;
        fail_streak = 0;
        next_in_ms = 0;
    }
    if (owner == 0) { PROFILE_END_VAR(_pt, "Platform_GetClipboardEntry"); return NULL; }
    if (fail_streak > 0 && now < next_in_ms) {
        return NULL;
    }
    char* out = NULL;
    if (Platform_ClipboardAvailable()) {
        out = Platform_ClipboardGetText();
        if (out) {
            fail_streak = 0;
            next_in_ms = 0;
            return out;
        }
    }
    out = x11_clipboard_get_text();
    if (!out) {
        fail_streak++;
        if (fail_streak > 4) fail_streak = 4;
        next_in_ms = now + (200LL << fail_streak);
    } else {
        fail_streak = 0;
        next_in_ms = 0;
    }
    { PROFILE_END_VAR(_pt, "Platform_GetClipboardEntry"); return out; }
}

bool Platform_PasteContent(char* content) {
  PROFILE_START(_pt);
log_info("Platform_PasteContent content='%s'", content ? content : "");;
  { PROFILE_END_VAR(_pt, "Platform_PasteContent"); return Wm_Paste(content); }
}
