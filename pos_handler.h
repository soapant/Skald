#ifndef POS_HANDLER_H
#define POS_HANDLER_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "pixel_utils.h"
#include "config.h"

static bool HandlePosKey(bool alt_down, bool ctrl_down, bool shift_down) {
    POINT mousePos;
    if (!GetCursorPos(&mousePos)) {
        return false;
    }

    if (alt_down && shift_down && !ctrl_down) {
        char coords_buf[64];
        snprintf(coords_buf, sizeof(coords_buf), "(%ld, %ld)", mousePos.x, mousePos.y);

        char temp_lines[100][512];  // Changed to 512
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
        printf("ALSO NOT IMPLEMENTED");
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
