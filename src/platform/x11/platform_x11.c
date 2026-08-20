#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <sys/mman.h>
#include <sys/stat.h> 
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <linux/input.h>
#include "platform/platform.h"
#include "config.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>

bool Platform_ShellExecuteWithVerb(const char* path, const char* verb, bool async) {
    if (!path) return false;

    char command[1024];
    if (async) {
        snprintf(command, sizeof(command), "xdg-open \"%s\" > /dev/null 2>&1 &", path);
    } else {
        snprintf(command, sizeof(command), "xdg-open \"%s\" > /dev/null 2>&1", path);
    }

    int status = system(command);

    return (status == 0);
}

bool Platform_LaunchApp(const char* path) {
    if (!path) return false;

    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }

    if (pid == 0) {
        char *args[] = {(char*)path, NULL};
        
        execvp(path, args);
        
        exit(1); 
    }

    return true;
}


bool Platform_LaunchAppAsAdmin(const char* path) {
    if (!path) return false;

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
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        return false; 
    }

    Window root, child;
    int root_x, root_y, win_x, win_y;
    unsigned int mask = 0;

    XQueryPointer(display, DefaultRootWindow(display), 
                  &root, &child, &root_x, &root_y, &win_x, &win_y, &mask);

    XCloseDisplay(display);

    return (mask & ControlMask) != 0;
}

void Linux_GetActiveWindowTitle(char* buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) return;
    buffer[0] = '\0';

    Display *display = XOpenDisplay(NULL);
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

    XCloseDisplay(display);
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
    if (!query) return NULL;

    static char path[1024];
    path[0] = '\0';

    char clean_query[512];
    strncpy(clean_query, query, sizeof(clean_query) - 1);
    clean_query[sizeof(clean_query) - 1] = '\0';
    clean_query[strcspn(clean_query, "\r\n")] = '\0';

    char command[1024];
    snprintf(command, sizeof(command), "zoxide query %s 2>&1", clean_query);

    FILE *pipe = popen(command, "r");
    if (!pipe) {
        return NULL;
    }

    if (fgets(path, sizeof(path) - 1, pipe) != NULL) {
        path[strcspn(path, "\r\n")] = '\0';
    }

    pclose(pipe);

    if (strstr(path, "no match found") || strlen(path) == 0) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "Query {%s} Doesnt Have A Zoxide Path", query);
        
        SubjectiveMessageDisplay(buffer, 140, TimeOutForMessages, COLOR_ERROR);
        return NULL;
    }

    return path;
}

char Platform_GetLastChar(MappedFile *file) {
    char lastChar = '\0';

    lseek(file->fd, -1, SEEK_END);
    
    read(file->fd, &lastChar, 1);

    return lastChar;
}

int Platform_FileInit(const char* filePath, MappedFile *mf) {
    if (!mf) return 0;

    mf->pBuffer = NULL;
    mf->fileSize = 0;

    mf->fd = memfd_create(filePath, MFD_CLOEXEC);
    if (mf->fd == -1) {
        perror("memfd_create failed");
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

    fprintf(pipe, "%s", text);
    
    int status = pclose(pipe);

    snprintf(buffer,sizeof(buffer),"Copied %s",text);
    SubjectiveMessageDisplay(buffer,140,TimeOutForMessages,COLOR_OF_SELECTION);

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
    fprintf(stderr, "[PLATFORM] Platform_GetUniversalTimeSeconds not implemented yet\n");
    return 0.0;
}

void Platform_SaveRegionToPNG(int x1, int y1, int x2, int y2, const char* fullPath) {
    (void)x1; (void)y1; (void)x2; (void)y2; (void)fullPath;
    fprintf(stderr, "[PLATFORM] Platform_SaveRegionToPNG not implemented yet\n");
}
