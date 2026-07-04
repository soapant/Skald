#ifndef REGION_HANDLER_H
#define REGION_HANDLER_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "config.h"

extern int region_step;
extern long saved_x1;
extern long saved_y1;
extern long img_x1, img_y1, img_x2, img_y2;
extern int img_region_step;
extern HWND hGlobalTrayWnd;
extern int CopyToClipboard(const char* text);
#define WM_TRIGGER_IMG_CAPTURE (WM_USER + 2)

static bool HandleRegionKey(bool alt_down, bool ctrl_down, bool shift_down) {
    POINT cursorPos;
    if (!GetCursorPos(&cursorPos)) {
        printf("Failed to get cursor position.\n");
        return false;
    }

    if (alt_down && ctrl_down && !shift_down) {
        if (img_region_step == 0) {
            img_x1 = cursorPos.x;
            img_y1 = cursorPos.y;
            printf("[IMG REGION] Step 1 Saved: Top-Left at %ld,%ld.\n", img_x1, img_y1);
            img_region_step = 1;
        }
        else if (img_region_step == 1) {
            img_x2 = cursorPos.x;
            img_y2 = cursorPos.y;
            printf("[IMG REGION] Step 2 Saved: Bottom-Right at %ld,%ld.\n", img_x2, img_y2);
            img_region_step = 0;

            if (hGlobalTrayWnd != NULL) {
                SendNotifyMessage(hGlobalTrayWnd, WM_TRIGGER_IMG_CAPTURE, 0, 0);
            } else {
                printf("[ERROR] hGlobalTrayWnd is null!\n");
            }
        }
        return true;
    }
    else if (alt_down && !ctrl_down && !shift_down) {
        if (region_step == 0) {
            saved_x1 = cursorPos.x;
            saved_y1 = cursorPos.y;
            printf("[REGION] Step 1 Saved: %ld,%ld.\n", saved_x1, saved_y1);
            region_step = 1;
        }
        else if (region_step == 1) {
            long x2 = cursorPos.x;
            long y2 = cursorPos.y;

            char region_text_buf[128];
            snprintf(region_text_buf, sizeof(region_text_buf), "%ld,%ld,%ld,%ld", 
                     saved_x1, saved_y1, x2, y2);
            CopyToClipboard(region_text_buf);
            printf("[REGION] Complete! Clipboard: %s\n", region_text_buf);

            region_step = 0;
            saved_x1 = 0;
            saved_y1 = 0;
        }
        return true;
    }

    return false;
}

#endif
