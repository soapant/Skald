#include "core/flapi.h"

const char* TEMP_FILE_PATH = "temp_list_of_positions.md";

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
        fl_copy_to_clipboard(coords_buf);
    }
#else

#endif
}

void TriggerPosAdd(void) {
#if defined(_WIN32)
    POINT mousePos;
    if (GetCursorPos(&mousePos)) {
        char coords_buf[64];
        snprintf(coords_buf, sizeof(coords_buf), "(%ld, %ld)", mousePos.x, mousePos.y);
        fl_append_unique_line(TEMP_FILE_PATH, coords_buf);
    }
#else

#endif
}

void TriggerPosClear(void) {
  fl_write_string_to_file(TEMP_FILE_PATH, "");
}

void TriggerColorPlain(void) {
    long x, y; uint8_t r, g, b;
    if (GetCursorPixel(&x, &y, &r, &g, &b)) {
        char buf[MaxFeatures];
        snprintf(buf, sizeof(buf), "%d, %d, %d", r, g, b);
        fl_copy_to_clipboard(buf);
    }
}

void TriggerColorParen(void) {
    long x, y; uint8_t r, g, b;
    if (GetCursorPixel(&x, &y, &r, &g, &b)) {
        char buf[MaxFeatures];
        snprintf(buf, sizeof(buf), "(%d, %d, %d)", r, g, b);
        fl_copy_to_clipboard(buf);
    }
}


void TriggerPixelEqualsAdb(void) {
    long x, y; uint8_t r, g, b;
    if (GetCursorPixel(&x, &y, &r, &g, &b)) {
        char buf[MaxFeatures];
        snprintf(buf, sizeof(buf), "adb.pixel_equals(%ld,%ld,(%d,%d,%d))", x, y, r, g, b);
        fl_copy_to_clipboard(buf);
    }
}

void TriggerPixelEqualsFn(void) {
    long x, y; uint8_t r, g, b;
    if (GetCursorPixel(&x, &y, &r, &g, &b)) {
        char buf[MaxFeatures];
        snprintf(buf, sizeof(buf), "fn.pixel_equals(%ld,%ld,(%d,%d,%d))", x, y, r, g, b);
        fl_copy_to_clipboard(buf);
    }
}

void TriggerRegionCoords(void) {
#if defined(_WIN32)
    POINT cursorPos;
    if (!GetCursorPos(&cursorPos)) return;
    
    static long saved_x1 = 0, saved_y1 = 0;
    static int region_step = 0;
    char buffer[MaxFeatures];
    
    if (region_step == 0) {
        saved_x1 = cursorPos.x;
        saved_y1 = cursorPos.y;
        region_step = 1;
        snprintf(buffer,sizeof(buffer),"Started Region Coord At %ld %ld \n",saved_x1,saved_y1);
        SubjectiveMessageDisplay(buffer,140,TimeOutForMessages,COLOR_TEXT);
    } else {
        char buf[MaxFeatures];
        snprintf(buf, sizeof(buf), "%ld,%ld,%ld,%ld", saved_x1, saved_y1, cursorPos.x, cursorPos.y);
        fl_copy_to_clipboard(buf);
        region_step = 0;
    }
#else

#endif
}

void TriggerRegionImage(void) {
#if defined(_WIN32)
    POINT cursorPos;
    if (!GetCursorPos(&cursorPos)) return;
    
    static int s_img_region_step = 0;
    static long s_img_x1 = 0, s_img_y1 = 0, s_img_x2 = 0, s_img_y2 = 0;
    static long s_saved_x1 = 0, s_saved_y1 = 0;
    char buffer[MaxFeatures];
    
    
    if (s_img_region_step == 0) {
        s_img_x1 = cursorPos.x;
        s_img_y1 = cursorPos.y;
        s_img_region_step = 1;
        snprintf(buffer,sizeof(buffer),"Started Region Image At %ld %ld \n",s_saved_x1,s_saved_y1);
        SubjectiveMessageDisplay(buffer,140,TimeOutForMessages,COLOR_TEXT);
    } else {
        s_img_x2 = cursorPos.x;
        s_img_y2 = cursorPos.y;
        s_img_region_step = 0;
        if (WH_TO_HWND(FlintContext_Get()->window.hGlobalTrayWnd) != NULL) {
            SendNotifyMessage(WH_TO_HWND(FlintContext_Get()->window.hGlobalTrayWnd), WM_TRIGGER_IMG_CAPTURE, 0, 0);
        }
    }
#else

#endif
}

FLINT_FEATURE_INIT {
    // temporary
    // fl_register_hotkey("Alt+Ctrl+Y", TriggerRegionImage);
    // fl_register_hotkey("Alt+Shift+Z", TriggerPosCopy);
    // fl_register_hotkey("Alt+Shift+X", TriggerColorPlain);
    // fl_register_hotkey("Alt+Shift+Y", TriggerRegionCoords);
}
