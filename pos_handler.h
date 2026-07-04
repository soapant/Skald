#ifndef POS_HANDLER_H
#define POS_HANDLER_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "pixel_utils.h"
#include "config.h"

extern int ReadMdFile(char lines[100][256], const char* path);
extern void WriteMdFile(char lines[100][256], int count, const char* path);
extern void ClearMdFile(const char* path);
extern void StripCheckboxes(char* str);
extern int CopyToClipboard(const char* text);
const char* TEMP_FILE_PATH = "temp_list_of_positions.md";

static bool HandlePosKey(bool alt_down, bool ctrl_down, bool shift_down) {
    POINT mousePos;
    if (!GetCursorPos(&mousePos)) {
        return false;
    }

    if (alt_down && shift_down && !ctrl_down) {
        char coords_buf[64];
        snprintf(coords_buf, sizeof(coords_buf), "(%ld, %ld)", mousePos.x, mousePos.y);

        char temp_lines[100][256];
        int temp_count = ReadMdFile(temp_lines, TEMP_FILE_PATH);

        if (temp_count < 100) {
            snprintf(temp_lines[temp_count], sizeof(temp_lines[temp_count]), "- [ ] %s", coords_buf);
            temp_count++;
        }

        WriteMdFile(temp_lines, temp_count, TEMP_FILE_PATH);
        
        char massive_buffer[25600] = {0};
        for (int i = 0; i < temp_count; i++) {
            strcat(massive_buffer, temp_lines[i]);
        }

        StripCheckboxes(massive_buffer);
        
        char wrapped_buffer[25600 + 4];
        snprintf(wrapped_buffer, sizeof(wrapped_buffer), "[%s]", massive_buffer);
        CopyToClipboard(wrapped_buffer);
        return true;
    }
    else if (alt_down && ctrl_down && !shift_down) {
        ClearMdFile(TEMP_FILE_PATH);
        return true;
    }
    else if (alt_down && !ctrl_down && !shift_down) {
        char coords_buf[64];
        snprintf(coords_buf, sizeof(coords_buf), "%ld, %ld", mousePos.x, mousePos.y);
        CopyToClipboard(coords_buf);
        return true;
    }

    return false;
}

#endif
