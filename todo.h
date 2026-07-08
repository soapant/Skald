#ifndef TODO_H
#define TODO_H

#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

// Forward declaration of your UI switcher engine from main.c
extern char* PromptQuickSwitcher(const char** items, int count, int width, int height);
extern HWND hPopupWnd;
#define TODO_FILE_PATH "todo.md"

// Clean exposure of the read function for both main.c and todo.h
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

// Clean exposure of the write function for both main.c and todo.h
void WriteMdFile(char lines[100][256], int count, const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s\n", lines[i]);
    }
    fclose(f);
}

// Modifies the markdown file on disk line-by-line during Ctrl+Enter
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

// The complete UI initialization loop block passed by pointer reference
void TriggerTodo(HWND targetPopupWnd, int* currentModeAddress) {
    if (targetPopupWnd != NULL && IsWindow(targetPopupWnd)) {
        return; 
    }

    char todo_lines[100][256];
    int todo_count = ReadMdFile(todo_lines, TODO_FILE_PATH);
    printf("[DEBUG-TODO] Loaded %d existing tasks from disk.\n", todo_count);

    const char* ui_list[100];
    for (int i = 0; i < todo_count; i++) {
        ui_list[i] = todo_lines[i];
    }

    // Assign via pointer tracking back to main.c (MODE_TODO = 3)
    *currentModeAddress = 3; 

    char* raw_input = PromptQuickSwitcher(ui_list, todo_count, 220, 350);

    *currentModeAddress = 0; // MODE_NONE

    if (raw_input == NULL) {
        printf("[DEBUG-TODO] UI closed via Escape or returned NULL string pointer.\n");
        return;
    }

    char user_input[256] = {0};
    strncpy(user_input, raw_input, sizeof(user_input) - 1);
    user_input[strcspn(user_input, "\r\n")] = 0;
    
    printf("[DEBUG-TODO] Raw Captured Input: \"%s\" (Length: %zu)\n", user_input, strlen(user_input));

    if (strlen(user_input) == 0) {
        printf("[DEBUG-TODO] Input string length is 0. Aborting file flush.\n");
        return;
    }

    todo_count = ReadMdFile(todo_lines, TODO_FILE_PATH);

    bool matched_existing = false;
    int match_index = -1;
    for (int i = 0; i < todo_count; i++) {
        if (strcmp(todo_lines[i], user_input) == 0) {
            matched_existing = true;
            match_index = i;
            break;
        }
    }

    if (matched_existing) {
        printf("[DEBUG-TODO] Matching line found at index [%d]. Removing task.\n", match_index);
        for (int i = match_index; i < todo_count - 1; i++) {
            strcpy(todo_lines[i], todo_lines[i + 1]);
        }
        todo_count--;
    } else {
        if (todo_count < 100) {
            if (strncmp(user_input, "- [ ] ", 6) != 0 && strncmp(user_input, "- [x] ", 6) != 0) {
                snprintf(todo_lines[todo_count], sizeof(todo_lines[todo_count]), "- [ ] %s", user_input);
            } else {
                strncpy(todo_lines[todo_count], user_input, sizeof(todo_lines[todo_count]) - 1);
            }
            printf("[DEBUG-TODO] Appending new task to array slot [%d]: \"%s\"\n", todo_count, todo_lines[todo_count]);
            todo_count++;
        }
    }

    WriteMdFile(todo_lines, todo_count, TODO_FILE_PATH);
}

#endif
