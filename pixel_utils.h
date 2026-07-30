#ifndef PIXEL_UTILS_H
#define PIXEL_UTILS_H

#include <windows.h>
#include <stdio.h>
#include "helpers.h"

static bool GetCursorPixel(long* out_x, long* out_y, BYTE* out_r, BYTE* out_g, BYTE* out_b) {
    POINT cursorPos;
    if (!GetCursorPos(&cursorPos)) {
        printf("Failed to get cursor position.\n");
        return false;
    }

    HDC hScreenDC = GetDC(NULL);
    if (hScreenDC == NULL) {
        printf("Failed to get screen Device Context.\n");
        return false;
    }

    COLORREF color = GetPixel(hScreenDC, cursorPos.x, cursorPos.y);
    ReleaseDC(NULL, hScreenDC);

    if (color == CLR_INVALID) {
        printf("Failed to get pixel color.\n");
        return false;
    }

    *out_x = cursorPos.x;
    *out_y = cursorPos.y;
    *out_r = GetRValue(color);
    *out_g = GetGValue(color);
    *out_b = GetBValue(color);

    return true;
}

static inline void TriggerPosCopy(void) {
    POINT mousePos;
    if (GetCursorPos(&mousePos)) {
        char coords_buf[64];
        snprintf(coords_buf, sizeof(coords_buf), "%ld, %ld", mousePos.x, mousePos.y);
        CopyToClipboard(coords_buf);
    }
}

static inline void TriggerPosAdd(void) {
    POINT mousePos;
    if (GetCursorPos(&mousePos)) {
        char coords_buf[64];
        snprintf(coords_buf, sizeof(coords_buf), "(%ld, %ld)", mousePos.x, mousePos.y);
        char temp_lines[100][512];
        int temp_count = ReadMdFile(temp_lines, TEMP_FILE_PATH);
        if (temp_count < 100) {
            snprintf(temp_lines[temp_count], sizeof(temp_lines[temp_count]), "- [ ] %s", coords_buf);
            WriteMdFile(temp_lines, temp_count + 1, TEMP_FILE_PATH);
        }
    }
}

static inline void TriggerPosClear(void) {
  printf("THIS IS NOT IMPLEMETNETD");
}

static inline void TriggerColorPlain(void) {
    long x, y; BYTE r, g, b;
    if (GetCursorPixel(&x, &y, &r, &g, &b)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%d, %d, %d", r, g, b);
        CopyToClipboard(buf);
    }
}

static inline void TriggerColorParen(void) {
    long x, y; BYTE r, g, b;
    if (GetCursorPixel(&x, &y, &r, &g, &b)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "(%d, %d, %d)", r, g, b);
        CopyToClipboard(buf);
    }
}


static inline void TriggerPixelEqualsAdb(void) {
    long x, y; BYTE r, g, b;
    if (GetCursorPixel(&x, &y, &r, &g, &b)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "adb.pixel_equals(%ld,%ld,(%d,%d,%d))", x, y, r, g, b);
        CopyToClipboard(buf);
    }
}

static inline void TriggerPixelEqualsFn(void) {
    long x, y; BYTE r, g, b;
    if (GetCursorPixel(&x, &y, &r, &g, &b)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "fn.pixel_equals(%ld,%ld,(%d,%d,%d))", x, y, r, g, b);
        CopyToClipboard(buf);
    }
}

static inline void TriggerRegionCoords(void) {
    POINT cursorPos;
    if (!GetCursorPos(&cursorPos)) return;
    
    static long saved_x1 = 0, saved_y1 = 0;
    static int region_step = 0;
    
    if (region_step == 0) {
        saved_x1 = cursorPos.x;
        saved_y1 = cursorPos.y;
        region_step = 1;
        printf("[REGION] Step 1: %ld,%ld\n", saved_x1, saved_y1);
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "%ld,%ld,%ld,%ld", saved_x1, saved_y1, cursorPos.x, cursorPos.y);
        CopyToClipboard(buf);
        region_step = 0;
        printf("[REGION] Complete: %s\n", buf);
    }
}

static inline void TriggerRegionImage(void) {
    POINT cursorPos;
    if (!GetCursorPos(&cursorPos)) return;
    
    extern int img_region_step;
    extern long img_x1, img_y1, img_x2, img_y2;
    
    
    if (img_region_step == 0) {
        img_x1 = cursorPos.x;
        img_y1 = cursorPos.y;
        img_region_step = 1;
        printf("[IMG REGION] Top-Left: %ld,%ld\n", img_x1, img_y1);
    } else {
        img_x2 = cursorPos.x;
        img_y2 = cursorPos.y;
        img_region_step = 0;
        printf("[IMG REGION] Bottom-Right: %ld,%ld\n", img_x2, img_y2);
        if (hGlobalTrayWnd != NULL) {
            SendNotifyMessage(hGlobalTrayWnd, WM_TRIGGER_IMG_CAPTURE, 0, 0);
        }
    }
}

#endif
