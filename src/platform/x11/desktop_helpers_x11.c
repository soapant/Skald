#include "platform/x11/desktop_helpers_x11.h"
#include "platform/platform.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

bool Desktop_Parse(const char* desktop_path, char exec_line[1024], char workdir[1024], bool* is_terminal) {
    if (exec_line) exec_line[0] = '\0';
    if (workdir) workdir[0] = '\0';
    if (is_terminal) *is_terminal = false;
    if (!desktop_path) return false;
    FILE* df = fopen(desktop_path, "r");
    if (!df) return false;
    char line[1024];
    while (fgets(line, sizeof(line), df)) {
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
    fclose(df);
    return true;
}

void Desktop_CleanExec(char exec_line[1024]) {
    if (!exec_line || !exec_line[0]) return;
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

bool Desktop_GetAppId(const char* desktop_path, char app_id[256]) {
    if (!desktop_path || !app_id) return false;
    const char* filename = strrchr(desktop_path, '/');
    filename = filename ? filename + 1 : desktop_path;
    strncpy(app_id, filename, 256 - 1);
    app_id[255] = '\0';
    char* ext = strstr(app_id, ".desktop");
    if (ext) *ext = '\0';
    return true;
}
