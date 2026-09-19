#include "core/context.h"
#include "core/modal.h"
#include "config.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "config.h"
#include "core/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_flag.h"
#include "helpers/helpers_text.h"
#include "core/timer.h"
#include "platform/ui.h"

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

