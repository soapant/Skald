#include "helpers/helpers_flag.h"
#include "core/flapi.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

int ParseTimeToSeconds(const char* input) {
    return fl_parse_time(input);
}

int SearchFlagsInText(const char* text, const void* flag_array, int flag_count, size_t flag_size, FoundFlag* matches, int max_matches) {
    if (!text || !flag_array || !matches) return 0;
    int found = 0;
    for (int i = 0; i < flag_count && found < max_matches; i++) {
        const char* flag = *(const char**)((const char*)flag_array + i * flag_size);
        if (!flag) continue;
        const char *p = text;
        while ((p = strstr(p, flag)) != NULL) {
            bool before_ok = (p == text || isspace((unsigned char)p[-1]));
            bool after_ok = (p[strlen(flag)] == '\0' || isspace((unsigned char)p[strlen(flag)]) || p[strlen(flag)] == '=');
            if (before_ok && after_ok) {
                matches[found].flag = (char*)flag;
                matches[found].index = p - text;
                found++;
                break;
            }
            p += strlen(flag);
        }
    }
    return found;
}

char* GetFlagParameter(const char* text, int flag_index) {
    static char buf[256];
    buf[0] = '\0';
    if (!text || flag_index < 0 || flag_index >= (int)strlen(text)) return buf;
    const char *p = text + flag_index;
    while (*p && !isspace((unsigned char)*p) && *p != '=') p++;
    if (*p == '=') p++;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return buf;
    if (*p == '"' || *p == '\'') {
        char q = *p++;
        const char *start = p;
        while (*p && *p != q) p++;
        size_t len = p - start;
        if (len >= sizeof(buf)) len = sizeof(buf)-1;
        memcpy(buf, start, len); buf[len]='\0';
    } else {
        const char *start = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        size_t len = p - start;
        if (len >= sizeof(buf)) len = sizeof(buf)-1;
        memcpy(buf, start, len); buf[len]='\0';
    }
    return buf;
}
