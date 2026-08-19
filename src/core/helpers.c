#include "common.h"
#include <ctype.h>
#if defined(_WIN32)
#include <shellapi.h>
#include <shlwapi.h>
#endif
#include "helpers.h"
#include "core/timer.h"
#include "core/common_ui.h"

int g_refresh_count = 0;
ColorRef DisplayMessageText;
ColorRef g_rainbow_text_cache[MAX_VISIBLE_ITEMS + 1] = {0};
bool draw_background = true;

char* SelectionData[2048] = {0};
int SelectionDataCount = 0;

void SubjectiveMessageDisplay(const char* buffer,int msg_height,int timeout,ColorRef color) {
    if (MessagesEnabled) {
        if (color == COLOR_OF_SELECTION) {
            if (UI_IsListBoxSelected()) {
              DisplayMessageText = COLOR_OF_SELECTION;
            } else {
              DisplayMessageText = COLOR_TEXT;
            }
        }
        else if (color) {
            DisplayMessageText = color;
        }
        else {
            DisplayMessageText = COLOR_TEXT;
        }
        if (WH_TO_HWND(hPopupWnd) != NULL) {
          MSG_TOP(buffer,msg_height,timeout);
        }else {
          MSG_MIDDLE_BOTTOM(buffer,msg_height / 2,timeout);
        }
        DisplayMessageText = COLOR_TEXT;
    }
}
EditorMode g_last_mode = MODE_INSERT;
bool AlwaysEnabled(void) { return true; }
bool ProcessHotkey(const HotkeyBinding* bindings, int bindings_count,
                                  int pressed_vk, int modifiers,
                                  const char* current_title) {
    for (int i = 0; i < bindings_count; i++) {
        const HotkeyBinding* b = &bindings[i];
        
        if (b->vk_code != pressed_vk) continue;
        if (b->modifiers != modifiers) continue;
        
        if (b->is_enabled && !b->is_enabled()) {
            continue;
        }
        
        if (b->blacklist && b->blacklist_count > 0) {
            bool blacklisted = false;
            for (int j = 0; j < b->blacklist_count; j++) {
#if defined(_WIN32)
                bool found = StrStrIA(current_title, b->blacklist[j]) != NULL;
#else
                bool found = strstr(current_title, b->blacklist[j]) != NULL;
#endif
                if (found) {
                    blacklisted = true;
                    break;
                }
            }
            if (blacklisted) return false;
        }
        
        if (AllowOtherFeatureTogglesInFeature) {
#if defined(_WIN32)
            if (IsPopupActive(hPopupWnd)) {
                KillTimer(WH_TO_HWND(hPopupWnd), 999);
                DestroyWindow(WH_TO_HWND(hPopupWnd));
                hPopupWnd = WH_NULL;
            }
#else
            if (IsPopupActive(hPopupWnd)) {
                hPopupWnd = WH_NULL;
            }
#endif
        } else {
          if (IsPopupActive(hPopupWnd)) return false;
        }
        
        if (b->handler) {
            g_timer = TimerStart();
            if (MessageAfterEachHotkey) {
              char buffer[64];
              snprintf(buffer,sizeof(buffer),"[%s] Triggered \n",b->name);
              MSG_BOTTOM(buffer,140,TimeOutForMessages);
            }
            b->handler();
            return true;
        }
    }
    return false;
}

int ParseTimeToSeconds(const char* input) {
    if (!input || strlen(input) == 0) return 0;
    
    int total = 0;
    char buffer[64];
    strncpy(buffer, input, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    char* token = strtok(buffer, " ");
    while (token != NULL) {
        int value = 0;
        char unit = 's';
        int len = strlen(token);
        
        if (len > 1) {
            char last = tolower(token[len - 1]);
            if (last == 's' || last == 'm' || last == 'h' || last == 'd') {
                token[len - 1] = '\0';
                value = atoi(token);
                unit = last;
            } else {
                value = atoi(token);
            }
        } else {
            value = atoi(token);
        }
        
        switch (unit) {
            case 's': total += value; break;
            case 'm': total += value * 60; break;
            case 'h': total += value * 3600; break;
            case 'd': total += value * 86400; break;
            default: total += value; break;
        }
        
        token = strtok(NULL, " ");
    }
    
    return total;
}

int SearchFlagsInText(const char* text, const void* flag_array, int flag_count, size_t flag_size, FoundFlag* matches, int max_matches) {
    if (!text || !flag_array || !matches) return 0;
    
    int match_count = 0;
    char* text_copy = _strdup(text);
    if (!text_copy) return 0;
    
    for (int i = 0; text_copy[i]; i++) {
        text_copy[i] = tolower(text_copy[i]);
    }
    
    for (int i = 0; i < flag_count && match_count < max_matches; i++) {
        const char* flag_str = NULL;
        
        if (flag_size == sizeof(char*)) {
            flag_str = ((char**)flag_array)[i];
        } else if (flag_size == sizeof(ActionDef)) {
            flag_str = ((ActionDef*)flag_array)[i].str;
        } else if (flag_size == sizeof(ConditionDef)) {
            flag_str = ((ConditionDef*)flag_array)[i].str;
        } else if (flag_size == sizeof(TimerDef)) {
            flag_str = ((TimerDef*)flag_array)[i].str;
        } else if (flag_size == sizeof(GenericFlagEntry)) {
            flag_str = ((const GenericFlagEntry*)flag_array)[i].flag;
        }
        
        if (!flag_str) continue;
        
        char* found = strstr(text_copy, flag_str);
        if (found) {
            int before_ok = (found == text_copy || *(found - 1) == ' ');
            
            int after_ok = 1;
            char* after = found + strlen(flag_str);
            if (*after != '\0' && *after != ' ' && *after != '\t') {
                after_ok = 0;
            }
            
            if (before_ok && after_ok) {
                matches[match_count].flag = (char*)flag_str;
                matches[match_count].index = found - text_copy;
                match_count++;
            }
        }
    }
    
    free(text_copy);
    return match_count;
}

char* GetFlagParameter(const char* text, int flag_index) {
    if (!text || flag_index < 0) return NULL;
    
    static char param[256];
    param[0] = '\0';
    
    int pos = flag_index;
    while (text[pos] && text[pos] != ' ') pos++;
    while (text[pos] && text[pos] == ' ') pos++;
    
    int start = pos;
    while (text[pos]) {
        if (text[pos] == '-' && (pos == 0 || text[pos-1] == ' ')) {
            break;
        }
        pos++;
    }
    
    int len = pos - start;
    if (len > 0) {
        strncpy(param, text + start, len);
        param[len] = '\0';
        
        char* end = param + strlen(param) - 1;
        while (end > param && *end == ' ') end--;
        *(end + 1) = '\0';
        
        return param;
    }
    
    return NULL;
}

char* GetCleanText(const char* text) {
    if (!text) return NULL;
    
    static char clean[256];
    clean[0] = '\0';
    
    const char* flags[] = {"-f", "-cg", "-trw", "-tr", "-t"};
    int flag_count = sizeof(flags) / sizeof(flags[0]);
    
    char* text_copy = _strdup(text);
    if (!text_copy) return NULL;
    
    char* result = text_copy;
    char* write_ptr = text_copy;
    
    while (*result) {
        int skip = 0;
        for (int i = 0; i < flag_count; i++) {
            if (strncmp(result, flags[i], strlen(flags[i])) == 0) {
                skip = 1;
                result += strlen(flags[i]);
                while (*result && *result == ' ') result++;
                while (*result && *result != ' ') result++;
                while (*result && *result == ' ') result++;
                break;
            }
        }
        
        if (!skip) {
            *write_ptr = *result;
            write_ptr++;
            result++;
        }
    }
    
    *write_ptr = '\0';
    
    char* start = text_copy;
    while (*start == ' ') start++;
    char* end = text_copy + strlen(text_copy) - 1;
    while (end > start && *end == ' ') end--;
    *(end + 1) = '\0';
    
    strncpy(clean, start, sizeof(clean) - 1);
    clean[sizeof(clean) - 1] = '\0';
    
    free(text_copy);
    return clean;
}

bool IsWindowBlacklisted(const char* window_title, const char* blacklist[], int count) {
    for (int i = 0; i < count; i++) {
#if defined(_WIN32)
        if (StrStrIA(window_title, blacklist[i]) != NULL) {
#else
        if (strstr(window_title, blacklist[i]) != NULL) {
#endif
            return true;
        }
    }
    return false;
}

int UI_LookupTrueIndex(const char* name) {
    if (!g_item_index_map || !name) return -1;
    size_t value = (size_t)HashMap_Get(g_item_index_map, name);
    return value > 0 ? (int)(value - 1) : -1;
}

void SaveFeatureHistory(char* string) {
    char** HistoryList = NULL;
    int count = 0;
    
    HistoryList = (char**)HashMap_Get(feature_history, g_current_feature);
    while (HistoryList && HistoryList[count]) count++;
    
    HistoryList = StringList_Append(HistoryList, &count, string);
    
    HashMap_Put(feature_history, g_current_feature, HistoryList);
}

int ReplaceLineInFile(const char* filepath, int line_index, const char* new_text) {
    if (!filepath || !new_text || line_index < 0) {
        return 0;
    }
    
    FILE* f = fopen(filepath, "r");
    if (!f) {
        return 0;
    }
    
    char lines[100][512];
    int count = 0;
    while (fgets(lines[count], sizeof(lines[count]), f) && count < 100) {
        lines[count][strcspn(lines[count], "\r\n")] = '\0';
        count++;
    }
    fclose(f);
    
    if (line_index >= count) {
        return 0;
    }
    
    strncpy(lines[line_index], new_text, sizeof(lines[line_index]) - 1);
    lines[line_index][sizeof(lines[line_index]) - 1] = '\0';
    
    f = fopen(filepath, "w");
    if (!f) {
        return 0;
    }
    
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s\n", lines[i]);
    }
    fclose(f);
    
    return 1;
}

void WriteToCacheIndex(int index, const char* text) {
    if (index < 0 || index >= MAX_ITEMS || !text) return;
    
    strncpy(g_populated_cache[index], text, sizeof(g_populated_cache[index]) - 1);
    g_populated_cache[index][sizeof(g_populated_cache[index]) - 1] = '\0';
}

int ReadMdFile(char lines[100][512], const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    int count = 0;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), f) && count < 100) {
        buffer[strcspn(buffer, "\r\n")] = '\0';
        if (strlen(buffer) > 0) {
            strncpy(lines[count], buffer, 511);
            lines[count][511] = '\0';
            count++;
        }
    }
    fclose(f);
    return count;
}

void StripCheckboxes(char* str) {
    char* read_ptr = str;
    char* write_ptr = str;
    bool is_first = true;

    while (*read_ptr != '\0') {
        if (strncmp(read_ptr, "- [ ] ", 6) == 0) {
            if (!is_first) {
                *write_ptr = ','; 
                write_ptr++;
            }
            is_first = false; 
            read_ptr += 6;
        } else {
            *write_ptr = *read_ptr;
            write_ptr++;
            read_ptr++;
        }
    }
    *write_ptr = '\0';
}

void WriteMdFile(char lines[100][512], int count, const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s\n", lines[i]);
    }
    fclose(f);
}

HashMap* history_indices = NULL;

bool GetFromHistory() {
    char** HistoryList = (char**)HashMap_Get(feature_history, g_current_feature);
    if (HistoryList) {
        int* idx_ptr = (int*)HashMap_Get(history_indices, g_current_feature);
        int idx = idx_ptr ? *idx_ptr : 0;
        
        int count = 0;
        while (HistoryList[count]) count++;
        
        if (count > 0) {
            idx = (idx - 1 + count) % count;
            
            UI_SetText(hEditControl, HistoryList[idx]);
            UI_SetCursorPosition(hEditControl,strlen(HistoryList[idx]));

            if (!IsNormalMode()) {
              UI_Redraw(hListBox);
            }
            if (!idx_ptr) {
                idx_ptr = malloc(sizeof(int));
                HashMap_Put(history_indices, g_current_feature, idx_ptr);
            }
            *idx_ptr = idx;
            return true;
        } 
    }
    return false;
}

int convert_string_to_int(const char *str) {
    if (*str == '\0') return 0;
    int num = 0;
    for (int i = 0;i < strlen(str);i++) {
        num *= 10;
        num += str[i] - '0';
    }
    return num;
}

void HandleCommandExecution(char* string) {
    int num = convert_string_to_int(string);
    UI_SetSelection(hListBox,num - 1,g_SelectedIndex);
    UI_LB_SetCaretIndex(hListBox, num, TRUE);
}

const char* get_alias_for_item(const char* item) {
    if (!g_add_alias_to_string) return NULL;
    if (!g_current_feature) return NULL;
    if (!item) return NULL;

    for (size_t i = 0; i < ALIAS_ENTRY_COUNT; i++) {
        if (strcmp(Aliases[i].feature, g_current_feature) == 0) continue;
        if (strcmp(item, Aliases[i].expanded_text) == 0) {
            return Aliases[i].alias;
        }
    }
    return NULL;
}
