#ifndef PIXEL_EQUALS_HANDLER_H
#define PIXEL_EQUALS_HANDLER_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "helpers.h"

static bool HandlePixelEqualsKey(bool alt_down, bool ctrl_down, bool shift_down) {
    long x, y;
    BYTE r, g, b;

    if (!GetCursorPixel(&x, &y, &r, &g, &b)) {
        return false;
    }

    char color_text_buf[128];

    if (alt_down && ctrl_down && !shift_down) {
        snprintf(color_text_buf, sizeof(color_text_buf), "fn.pixel_equals(%ld,%ld,(%d,%d,%d))", x, y, r, g, b);
    }
    else if (alt_down && !ctrl_down && !shift_down) {
        snprintf(color_text_buf, sizeof(color_text_buf), "adb.pixel_equals(%ld,%ld,(%d,%d,%d))", x, y, r, g, b);
    }
    else {
        return false;
    }

    CopyToClipboard(color_text_buf);
    return true;
}

#endif
