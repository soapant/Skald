#ifndef TODO_H
#define TODO_H

#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <winsock.h>
#include "modal.h"
#include "types.h"
#include "config.h"
#include "common_ui.h"

static inline COLORREF GetTagColor(const char* tag_string) {
    if (!tag_string || strlen(tag_string) == 0) return RGB(255,255,255);
    
    if (strncmp(tag_string, "[Project: ", 10) == 0) {
        for (int i = 0; i < TODO_DICT_COUNT; i++) {
            if (TODO_DICT[i].is_alias && strstr(tag_string, TODO_DICT[i].internal_name) != NULL) {
                return TODO_DICT[i].color;
            }
        }
    }
    for (int i = 0; i < TODO_DICT_COUNT; i++) {
        if (!TODO_DICT[i].is_alias && TODO_DICT[i].tag_label && strcmp(tag_string, TODO_DICT[i].tag_label) == 0) {
            return TODO_DICT[i].color;
        }
    }
    return COLOR_TEXT; 
}

static void parse_todo_flags(char* input, TodoFlags* flags, char* clean_text) {
    memset(flags, 0, sizeof(TodoFlags));
    clean_text[0] = '\0';
    
    char processed[256];
    strncpy(processed, input, sizeof(processed) - 1);
    processed[sizeof(processed) - 1] = '\0';
    
    char temp[256];
    strcpy(temp, processed);
    char* token = strtok(temp, " ");
    bool first = true;
    
    while (token != NULL) {
        bool processed_token = false;

        for (int i = 0; i < TODO_DICT_COUNT; i++) {
            if (strcmp(token, TODO_DICT[i].flag) == 0) {
                if (TODO_DICT[i].is_alias) {
                    flags->is_project = true;
                    strncpy(flags->project_name, TODO_DICT[i].internal_name, sizeof(flags->project_name) - 1);
                } else {
                    if (strcmp(TODO_DICT[i].flag, "-m") == 0) flags->is_misc = true;
                    if (strcmp(TODO_DICT[i].flag, "-h") == 0) flags->is_high_priority = true;
                    if (strcmp(TODO_DICT[i].flag, "-l") == 0) flags->is_low_priority = true;
                }
                processed_token = true;
                break;
            }
        }
        
        if (!processed_token && token[0] == '-') {
            if (strncmp(token, "-p", 2) == 0) {
                flags->is_project = true;
                if (strlen(token) > 2) {
                    strncpy(flags->project_name, token + 2, sizeof(flags->project_name) - 1);
                } else {
                    token = strtok(NULL, " ");
                    if (token != NULL) {
                        char* start = token;
                        char* end = token + strlen(token) - 1;
                        if (*start == '"' && *end == '"') { start++; *end = '\0'; }
                        strncpy(flags->project_name, start, sizeof(flags->project_name) - 1);
                    }
                }
            }
        } 
        else if (!processed_token) {
            if (first) {
                strncpy(clean_text, token, 255);
                clean_text[255] = '\0';
                first = false;
            } else {
                size_t current_len = strlen(clean_text);
                if (current_len + strlen(token) + 2 < 256) {
                    strcat(clean_text, " ");
                    strcat(clean_text, token);
                }
            }
        }
        token = strtok(NULL, " ");
    }
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

static void add_todo_to_file(const char* text) {
    printf("ADDING %s\n", text);
    if (!text || strlen(text) == 0) return;
    
    char lines[100][512];
    int count = ReadMdFile(lines, TODO_FILE_PATH);
    
    char input_copy[256];
    strncpy(input_copy, text, sizeof(input_copy) - 1);
    input_copy[sizeof(input_copy) - 1] = '\0';
    
    TodoFlags flags;
    char clean_text[256];
    parse_todo_flags(input_copy, &flags, clean_text);
    
    for (int i = 0; i < count; i++) {
        char existing_clean[256];
        strncpy(existing_clean, lines[i], sizeof(existing_clean) - 1);
        existing_clean[sizeof(existing_clean) - 1] = '\0';
        char* delimiter = strstr(existing_clean, "||");
        if (delimiter) {
            *delimiter = '\0';
        }
        if (strcmp(existing_clean, clean_text) == 0) return;
    }
    
    char new_entry[512];
    char tag_text[128] = {0};
    
    if (flags.is_project) {
        snprintf(tag_text, sizeof(tag_text), "[Project: %s]", flags.project_name);
    } else {
        for (int i = 0; i < TODO_DICT_COUNT; i++) {
            if (!TODO_DICT[i].is_alias && TODO_DICT[i].tag_label) {
                if ((strcmp(TODO_DICT[i].flag, "-m") == 0 && flags.is_misc) ||
                    (strcmp(TODO_DICT[i].flag, "-h") == 0 && flags.is_high_priority) ||
                    (strcmp(TODO_DICT[i].flag, "-l") == 0 && flags.is_low_priority)) {
                    strcpy(tag_text, TODO_DICT[i].tag_label);
                    break;
                }
            }
        }
    }
    
    if (strlen(tag_text) > 0) {
        snprintf(new_entry, sizeof(new_entry), "%s||%s", clean_text, tag_text);
    } else {
        strncpy(new_entry, clean_text, sizeof(new_entry) - 1);
    }
    
    strncpy(lines[count], new_entry, sizeof(lines[count]) - 1);
    count++;
    WriteMdFile(lines, count, TODO_FILE_PATH);
}

static void remove_todo_item_from_file(const char* item_to_remove) {
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

        char compare_text[256];
        strncpy(compare_text, clean_line, sizeof(compare_text) - 1);
        char* delimiter = strstr(compare_text, "||");
        if (delimiter) {
            *delimiter = '\0';
        }
        
        char clean_item[256];
        strncpy(clean_item, item_to_remove, sizeof(clean_item) - 1);
        char* item_delimiter = strstr(clean_item, "||");
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

static void todo_refresh_callback(const char* filter, HWND hListBox) {
    char todo_lines[100][512];
    int todo_count = ReadMdFile(todo_lines, TODO_FILE_PATH);
    
    const char* items[100];
    for (int i = 0; i < todo_count; i++) {
        items[i] = todo_lines[i];
    }
    PopulateListBox(hListBox, filter, items, todo_count, NULL);
    
}

static void todo_enter_callback(const char* selection) {
    char input[256] = {0};
    if (hEditControl != NULL) {
        UI_GetText(hEditControl, input, sizeof(input));
    }
    
    input[strcspn(input, "\r\n")] = '\0';
    size_t input_len = strlen(input);

    int selected_index = UI_GetSelection(hListBox);
    
    if (selected_index != LB_ERR && (input_len == 0 || strcmp(g_populated_cache[selected_index], input) == 0)) {
        const char* item_to_remove = g_populated_cache[selected_index];
        remove_todo_item_from_file(item_to_remove);
        todo_refresh_callback(input, hListBox);
    } 
    else if (input_len > 0) {
        add_todo_to_file(input);
        UI_SetText(hEditControl, "");
        todo_refresh_callback("", hListBox);
    }
}

static void todo_ctrlenter_callback(const char* selection) {
    int selected_index = UI_GetSelection(hListBox);
    
    
    if (selected_index != -1) {
        const char* item_to_remove = g_populated_cache[selected_index];
        remove_todo_item_from_file(item_to_remove);
    } else {
        char input[256] = {0};
        UI_GetText(hEditControl, input, sizeof(input));
        add_todo_to_file(input);
        UI_SetText(hEditControl, "");
    }
    todo_refresh_callback(NULL, hListBox);
    selection_complete = false;
    
}
static void todo_render_callback(HDC hdc, const char* text, RECT* rect, BOOL is_selected, int itemIndex) {
    if (!text || !rect) return;
    
    char main_text[256] = {0};
    char tag_text[128] = {0};
    
    char* delimiter = strstr(text, "||");
    if (delimiter) {
        int len = delimiter - text;
        if (len >= sizeof(main_text)) len = sizeof(main_text) - 1;
        strncpy(main_text, text, len);
        main_text[len] = '\0';
        strncpy(tag_text, delimiter + 2, sizeof(tag_text) - 1);
        tag_text[sizeof(tag_text) - 1] = '\0';
    } else {
        strncpy(main_text, text, sizeof(main_text) - 1);
        main_text[sizeof(main_text) - 1] = '\0';
    }
    
    RECT textRect = *rect;
    textRect.left += 10;
    textRect.top += 4;
    textRect.bottom -= 30;
    
    if (!is_selected) {
        SetTextColor(hdc, GetRainbowColor((float)itemIndex / 20.0f, 0.85f, 0.85f));
    }
    
    HFONT hOldFont = (HFONT)SelectObject(hdc, g_hMainFont);
    DrawTextA(hdc, main_text, -1, &textRect, DT_SINGLELINE | DT_VCENTER);
    
    if (strlen(tag_text) > 0) {
        RECT tagRect = *rect;
        tagRect.left += 14;
        tagRect.top += 28;
        tagRect.bottom -= 4;
        
        HFONT hOldTagFont = (HFONT)SelectObject(hdc, g_hMainFont);
        
        SetTextColor(hdc, GetTagColor(tag_text));
        DrawTextA(hdc, tag_text, -1, &tagRect, DT_LEFT | DT_TOP);
        
        SelectObject(hdc, hOldTagFont);
    }
    
    SelectObject(hdc, hOldFont);
}
static void todo_edit_callback(const char* selection) {
    char* NewTodoName = EditTextPopup(selection,960,540,1000,300);
}

void TriggerTodo(void) {
    char todo_lines[100][512];
    int count = ReadMdFile(todo_lines, TODO_FILE_PATH);

    const char* ui_list[100];
    for (int i = 0; i < count; i++) {
        ui_list[i] = todo_lines[i];
    }

    printf("[TODO] Launching UI Selection Prompt with %d items...\n", count);

    char* selected_item = PromptUser(ui_list, "📝 Todo", count, 920, 650, 56,
                                     todo_refresh_callback,  
                                     todo_enter_callback,    
                                     todo_ctrlenter_callback, 
                                     NULL,                    
                                     todo_render_callback,
                                     todo_edit_callback);
}


#endif
