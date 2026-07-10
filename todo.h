#ifndef TODO_H
#define TODO_H

#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "types.h"

extern char* PromptQuickSwitcher(const char* window_list[], int count, int height, int width, 
                                 RefreshCallback refresh_callback, 
                                 EnterCallback enter_callback,
                                 CtrlEnterCallback ctrl_enter_callback,
                                 TabCallback tab_callback);
extern HWND hPopupWnd;
extern HWND hListBox;
extern HWND hEditControl;

#define TODO_FILE_PATH "todo.md"

int ReadMdFile(char lines[100][256], const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    
    int count = 0;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), f) && count < 100) {
        buffer[strcspn(buffer, "\r\n")] = '\0';
        if (strlen(buffer) > 0) {
            strncpy(lines[count], buffer, 255);
            lines[count][255] = '\0';
            count++;
        }
    }
    fclose(f);
    return count;
}

void WriteMdFile(char lines[100][256], int count, const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s\n", lines[i]);
    }
    fclose(f);
}

static void add_todo_to_file(const char* text) {
    if (!text || strlen(text) == 0) return;
    
    char lines[100][256];
    int count = ReadMdFile(lines, TODO_FILE_PATH);
    
    // Check if already exists
    for (int i = 0; i < count; i++) {
        if (strcmp(lines[i], text) == 0) return;
    }
    
    // Format with checkbox if not already
    char new_entry[256];
    if (strncmp(text, "- [ ] ", 6) != 0 && strncmp(text, "- [x] ", 6) != 0) {
        snprintf(new_entry, sizeof(new_entry), "- [ ] %s", text);
    } else {
        strncpy(new_entry, text, sizeof(new_entry) - 1);
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

    char line[256];
    while (fgets(line, sizeof(line), infile)) {
        char clean_line[256];
        strncpy(clean_line, line, sizeof(clean_line) - 1);
        clean_line[sizeof(clean_line) - 1] = '\0';
        clean_line[strcspn(clean_line, "\r\n")] = '\0';

        if (strcmp(clean_line, item_to_remove) == 0) {
            printf("[TODO-MGR] Dropped file line item: \"%s\"\n", item_to_remove);
            continue; 
        }
        fprintf(outfile, "%s", line);
    }

    fclose(infile);
    fclose(outfile);

    DeleteFileA(TODO_FILE_PATH);
    MoveFileA("todo.tmp", TODO_FILE_PATH);
}

static void todo_refresh_callback(const char* filter, HWND hListBox) {
    if (!hListBox) return;
    
    char todo_lines[100][256];
    int todo_count = ReadMdFile(todo_lines, TODO_FILE_PATH);
    
    SendMessageA(hListBox, LB_RESETCONTENT, 0, 0);
    
    if (filter == NULL || strlen(filter) == 0) {
        for (int i = 0; i < todo_count; i++) {
            SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)todo_lines[i]);
        }
    } else {
        for (int i = 0; i < todo_count; i++) {
            if (StrStrIA(todo_lines[i], filter) != NULL) {
                SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)todo_lines[i]);
            }
        }
    }
    SendMessageA(hListBox, LB_SETCURSEL, 0, 0);
}

static void todo_enter_callback(const char* selection) {
    // Check if something is selected in the list box
    int selected_index = LB_ERR;
    if (hListBox != NULL) {
        selected_index = SendMessageA(hListBox, LB_GETCURSEL, 0, 0);
    }
    
    if (selected_index != LB_ERR) {
        // Something is selected -> remove it
        remove_todo_item_from_file(selection);
        printf("[TODO] Removed: %s\n", selection);
    } else {
        // Nothing selected -> add new todo from edit box
        char input[256] = {0};
        if (hEditControl != NULL) {
            GetWindowTextA(hEditControl, input, sizeof(input));
            if (strlen(input) > 0) {
                add_todo_to_file(input);
                printf("[TODO] Added: %s\n", input);
            }
        }
    }
}
static void todo_ctrlenter_callback(const char* selection) {
    // Get the selected item from the list box directly
    int selected_index = LB_ERR;
    char selected_text[256] = {0};
    
    if (hListBox != NULL) {
        selected_index = SendMessageA(hListBox, LB_GETCURSEL, 0, 0);
        if (selected_index != LB_ERR) {
            SendMessageA(hListBox, LB_GETTEXT, selected_index, (LPARAM)selected_text);
        }
    }
    
    if (selected_index != LB_ERR && strlen(selected_text) > 0) {
        // Remove the selected item
        remove_todo_item_from_file(selected_text);
        printf("[TODO] Ctrl+Enter removed: %s\n", selected_text);
        
        // Refresh the list
        if (hListBox != NULL) {
            todo_refresh_callback(NULL, hListBox);
        }
    }
}

void TriggerTodo(HWND targetPopupWnd) {
    if (targetPopupWnd != NULL && IsWindow(targetPopupWnd)) {
        return; 
    }

    char todo_lines[100][256];
    int todo_count = ReadMdFile(todo_lines, TODO_FILE_PATH);

    const char* ui_list[100];
    for (int i = 0; i < todo_count; i++) {
        ui_list[i] = todo_lines[i];
    }

    PromptQuickSwitcher(ui_list, todo_count, 220, 350, 
                        todo_refresh_callback,  // refresh -> reloads from file on typing
                        todo_enter_callback,    // enter -> remove if selected, add if not
                        todo_ctrlenter_callback,                   // ctrl enter (not needed)
                        NULL);                  // tab
}

#endif
