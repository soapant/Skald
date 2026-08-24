#include "common.h"
#include "features/pixel_utils.h"
#include "helpers.h"

static bool GetCursorPixel(long* out_x, long* out_y, uint8_t* out_r, uint8_t* out_g, uint8_t* out_b) {
#if defined(_WIN32)
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
#else
    (void)out_x; (void)out_y; (void)out_r; (void)out_g; (void)out_b;
    return false;
#endif
}

void TriggerPosCopy(void) {
#if defined(_WIN32)
    POINT mousePos;
    if (GetCursorPos(&mousePos)) {
        char coords_buf[64];
        snprintf(coords_buf, sizeof(coords_buf), "%ld, %ld", mousePos.x, mousePos.y);
        Platform_CopyToClipboard(coords_buf);
    }
#else
    // Not Implemented
#endif
}

void TriggerPosAdd(void) {
#if defined(_WIN32)
    POINT mousePos;
    if (GetCursorPos(&mousePos)) {
        char coords_buf[64];
        snprintf(coords_buf, sizeof(coords_buf), "(%ld, %ld)", mousePos.x, mousePos.y);
        char temp_lines[100][512];
        int temp_count = ReadMdFile(temp_lines, TEMP_FILE_PATH);
        if (temp_count < 100) {
            snprintf(temp_lines[temp_count], sizeof(temp_lines[temp_count]), "%s", coords_buf);
            WriteMdFile(temp_lines, temp_count + 1, TEMP_FILE_PATH);
        }
    }
#else
    // Not Implemented
#endif
}

void TriggerPosClear(void) {
  char temp_lines[100][512] = {0};
  WriteMdFile(temp_lines,0,TEMP_FILE_PATH);
}

void TriggerColorPlain(void) {
    long x, y; uint8_t r, g, b;
    if (GetCursorPixel(&x, &y, &r, &g, &b)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%d, %d, %d", r, g, b);
        Platform_CopyToClipboard(buf);
    }
}

void TriggerColorParen(void) {
    long x, y; uint8_t r, g, b;
    if (GetCursorPixel(&x, &y, &r, &g, &b)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "(%d, %d, %d)", r, g, b);
        Platform_CopyToClipboard(buf);
    }
}


void TriggerPixelEqualsAdb(void) {
    long x, y; uint8_t r, g, b;
    if (GetCursorPixel(&x, &y, &r, &g, &b)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "adb.pixel_equals(%ld,%ld,(%d,%d,%d))", x, y, r, g, b);
        Platform_CopyToClipboard(buf);
    }
}

void TriggerPixelEqualsFn(void) {
    long x, y; uint8_t r, g, b;
    if (GetCursorPixel(&x, &y, &r, &g, &b)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "fn.pixel_equals(%ld,%ld,(%d,%d,%d))", x, y, r, g, b);
        Platform_CopyToClipboard(buf);
    }
}

void TriggerRegionCoords(void) {
#if defined(_WIN32)
    POINT cursorPos;
    if (!GetCursorPos(&cursorPos)) return;
    
    static long saved_x1 = 0, saved_y1 = 0;
    static int region_step = 0;
    char buffer[128];
    
    if (region_step == 0) {
        saved_x1 = cursorPos.x;
        saved_y1 = cursorPos.y;
        region_step = 1;
        snprintf(buffer,sizeof(buffer),"Started Region Coord At %ld %ld \n",saved_x1,saved_y1);
        SubjectiveMessageDisplay(buffer,140,TimeOutForMessages,COLOR_TEXT);
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "%ld,%ld,%ld,%ld", saved_x1, saved_y1, cursorPos.x, cursorPos.y);
        Platform_CopyToClipboard(buf);
        region_step = 0;
    }
#else
    // Not Implemented
#endif
}

void TriggerRegionImage(void) {
#if defined(_WIN32)
    POINT cursorPos;
    if (!GetCursorPos(&cursorPos)) return;
    
    extern int img_region_step;
    extern long img_x1, img_y1, img_x2, img_y2;
    char buffer[128];
    
    
    if (img_region_step == 0) {
        img_x1 = cursorPos.x;
        img_y1 = cursorPos.y;
        img_region_step = 1;
        snprintf(buffer,sizeof(buffer),"Started Region Image At %ld %ld \n",saved_x1,saved_y1);
        SubjectiveMessageDisplay(buffer,140,TimeOutForMessages,COLOR_TEXT);
    } else {
        img_x2 = cursorPos.x;
        img_y2 = cursorPos.y;
        img_region_step = 0;
        if (WH_TO_HWND(hGlobalTrayWnd) != NULL) {
            SendNotifyMessage(WH_TO_HWND(hGlobalTrayWnd), WM_TRIGGER_IMG_CAPTURE, 0, 0);
        }
    }
#else
    // Not Implemented
#endif
}
