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

#define TODO_FILE_PATH "todo.txt"

extern HWND hPopupWnd;

typedef struct {
    bool is_project;
    char project_name[64];
    bool is_high_priority;
    bool is_misc;
    bool is_low_priority;
} TodoFlags;

typedef struct {
    const char* flag;         
    const char* internal_name;
    const char* tag_label;    
    COLORREF color;           
    bool is_alias;            
} TodoConfigEntry;

static const TodoConfigEntry TODO_DICT[] = {
    { "-pc", "Cprogram",          NULL, RGB(98,  160, 234), true  },
    { "-ph", "HeaderC",           NULL, RGB(51,  209, 122), true  },
    { "-pm", "MinecraftModpack",  NULL, RGB(255, 120, 0),   true  },

    { "-m",  NULL,                "[MISC]", RGB(154, 153, 150), false },
    { "-h",  NULL,                "[HIGH]", RGB(246, 97,  81),  false },
    { "-l",  NULL,                "[LOW]",  RGB(94,  92,  100),  false }
};
enum { TODO_DICT_COUNT = sizeof(TODO_DICT) / sizeof(TODO_DICT[0]) };


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
    return RGB(255,255,255); 
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
        GetWindowTextA(hEditControl, input, sizeof(input));
    }
    
    input[strcspn(input, "\r\n")] = '\0';
    size_t input_len = strlen(input);

    int selected_index = UI_GetSelection(hListBox);
    
    if (selected_index != LB_ERR && (input_len == 0 || strcmp(g_populated_cache[selected_index], input) == 0)) {
        const char* item_to_remove = g_populated_cache[selected_index];
        remove_todo_item_from_file(item_to_remove);
        printf("[TODO] Removed: %s\n", item_to_remove);
        todo_refresh_callback(input, hListBox);
    } 
    else if (input_len > 0) {
        add_todo_to_file(input);
        printf("[TODO] Added: %s\n", input);
        SetWindowTextA(hEditControl, "");
        todo_refresh_callback("", hListBox);
    }
}

static void todo_ctrlenter_callback(const char* selection) {
    int selected_index = UI_GetSelection(hListBox);
    
    if (selected_index != -1) {
        const char* item_to_remove = g_populated_cache[selected_index];
        remove_todo_item_from_file(item_to_remove);
        printf("[TODO] Removed via Ctrl+Enter: %s\n", item_to_remove);
    } else {
        char input[256] = {0};
        GetWindowTextA(hEditControl, input, sizeof(input));
        add_todo_to_file(input);
        printf("[TODO] Added: %s\n", input);
        SetWindowTextA(hEditControl, "");
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
        float hue = (float)itemIndex / 20.0f * 240.0f;
        float h = hue / 60.0f;
        float s = 0.85f;
        float v = 0.85f;
        int i = (int)h;
        float f = h - i;
        float p = v * (1 - s);
        float q = v * (1 - s * f);
        float t = v * (1 - s * (1 - f));
        float r, g, b;
        switch (i) {
            case 0: r = v; g = t; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t; g = p; b = v; break;
            default: r = v; g = p; b = q; break;
        }
        SetTextColor(hdc, RGB((BYTE)(r * 255), (BYTE)(g * 255), (BYTE)(b * 255)));
    }
    
    HFONT hMainFont = CreateFontA(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "0xProto Nerd Font");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hMainFont);
    DrawTextA(hdc, main_text, -1, &textRect, DT_SINGLELINE | DT_VCENTER);
    
    if (strlen(tag_text) > 0) {
        RECT tagRect = *rect;
        tagRect.left += 14;
        tagRect.top += 24;
        tagRect.top += 4;
        tagRect.bottom -= 4;
        
        HFONT hTagFont = CreateFontA(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "0xProto Nerd Font");
        HFONT hOldTagFont = (HFONT)SelectObject(hdc, hTagFont);
        
        COLORREF tagColor;
        tagColor = GetTagColor(tag_text);
        
        SetTextColor(hdc, tagColor);
        DrawTextA(hdc, tag_text, -1, &tagRect, DT_LEFT | DT_TOP);
        
        SelectObject(hdc, hOldTagFont);
        DeleteObject(hTagFont);
    }
    
    SelectObject(hdc, hOldFont);
    DeleteObject(hMainFont);
}

void TriggerTodo(void) {
    char todo_lines[100][512];
    int count = ReadMdFile(todo_lines, TODO_FILE_PATH);

    const char* ui_list[100];
    for (int i = 0; i < count; i++) {
        ui_list[i] = todo_lines[i];
    }

    printf("[TODO] Launching UI Selection Prompt with %d items...\n", count);

    char* selected_item = PromptUser(ui_list, "📝 Todo List", count, 920, 650, 56,
                                     todo_refresh_callback,  
                                     todo_enter_callback,    
                                     todo_ctrlenter_callback, 
                                     NULL,                    
                                     todo_render_callback);
}


#endif
