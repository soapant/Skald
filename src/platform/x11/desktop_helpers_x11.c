#include "platform/x11/desktop_helpers_x11.h"
#include "platform/platform.h"
#include "helpers/helpers_text.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "core/timer.h"
bool Desktop_Parse(const char* desktop_path, char exec_line[1024], char workdir[1024], bool* is_terminal) {
        PROFILE_START(_pt);
if (exec_line) exec_line[0] = '\0';
    if (workdir) workdir[0] = '\0';
    if (is_terminal) *is_terminal = false;
    if (!desktop_path) { PROFILE_END_VAR(_pt, "Desktop_Parse"); return false; }
    static char content[16384];
    if (File_ReadAll(desktop_path, content, (long)sizeof(content)) <= 0) { PROFILE_END_VAR(_pt, "Desktop_Parse"); return false; }
    char* cur = content;
    while (*cur) {
        char* nl = strchr(cur, '\n');
        if (nl) *nl = '\0';
        char* line = cur;
        cur = nl ? nl + 1 : cur + strlen(cur);
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (Platform_StringStartsWithCaseInsensitive(p, "Terminal=")) {
            char* v = p + 9;
            while (*v == ' ' || *v == '\t') v++;
            v[strcspn(v, "\r\n")] = '\0';
            if (is_terminal && (Platform_StringContainsCaseInsensitive(v, "true") || strcmp(v, "1") == 0)) *is_terminal = true;
        } else if (Platform_StringStartsWithCaseInsensitive(p, "Exec=")) {
            char* v = p + 5;
            while (*v == ' ' || *v == '\t') v++;
            v[strcspn(v, "\r\n")] = '\0';
            if (exec_line) strncpy(exec_line, v, 1024 - 1);
        } else if (Platform_StringStartsWithCaseInsensitive(p, "Path=")) {
            char* v = p + 5;
            while (*v == ' ' || *v == '\t') v++;
            v[strcspn(v, "\r\n")] = '\0';
            if (workdir) strncpy(workdir, v, 1024 - 1);
        }
    }
    { PROFILE_END_VAR(_pt, "Desktop_Parse"); return true; }
}

void Desktop_CleanExec(char exec_line[1024]) {
        PROFILE_START(_pt);
if (!exec_line || !exec_line[0]) { PROFILE_END_VAR(_pt, "Desktop_CleanExec"); return ; }
    char cleaned[1024] = {0};
    size_t o = 0;
    for (size_t i = 0; exec_line[i] && o < sizeof(cleaned) - 1; i++) {
        if (exec_line[i] == '%' && exec_line[i + 1]) {
            char c = exec_line[i + 1];
            if (c == 'f' || c == 'F' || c == 'u' || c == 'U' || c == 'i' || c == 'c' || c == 'k' || c == 'v' || c == 'm') {
                i++;
                while (exec_line[i + 1] == ' ') i++;
                continue;
            }
        }
        cleaned[o++] = exec_line[i];
    }
    cleaned[o] = '\0';
    char* s = cleaned;
    while (*s == ' ' || *s == '\t') s++;
    char* e = s + strlen(s) - 1;
    while (e > s && (*e == ' ' || *e == '\t')) *e-- = '\0';
    strncpy(exec_line, s, 1024 - 1);
}

