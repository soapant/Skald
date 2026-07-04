#ifndef WAIT_FOR_PIXEL_HANDLER_H
#define WAIT_FOR_PIXEL_HANDLER_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "pixel_utils.h"
#include "config.h"

extern int CopyToClipboard(const char* text);

static bool HandleWaitForPixelKey(bool alt_down, bool ctrl_down, bool shift_down) {
    long x, y;
    BYTE r, g, b;

    if (!GetCursorPixel(&x, &y, &r, &g, &b)) {
        return false;
    }

    char color_text_buf[256];

    if (alt_down && ctrl_down && !shift_down) {
        snprintf(color_text_buf, sizeof(color_text_buf), 
                 "adb.wait_for_pixel(%ld,%ld,(%d,%d,%d),tolerance=0)\nadb.tap(%ld,%ld)", 
                 x, y, r, g, b, x, y);
    }
    else if (alt_down && !ctrl_down && !shift_down) {
        snprintf(color_text_buf, sizeof(color_text_buf), "adb.wait_for_pixel(%ld,%ld,(%d,%d,%d),tolerance=0)", 
                 x, y, r, g, b);
    }
    else if (alt_down && !ctrl_down && shift_down) {
        snprintf(color_text_buf, sizeof(color_text_buf), "wait_for_pixel(%ld,%ld,%d,%d,%d)", 
                 x, y, r, g, b);
    }
    else {
        return false;
    }

    CopyToClipboard(color_text_buf);
    return true;
}

#endif
