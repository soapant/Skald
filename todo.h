#ifndef TODO_H
#define TODO_H

#include <ctype.h>
#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <winsock.h>
#include "helpers.h"
#include "modal.h"
#include "types.h"
#include "config.h"
#include "common_ui.h"

static inline COLORREF GetTagColor(const char* tag_text) {
    if (!tag_text || strlen(tag_text) == 0) return COLOR_TEXT;
    
    for (int i = 0; i < TODO_CONFIG_FLAGS_COUNT; i++) {
        if (TODO_CONFIG_FLAGS_ENTRIES[i].tag_label) {
            if (strstr(tag_text, TODO_CONFIG_FLAGS_ENTRIES[i].tag_label) != NULL) {
                return TODO_CONFIG_FLAGS_ENTRIES[i].color;
            }
        }
    }
    for (int i = 0; i < TODO_ALIAS_DICT_COUNT; i++) {
        if (strstr(tag_text, TODO_ALIAS_DICT[i].alias) != NULL) {
            return COLOR_TEXT;
        }
    }
    return COLOR_TEXT;
}

static void ParseTodoInput(const char* input, TodoAliasEntry* alias_out, TodoConfigEntry* config_out, char* clean_text, char tags_out[][64], int* tag_count) {
    if (!input) return;
    
    memset(alias_out, 0, sizeof(TodoAliasEntry));
    if (config_out) memset(config_out, 0, sizeof(TodoConfigEntry));
    clean_text[0] = '\0';
    *tag_count = 0;
    
    char input_copy[512];
    strncpy(input_copy, input, sizeof(input_copy) - 1);
    input_copy[sizeof(input_copy) - 1] = '\0';
    
    FoundFlag alias_matches[10];
    int alias_match_count = SearchFlagsInText(input_copy, TODO_ALIAS_FLAGS, TODO_ALIAS_FLAGS_COUNT, sizeof(GenericFlagEntry), alias_matches, 10);
    
    for (int i = 0; i < alias_match_count && *tag_count < 10; i++) {
        char* param = GetFlagParameter(input_copy, alias_matches[i].index);
        if (param && strlen(param) > 0) {
            int alias_idx = -1;
            for (int j = 0; j < TODO_ALIAS_DICT_COUNT; j++) {
                if (strcmp(alias_matches[i].flag, TODO_ALIAS_DICT[j].alias) == 0) {
                    alias_idx = j;
                    break;
                }
            }
            if (alias_idx >= 0) {
                snprintf(tags_out[*tag_count], 64, "%s%s %s%s", 
                    TODO_TAG_OPEN, TODO_ALIAS_DICT[alias_idx].alias_label, param, TODO_TAG_CLOSE);
                (*tag_count)++;
            }
        }
    }
    
    FoundFlag config_matches[10];
    int config_match_count = SearchFlagsInText(input_copy, TODO_CONFIG_FLAGS, TODO_CONFIG_FLAGS_COUNT, sizeof(GenericFlagEntry), config_matches, 10);
    
    for (int i = 0; i < config_match_count && *tag_count < 10; i++) {
        for (int j = 0; j < TODO_CONFIG_FLAGS_COUNT; j++) {
            if (strcmp(config_matches[i].flag, TODO_CONFIG_FLAGS[j].flag) == 0) {
                if (TODO_CONFIG_FLAGS_ENTRIES[j].tag_label) {
                    snprintf(tags_out[*tag_count], 64, "%s%s%s", 
                        TODO_TAG_OPEN, TODO_CONFIG_FLAGS_ENTRIES[j].tag_label, TODO_TAG_CLOSE);
                    (*tag_count)++;
                }
                if (config_out && TODO_CONFIG_FLAGS_ENTRIES[j].is_alias) {
                    char* param = GetFlagParameter(input_copy, config_matches[i].index);
                    if (param && strlen(param) > 0) {
                        memcpy((void*)config_out->internal_name, TODO_CONFIG_FLAGS_ENTRIES[j].internal_name, sizeof(config_out->internal_name) - 1);
                        snprintf(tags_out[*tag_count], 64, "%s%s %s%s", 
                            TODO_TAG_OPEN, TODO_CONFIG_FLAGS_ENTRIES[j].internal_name, param, TODO_TAG_CLOSE);
                        (*tag_count)++;
                    }
                }
                break;
            }
        }
    }
    
    char clean[512] = {0};
    char* token = strtok(input_copy, " ");
    bool first = true;
    
    while (token != NULL) {
        bool is_flag = false;
        
        for (int i = 0; i < TODO_ALIAS_FLAGS_COUNT; i++) {
            if (strcmp(token, TODO_ALIAS_FLAGS[i].flag) == 0) {
                is_flag = true;
                token = strtok(NULL, " ");
                break;
            }
        }
        
        if (!is_flag) {
            for (int i = 0; i < TODO_CONFIG_FLAGS_COUNT; i++) {
                if (strcmp(token, TODO_CONFIG_FLAGS[i].flag) == 0) {
                    is_flag = true;
                    if (TODO_CONFIG_FLAGS_ENTRIES[i].is_alias) {
                        token = strtok(NULL, " ");
                    }
                    break;
                }
            }
        }
        
        if (!is_flag && token[0] != '-') {
            if (first) {
                strcpy(clean, token);
                first = false;
            } else {
                strcat(clean, " ");
                strcat(clean, token);
            }
        }
        
        token = strtok(NULL, " ");
    }
    
    strncpy(clean_text, clean, 511);
    clean_text[511] = '\0';
}

static void ParseTodoLine(const char* line, char* clean_text, char tags_out[][64], int* tag_count) {
    clean_text[0] = '\0';
    *tag_count = 0;
    
    char line_copy[512];
    strncpy(line_copy, line, sizeof(line_copy) - 1);
    line_copy[sizeof(line_copy) - 1] = '\0';
    
    char* delimiter = strstr(line_copy, TODO_ENTRY_DELIMITER);
    if (delimiter) {
        *delimiter = '\0';
        strncpy(clean_text, line_copy, 511);
        clean_text[511] = '\0';
        
        char* tag_part = delimiter + strlen(TODO_ENTRY_DELIMITER);
        char* tag_token = strtok(tag_part, TODO_TAG_SEPARATOR);
        while (tag_token && *tag_count < 10) {
            strncpy(tags_out[*tag_count], tag_token, 63);
            tags_out[*tag_count][63] = '\0';
            (*tag_count)++;
            tag_token = strtok(NULL, TODO_TAG_SEPARATOR);
        }
    } else {
        strncpy(clean_text, line_copy, 511);
        clean_text[511] = '\0';
    }
}

static void AddTodoToFile(const char* text) {
    if (!text || strlen(text) == 0) return;
    
    char lines[100][512];
    int count = ReadMdFile(lines, TODO_FILE_PATH);
    
    char input_copy[512];
    strncpy(input_copy, text, sizeof(input_copy) - 1);
    input_copy[sizeof(input_copy) - 1] = '\0';
    
    TodoAliasEntry alias;
    TodoConfigEntry config;
    char clean_text[512];
    char tags[10][64];
    int tag_count = 0;
    
    ParseTodoInput(input_copy, &alias, &config, clean_text, tags, &tag_count);
    
    for (int i = 0; i < count; i++) {
        char existing_clean[512];
        char existing_tags[10][64];
        int existing_tag_count = 0;
        ParseTodoLine(lines[i], existing_clean, existing_tags, &existing_tag_count);
        if (strcmp(existing_clean, clean_text) == 0) return;
    }
    
    char new_entry[512];
    new_entry[0] = '\0';
    strncpy(new_entry, clean_text, sizeof(new_entry) - 1);
    
    if (tag_count > 0) {
        strcat(new_entry, TODO_ENTRY_DELIMITER);
        for (int i = 0; i < tag_count; i++) {
            strcat(new_entry, tags[i]);
            if (i < tag_count - 1) {
                strcat(new_entry, TODO_TAG_SEPARATOR);
            }
        }
    }
    
    strncpy(lines[count], new_entry, sizeof(lines[count]) - 1);
    lines[count][sizeof(lines[count]) - 1] = '\0';
    count++;
    WriteMdFile(lines, count, TODO_FILE_PATH);
}

static void RemoveTodoItemFromFile(const char* item_to_remove) {
    FILE* infile = fopen(TODO_FILE_PATH, "r");
    if (!infile) return;

    FILE* outfile = fopen("todo.tmp", "w");
    if (!outfile) {
        fclose(infile);
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), infile)) {
        char clean_line[512];
        strncpy(clean_line, line, sizeof(clean_line) - 1);
        clean_line[sizeof(clean_line) - 1] = '\0';
        clean_line[strcspn(clean_line, "\r\n")] = '\0';

        char compare_text[512];
        strncpy(compare_text, clean_line, sizeof(compare_text) - 1);
        char* delimiter = strstr(compare_text, TODO_ENTRY_DELIMITER);
        if (delimiter) {
            *delimiter = '\0';
        }
        
        char clean_item[512];
        strncpy(clean_item, item_to_remove, sizeof(clean_item) - 1);
        char* item_delimiter = strstr(clean_item, TODO_ENTRY_DELIMITER);
        if (item_delimiter) {
            *item_delimiter = '\0';
        }
        
        if (strcmp(compare_text, clean_item) == 0) {
            printf("[TODO-MGR] Dropped file line item: \"%s\"\n", item_to_remove);
            continue; 
        }
        fprintf(outfile, "%s\n", clean_line);
    }

    fclose(infile);
    fclose(outfile);

    DeleteFileA(TODO_FILE_PATH);
    MoveFileA("todo.tmp", TODO_FILE_PATH);
}

static void TodoRefreshCallback(const char* filter, HWND hListBox) {
    char todo_lines[100][512];
    int todo_count = ReadMdFile(todo_lines, TODO_FILE_PATH);
    
    const char* items[100];
    for (int i = 0; i < todo_count; i++) {
        items[i] = todo_lines[i];
    }
    PopulateListBox(hListBox, filter, items, todo_count, NULL);
}

static void TodoEnterCallback(const char* selection) {
    char input[256] = {0};
    if (hEditControl != NULL) {
        UI_GetText(hEditControl, input, sizeof(input));
    }
    
    input[strcspn(input, "\r\n")] = '\0';
    size_t input_len = strlen(input);

    int selected_index = UI_GetSelection(hListBox);
    
    if (selected_index != LB_ERR && (input_len == 0 || strcmp(g_populated_cache[selected_index], input) == 0)) {
        const char* item_to_remove = g_populated_cache[selected_index];
        RemoveTodoItemFromFile(item_to_remove);
        TodoRefreshCallback(input, hListBox);
    } 
    else if (input_len > 0) {
        AddTodoToFile(input);
        UI_SetText(hEditControl, "");
        TodoRefreshCallback("", hListBox);
    }
}

static void TodoCtrlEnterCallback(const char* selection) {
    int selected_index = UI_GetSelection(hListBox);
    
    if (selected_index != -1) {
        const char* item_to_remove = g_populated_cache[selected_index];
        RemoveTodoItemFromFile(item_to_remove);
    } else {
        char input[256] = {0};
        UI_GetText(hEditControl, input, sizeof(input));
        AddTodoToFile(input);
        UI_SetText(hEditControl, "");
    }
    TodoRefreshCallback(NULL, hListBox);
    selection_complete = false;
}

static void TodoRenderCallback(HDC hdc, const char* text, RECT* rect, BOOL is_selected, int itemIndex) {
    if (!text || !rect) return;
    
    char main_text[512] = {0};
    char tags[10][64];
    int tag_count = 0;
    
    ParseTodoLine(text, main_text, tags, &tag_count);
    
    RECT textRect = *rect;
    textRect.left += 10;
    textRect.top += 4;
    textRect.bottom -= 30;
    
    if (!is_selected) {
        SetTextColor(hdc, GetRainbowColor((float)itemIndex / 20.0f, 0.85f, 0.85f));
    }
    
    HFONT hOldFont = (HFONT)SelectObject(hdc, g_hMainFont);
    DrawTextA(hdc, main_text, -1, &textRect, DT_SINGLELINE | DT_VCENTER);
    
    if (tag_count > 0) {
        RECT tagRect = *rect;
        tagRect.left += 14;
        tagRect.top += 28;
        tagRect.bottom -= 4;
        
        HFONT hOldTagFont = (HFONT)SelectObject(hdc, g_hMainFont);
        
        int y_offset = 0;
        for (int i = 0; i < tag_count; i++) {
            SetTextColor(hdc, GetTagColor(tags[i]));
            RECT singleTagRect = tagRect;
            singleTagRect.top += y_offset;
            DrawTextA(hdc, tags[i], -1, &singleTagRect, DT_LEFT | DT_TOP);
            y_offset += 18;
        }
        
        SelectObject(hdc, hOldTagFont);
    }
    
    SelectObject(hdc, hOldFont);
}

static void TodoEditCallback(const char* selection) {
    char* NewTodoName = EditTextPopup(selection, 960, 540, 1000, 300);
    
    if (NewTodoName != NULL) {
        WriteToCacheIndex(g_SelectedIndex, NewTodoName);
    }
    ReplaceLineInFile(TODO_FILE_PATH, g_SelectedIndex, NewTodoName);
}

void TriggerTodo(void) {
    char todo_lines[100][512];
    int count = ReadMdFile(todo_lines, TODO_FILE_PATH);

    const char* ui_list[100];
    for (int i = 0; i < count; i++) {
        ui_list[i] = todo_lines[i];
    }

    char* selected_item = PromptUser(ui_list, "📝 Todo", count, 920, 650, 56,
                                     TodoRefreshCallback,  
                                     TodoEnterCallback,    
                                     TodoCtrlEnterCallback, 
                                     NULL,                    
                                     TodoRenderCallback,
                                     TodoEditCallback,
                                     NULL);
}

#endif
