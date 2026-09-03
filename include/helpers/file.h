#ifndef HELPERS_FILE_H
#define HELPERS_FILE_H

#include "core/types.h"

typedef struct {
    const char* filepath;
    ListBoxInfo* lb;
    int selected_index;
    int true_index;
    char* new_text;
    bool restore_selection;
    WindowHandle hListBox;
    WindowHandle hEditControl;
    RefreshCallback refresh_cb;
} EditCallbackData;

int EditCallback_Handle(EditCallbackData* data, RefreshCallback refresh);
int ReadFile(char lines[100][512], const char* path);
void WriteFile(char lines[100][512], int count, const char* path);
int ReplaceLineInFile(const char* filepath, int line_index, const char* new_text);
void WriteToCacheIndex(ListBoxInfo* lb, int index, const char* text);
long GetFileSize(char* filepath);

void fl_edit_text_popup(const char* initial_text, const char* title, int x, int y, const char* filepath, ListBoxInfo* lb, RefreshCallback refresh);

#endif
