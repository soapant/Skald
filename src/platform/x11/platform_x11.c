#define _GNU_SOURCE
#include <sys/mman.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <unistd.h>
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>

bool Platform_ShellExecuteWithVerb(const char* path, const char* verb, bool async) {
    (void)path; (void)verb; (void)async;
    fprintf(stderr, "[PLATFORM] Platform_ShellExecuteWithVerb not implemented yet\n");
    return false;
}

bool Platform_LaunchApp(const char* path) {
    (void)path;
    fprintf(stderr, "[PLATFORM] Platform_LaunchApp not implemented yet\n");
    return false;
}

bool Platform_LaunchAppAsAdmin(const char* path) {
    (void)path;
    fprintf(stderr, "[PLATFORM] Platform_LaunchAppAsAdmin not implemented yet\n");
    return false;
}

bool Platform_IsCtrlPressed(void) {
    fprintf(stderr, "[PLATFORM] Platform_IsCtrlPressed not implemented yet\n");
    return false;
}

char* Platform_QueryZoxidePath(const char* query) {
    (void)query;
    fprintf(stderr, "[PLATFORM] Platform_QueryZoxidePath not implemented yet\n");
    return NULL;
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
    (void)text;
    fprintf(stderr, "[PLATFORM] Platform_CopyToClipboard not implemented yet\n");
    return 0;
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
