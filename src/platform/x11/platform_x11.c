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

int Platform_FileInit(const char *filePath, MappedFile *mf, size_t initialPadding) {
    (void)filePath; (void)mf; (void)initialPadding;
    fprintf(stderr, "[PLATFORM] Platform_FileInit not implemented yet\n");
    return 0;
}

int Platform_FileAppend(MappedFile *mf, const char *text) {
    (void)mf; (void)text;
    fprintf(stderr, "[PLATFORM] Platform_FileAppend not implemented yet\n");
    return 0;
}

void Platform_FileClose(MappedFile *mf) {
    (void)mf;
    fprintf(stderr, "[PLATFORM] Platform_FileClose not implemented yet\n");
}

Line* Platform_FileGetLines(MappedFile *mf, size_t *outLineCount) {
    (void)mf; (void)outLineCount;
    fprintf(stderr, "[PLATFORM] Platform_FileGetLines not implemented yet\n");
    return NULL;
}

char** Platform_FileGetStrings(MappedFile *mf, size_t *outLineCount, NewlineInfo **outNewlines, size_t *outNewlineCount) {
    (void)mf; (void)outLineCount; (void)outNewlines; (void)outNewlineCount;
    fprintf(stderr, "[PLATFORM] Platform_FileGetStrings not implemented yet\n");
    return NULL;
}

void Platform_CleanUpFile(MappedFile *mf, NewlineInfo *newlines, size_t newlineCount) {
    (void)mf; (void)newlines; (void)newlineCount;
    fprintf(stderr, "[PLATFORM] Platform_CleanUpFile not implemented yet\n");
}

void Platform_SanitizeFile(const char *filePath) {
    (void)filePath;
    fprintf(stderr, "[PLATFORM] Platform_SanitizeFile not implemented yet\n");
}

int Platform_ReplaceLineInMappedFile(MappedFile* mf, int line_index, const char* new_text) {
    (void)mf; (void)line_index; (void)new_text;
    fprintf(stderr, "[PLATFORM] Platform_ReplaceLineInMappedFile not implemented yet\n");
    return 0;
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