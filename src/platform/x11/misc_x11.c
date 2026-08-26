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
#include <linux/input.h>

#include "common.h"
#include "config.h"
#include "helpers.h"
#include "platform/platform.h"


static uid_t ProbeSessionUid(void){
    DIR* d = opendir("/tmp/.X11-unix");
    if (!d) return (uid_t)-1;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] != 'X') continue;
        char path[512];
        snprintf(path, sizeof(path), "/tmp/.X11-unix/%s", e->d_name);
        struct stat st;
        if (stat(path, &st) == 0 && st.st_uid != 0) {
            closedir(d);
            return st.st_uid;
        }
    }
    closedir(d);
    return (uid_t)-1;
}

const char* Platform_GetRealUser(void){
    static char buf[256]={0};
    static bool done=false;
    if(done) return buf[0]?buf:"diggy";
    const char* u=getenv("SUDO_USER");
    if(u && u[0] && strcmp(u,"root")!=0){ strncpy(buf,u,sizeof(buf)-1); done=true; return buf; }
    u=getenv("USER");
    if(u && u[0] && strcmp(u,"root")!=0){ strncpy(buf,u,sizeof(buf)-1); done=true; return buf; }
    u=getenv("LOGNAME");
    if(u && u[0] && strcmp(u,"root")!=0){ strncpy(buf,u,sizeof(buf)-1); done=true; return buf; }
    
    uid_t suid = ProbeSessionUid();
    if (suid != (uid_t)-1) {
        struct passwd* spw = getpwuid(suid);
        if (spw && spw->pw_name) { strncpy(buf,spw->pw_name,sizeof(buf)-1); done=true; return buf; }
    }
    struct passwd* pw=getpwuid(getuid());
    if(pw && pw->pw_name && strcmp(pw->pw_name,"root")!=0){ strncpy(buf,pw->pw_name,sizeof(buf)-1); done=true; return buf; }
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



const char* Platform_GetSessionDisplay(void){
    static char buf[64]={0};
    static bool done=false;
    if(done) return buf[0]?buf:":0";
    const char* env=getenv("DISPLAY");
    if(env && env[0]){ snprintf(buf,sizeof(buf),"%s",env); done=true; return buf; }
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
    return buf[0]?buf:":0";
}



const char* Platform_GetSessionXAuthority(void){
    static char buf[512]={0};
    static bool done=false;
    if(done) return buf[0]?buf:"";
    const char* xa=getenv("XAUTHORITY");
    if(xa && xa[0]){ snprintf(buf,sizeof(buf),"%s",xa); done=true; return buf; }
    snprintf(buf,sizeof(buf),"%s/.Xauthority",Platform_GetRealHome());
    if(access(buf,R_OK)==0){ done=true; return buf; }
    buf[0]='\0'; done=true;
    return "";
}


const char* Platform_GetRuntimeDir(void){
    static char buf[256]={0};
    static bool done=false;
    if(done) return buf[0]?buf:"/run/user/1000";
    const char* rt=getenv("XDG_RUNTIME_DIR");
    if(rt && rt[0]){ snprintf(buf,sizeof(buf),"%s",rt); done=true; return buf; }
    const char* u=Platform_GetRealUser();
    struct passwd* pw=getpwnam(u);
    if(pw){ snprintf(buf,sizeof(buf),"/run/user/%d",(int)pw->pw_uid); done=true; return buf; }
    snprintf(buf,sizeof(buf),"/run/user/%d",(int)getuid()); done=true;
    return buf[0]?buf:"/run/user/1000";
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
    buffer[0] = '\0';

    fp = popen("date", "r");
    if (!fp) return;

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        printf("%s \n", buffer);
    }
    pclose(fp);

    if (buffer[0] != '\0')
        SubjectiveMessageDisplay(buffer,70,TimeOutForMessages * 3,COLOR_TEXT);
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
                size_t newMax = maxLines * 2;
                Line *tmp = (Line *)realloc(list, newMax * sizeof(Line));
                if (!tmp) {
                    for (size_t k = 0; k < count; k++) free(list[k].ptr);
                    free(list);
                    munmap(pBuffer, mf->fileSize);
                    *outLineCount = 0;
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
