#ifndef HELPERS_FILE_H
#define HELPERS_FILE_H

#include "core/types.h"

typedef struct {
    const char* filepath;
    MappedFile* file;
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
int Helpers_ReadFile(char lines[100][512], const char* path);
void Helpers_WriteFile(char lines[100][512], int count, const char* path);
int ReplaceLineInFile(const char* filepath, int line_index, const char* new_text);
void WriteToCacheIndex(ListBoxInfo* lb, int index, const char* text);

#endif
