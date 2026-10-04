#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "platform/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_text.h"

HotkeyBinding convert_into_hotkey_binding(char* hotkey_string, void (*on_hotkey_trigger)(void), 
                                          bool (*optional_condition_for_hotkey)(void), 
                                          const char** optional_blacklistedwindows, bool whitelist) { 
    HotkeyBinding output = {0};
    int out_modifiers = 0;
    int out_key = 0;
    
    char buffer[256];
    strncpy(buffer, hotkey_string, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    size_t len = strlen(buffer);
    bool ends_with_plus = false;
    if (len > 2 && buffer[len-1] == '+' && buffer[len-2] == '+') {
        ends_with_plus = true;
        buffer[len-2] = '\0';
    }

    char* token = strtok(buffer, "+");
    while (token != NULL) {
        if (strcasecmp(token, "RShift") == 0 || strcasecmp(token, "RightShift") == 0) {
            out_key = VK_RSHIFT;
        } else if (strcasecmp(token, "LShift") == 0 || strcasecmp(token, "LeftShift") == 0) {
            out_key = VK_LSHIFT;
        } else if (strcasecmp(token, "RCtrl") == 0 || strcasecmp(token, "RightCtrl") == 0 || strcasecmp(token, "RControl") == 0 || strcasecmp(token, "RightControl") == 0) {
            out_key = VK_RCONTROL;
        } else if (strcasecmp(token, "LCtrl") == 0 || strcasecmp(token, "LeftCtrl") == 0 || strcasecmp(token, "LControl") == 0 || strcasecmp(token, "LeftControl") == 0) {
            out_key = VK_LCONTROL;
        } else if (strcasecmp(token, "RAlt") == 0 || strcasecmp(token, "RightAlt") == 0 || strcasecmp(token, "RMenu") == 0) {
            out_key = VK_RMENU;
        } else if (strcasecmp(token, "LAlt") == 0 || strcasecmp(token, "LeftAlt") == 0 || strcasecmp(token, "LMenu") == 0) {
            out_key = VK_LMENU;
        } else if (strcasecmp(token, "RWin") == 0 || strcasecmp(token, "RightWin") == 0 || strcasecmp(token, "RSuper") == 0 || strcasecmp(token, "RightSuper") == 0 || strcasecmp(token, "RMeta") == 0) {
            out_key = VK_RWIN;
        } else if (strcasecmp(token, "LWin") == 0 || strcasecmp(token, "LeftWin") == 0 || strcasecmp(token, "LSuper") == 0 || strcasecmp(token, "LeftSuper") == 0 || strcasecmp(token, "LMeta") == 0) {
            out_key = VK_LWIN;
        } else if (Platform_StringContainsCaseInsensitive(token, "Ctrl") || Platform_StringContainsCaseInsensitive(token, "Control")) {
            out_modifiers |= HOTKEY_MOD_CTRL;
        } else if (Platform_StringContainsCaseInsensitive(token, "Shift")) {
            out_modifiers |= HOTKEY_MOD_SHIFT;
        } else if (Platform_StringContainsCaseInsensitive(token, "Alt")) {
            out_modifiers |= HOTKEY_MOD_ALT;
        } else if (Platform_StringContainsCaseInsensitive(token, "Win") || Platform_StringContainsCaseInsensitive(token, "Super") || Platform_StringContainsCaseInsensitive(token, "Meta") ) {
            out_modifiers |= HOTKEY_MOD_SUPER;
        } else {
            if (strcasecmp(token, "LeftBracket") == 0) out_key = VK_OEM_4;
            else if (strcasecmp(token, "RightBracket") == 0) out_key = VK_OEM_6;
            else if (token[0] == '[' && token[1] == '\0') out_key = VK_OEM_4;
            else if (token[0] == ']' && token[1] == '\0') out_key = VK_OEM_6;
            else if (token[0] == '{' && token[1] == '\0') out_key = VK_OEM_4;
            else if (token[0] == '}' && token[1] == '\0') out_key = VK_OEM_6;
            else if (Platform_StringContainsCaseInsensitive(token, "Space")) out_key = ' ';
            else if (Platform_StringContainsCaseInsensitive(token, "Tab")) out_key = '\t';
            else if (Platform_StringContainsCaseInsensitive(token, "Enter") || Platform_StringContainsCaseInsensitive(token, "Return")) out_key = '\n';
            else if (Platform_StringContainsCaseInsensitive(token, "Escape") || Platform_StringContainsCaseInsensitive(token, "Esc")) out_key = 27;
            else if (Platform_StringContainsCaseInsensitive(token, "{") || Platform_StringContainsCaseInsensitive(token, "[") || Platform_StringContainsCaseInsensitive(token, "OEM_4") || Platform_StringContainsCaseInsensitive(token, "Oem4")) out_key = VK_OEM_4;
            else if (Platform_StringContainsCaseInsensitive(token, "}") || Platform_StringContainsCaseInsensitive(token, "]") || Platform_StringContainsCaseInsensitive(token, "OEM_6") || Platform_StringContainsCaseInsensitive(token, "Oem6")) out_key = VK_OEM_6;
            else if (Platform_StringContainsCaseInsensitive(token, ",") || Platform_StringContainsCaseInsensitive(token, "OEM_COMMA") || Platform_StringContainsCaseInsensitive(token, "OemComma")) out_key = VK_OEM_COMMA;
            else if (Platform_StringContainsCaseInsensitive(token, ".") || Platform_StringContainsCaseInsensitive(token, "OEM_PERIOD") || Platform_StringContainsCaseInsensitive(token, "OemPeriod")) out_key = VK_OEM_PERIOD;
            else if (Platform_StringContainsCaseInsensitive(token, ";") || Platform_StringContainsCaseInsensitive(token, "OEM_1") || Platform_StringContainsCaseInsensitive(token, "Oem1")) out_key = VK_OEM_1;
            else if (Platform_StringContainsCaseInsensitive(token, "/") || Platform_StringContainsCaseInsensitive(token, "OEM_2") || Platform_StringContainsCaseInsensitive(token, "Oem2")) out_key = VK_OEM_2;
            else if (Platform_StringContainsCaseInsensitive(token, "'") || Platform_StringContainsCaseInsensitive(token, "\"") || Platform_StringContainsCaseInsensitive(token, "OEM_7") || Platform_StringContainsCaseInsensitive(token, "Oem7")) out_key = VK_OEM_7;
            else if (Platform_StringContainsCaseInsensitive(token, "`") || Platform_StringContainsCaseInsensitive(token, "OEM_3") || Platform_StringContainsCaseInsensitive(token, "Oem3")) out_key = VK_OEM_3;
            else if (Platform_StringContainsCaseInsensitive(token, "-") || Platform_StringContainsCaseInsensitive(token, "OEM_MINUS") || Platform_StringContainsCaseInsensitive(token, "OemMinus")) out_key = VK_OEM_MINUS;
            else if (Platform_StringContainsCaseInsensitive(token, "=") || Platform_StringContainsCaseInsensitive(token, "OEM_PLUS") || Platform_StringContainsCaseInsensitive(token, "OemPlus")) out_key = VK_OEM_PLUS;
            else out_key = toupper((unsigned char)token[0]);
        }
        token = strtok(NULL, "+");
    }

    if (ends_with_plus) {
        out_key = VK_OEM_PLUS;
    }

    output.vk_code = out_key;
    output.modifiers = out_modifiers;
    output.hotkey_callback = on_hotkey_trigger;
    output.hotkey_condition = optional_condition_for_hotkey;
    output.whitelist = whitelist;

    if (optional_blacklistedwindows) {
        int cnt = 0;
        while (optional_blacklistedwindows[cnt] != NULL) cnt++;
        output.blacklist_count = cnt;

        if (cnt > 0) {

            output.blacklist = malloc(sizeof(char*) * cnt);
            for (int i = 0; i < cnt; i++) {

                output.blacklist[i] = strdup(optional_blacklistedwindows[i]);
            }
        } else {
            output.blacklist = NULL;
        }
    } else {
        output.blacklist = NULL;
        output.blacklist_count = 0;
    }

    return output;
}

StringList String_Split(const char *string, const char *delimiter)
{
    StringList result = {0};

    if (!string || !delimiter || delimiter[0] == '\0')
        return result;

    size_t delimiter_count = 0;
    const char *p = string;

    while ((p = strstr(p, delimiter)) != NULL) {
        delimiter_count++;
        p += strlen(delimiter);
    }

    result.count = delimiter_count + 1;
    result.items = malloc(result.count * sizeof(char *));

    if (!result.items) {
        result.count = 0;
        return result;
    }

    size_t delimiter_len = strlen(delimiter);
    const char *start = string;

    for (size_t i = 0; i < result.count; i++) {
        const char *end = strstr(start, delimiter);

        size_t length;

        if (end)
            length = (size_t)(end - start);
        else
            length = strlen(start);

        result.items[i] = malloc(length + 1);

        if (!result.items[i]) {
            for (size_t j = 0; j < i; j++)
                free(result.items[j]);

            free(result.items);
            result.items = NULL;
            result.count = 0;
            return result;
        }

        memcpy(result.items[i], start, length);
        result.items[i][length] = '\0';

        if (!end)
            break;

        start = end + delimiter_len;
    }

    return result;
}

void StringList_Free(StringList *list)
{
    if (!list)
        return;

    for (size_t i = 0; i < list->count; i++)
        free(list->items[i]);

    free(list->items);

    list->items = NULL;
    list->count = 0;
}


long File_ReadAll(const char *path, char *buffer, long buffer_size)
{
    if (!path || !buffer || buffer_size <= 0)
        return -1;

    buffer[0] = '\0';

    char expanded[4096];

    if (path[0] == '~' && (path[1] == '/' || path[1] == '\0'))
    {
        const char *home = Platform_GetRealHome();

        if (!home || !home[0])
            return -1;

        size_t total = 0;
        size_t hn = strlen(home);

        if (hn >= sizeof(expanded))
            return -1;

        memcpy(expanded, home, hn);
        total = hn;

        const char *rest = path + 1;

        while (*rest && total < sizeof(expanded) - 1)
            expanded[total++] = *rest++;

        expanded[total] = '\0';
        path = expanded;
    }

    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;

    size_t total = 0;

    while (total < (size_t)buffer_size - 1)
    {
        size_t got = fread(buffer + total, 1, (size_t)buffer_size - 1 - total, f);

        if (got == 0)
            break;

        total += got;
    }

    fclose(f);

    buffer[total] = '\0';

    return (long)total;
}

static char String_MatchingClose(char open) {
    switch (open) {
        case '(': return ')';
        case '[': return ']';
        case '{': return '}';
        case '<': return '>';
        case '"': return '"';
        case '\'': return '\'';
        case '`': return '`';
        default: return open;
    }
}

char* String_CutAt(const char* str, char delim) {
    if (!str) return NULL;
    const char* p = strchr(str, delim);
    if (!p) return NULL;
    return strdup(p);
}

char* String_CutBefore(const char* str, char delim) {
    if (!str) return NULL;
    const char* p = strchr(str, delim);
    if (!p) return strdup(str);
    size_t len = (size_t)(p - str);
    char* out = (char*)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, str, len);
    out[len] = '\0';
    return out;
}

char* String_CutAfter(const char* str, char delim) {
    if (!str) return NULL;
    const char* p = strchr(str, delim);
    if (!p) return NULL;
    return strdup(p + 1);
}

char* String_Between(const char* str, char open, char close) {
    if (!str) return NULL;
    const char* start = strchr(str, open);
    if (!start) return NULL;
    start++;
    if (open == close) {
        const char* end = strchr(start, close);
        if (!end) return NULL;
        size_t len = (size_t)(end - start);
        char* out = (char*)malloc(len + 1);
        if (!out) return NULL;
        memcpy(out, start, len);
        out[len] = '\0';
        return out;
    }
    int depth = 1;
    const char* cur = start;
    while (*cur) {
        if (*cur == open) depth++;
        else if (*cur == close) {
            depth--;
            if (depth == 0) {
                size_t len = (size_t)(cur - start);
                char* out = (char*)malloc(len + 1);
                if (!out) return NULL;
                memcpy(out, start, len);
                out[len] = '\0';
                return out;
            }
        }
        cur++;
    }
    return NULL;
}

char* String_Inside(const char* str, char delim) {
    if (!str) return NULL;
    char close = String_MatchingClose(delim);
    if (delim == close) {
        const char* p = str;
        char* last = NULL;
        while ((p = strchr(p, delim)) != NULL) {
            const char* start = p + 1;
            const char* end = strchr(start, close);
            if (!end) break;
            size_t len = (size_t)(end - start);
            char* cur = (char*)malloc(len + 1);
            if (!cur) { free(last); return NULL; }
            memcpy(cur, start, len);
            cur[len] = '\0';
            free(last);
            last = cur;
            p = end + 1;
        }
        return last;
    } else {
        const char* p = str;
        char* last = NULL;
        while ((p = strchr(p, delim)) != NULL) {
            const char* start = p + 1;
            int depth = 1;
            const char* cur = start;
            while (*cur) {
                if (*cur == delim) depth++;
                else if (*cur == close) {
                    depth--;
                    if (depth == 0) break;
                }
                cur++;
            }
            if (*cur != close) break;
            size_t len = (size_t)(cur - start);
            char* cur_out = (char*)malloc(len + 1);
            if (!cur_out) { free(last); return NULL; }
            memcpy(cur_out, start, len);
            cur_out[len] = '\0';
            free(last);
            last = cur_out;
            p = cur + 1;
        }
        return last;
    }
}

char* String_Trim(const char* str) {
    if (!str) return NULL;
    const char* s = str;
    while (*s && isspace((unsigned char)*s)) s++;
    const char* e = str + strlen(str);
    while (e > s && isspace((unsigned char)*(e - 1))) e--;
    size_t len = (size_t)(e - s);
    char* out = (char*)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

char* String_ToLower(const char* str) {
    if (!str) return NULL;
    char* out = strdup(str);
    if (!out) return NULL;
    for (char* p = out; *p; p++) *p = (char)tolower((unsigned char)*p);
    return out;
}

char* String_ToUpper(const char* str) {
    if (!str) return NULL;
    char* out = strdup(str);
    if (!out) return NULL;
    for (char* p = out; *p; p++) *p = (char)toupper((unsigned char)*p);
    return out;
}

bool String_StartsWith(const char* str, const char* prefix) {
    if (!str || !prefix) return false;
    size_t n = strlen(prefix);
    return strncmp(str, prefix, n) == 0;
}

bool String_EndsWith(const char* str, const char* suffix) {
    if (!str || !suffix) return false;
    size_t ls = strlen(str);
    size_t lx = strlen(suffix);
    if (lx > ls) return false;
    return strcmp(str + ls - lx, suffix) == 0;
}

char* String_Replace(const char* str, const char* from, const char* to) {
    if (!str || !from || !to) return NULL;
    size_t lf = strlen(from);
    if (lf == 0) return strdup(str);
    size_t lt = strlen(to);
    size_t count = 0;
    const char* p = str;
    while ((p = strstr(p, from))) { count++; p += lf; }
    size_t ls = strlen(str);
    size_t lo = ls + count * (lt - lf);
    char* out = (char*)malloc(lo + 1);
    if (!out) return NULL;
    char* dst = out;
    const char* src = str;
    while ((p = strstr(src, from))) {
        size_t n = (size_t)(p - src);
        memcpy(dst, src, n); dst += n;
        memcpy(dst, to, lt); dst += lt;
        src = p + lf;
    }
    strcpy(dst, src);
    return out;
}

char* String_Slice(const char* str, int start, int end) {
    if (!str) return NULL;
    int len = (int)strlen(str);
    if (start < 0) start = len + start;
    if (end < 0) end = len + end;
    if (start < 0) start = 0;
    if (end > len) end = len;
    if (end < start) end = start;
    size_t n = (size_t)(end - start);
    char* out = (char*)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, str + start, n);
    out[n] = '\0';
    return out;
}

StringList File_ReadLines(const char *path, long buffer_size)
{
    StringList result = {NULL, 0};

    if (!path || buffer_size <= 0)
        return result;

    char *content = malloc((size_t)buffer_size);

    if (!content)
        return result;

    if (File_ReadAll(path, content, buffer_size) <= 0)
    {
        free(content);
        return result;
    }

    StringList all = String_Split(content, "\n");

    free(content);

    if (!all.items)
        return result;

    size_t kept = 0;

    for (size_t i = 0; i < all.count; i++)
    {
        char *line = all.items[i];

        if (!line)
            continue;

        line[strcspn(line, "\r")] = '\0';

        if (line[0] == '\0')
        {
            free(line);
            continue;
        }

        all.items[kept++] = line;
    }

    if (kept == 0)
    {
        free(all.items);
        return result;
    }

    result.items = all.items;
    result.count = kept;

    return result;
}
