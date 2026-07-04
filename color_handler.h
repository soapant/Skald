#ifndef COLOR_HANDLER_H
#define COLOR_HANDLER_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "pixel_utils.h"
#include "config.h"

extern int CopyToClipboard(const char* text);

static bool HandleColorKey(bool alt_down, bool ctrl_down, bool shift_down) {
    long x, y;
    BYTE r, g, b;

    if (!GetCursorPixel(&x, &y, &r, &g, &b)) {
        return false;
    }

    char color_text_buf[128];

    if (alt_down && ctrl_down && !shift_down) {
        snprintf(color_text_buf, sizeof(color_text_buf), "(%d, %d, %d)", r, g, b);
    }
    else if (alt_down && !ctrl_down && !shift_down) {
        snprintf(color_text_buf, sizeof(color_text_buf), "%d, %d, %d", r, g, b);
    }
    else {
        return false;
    }

    CopyToClipboard(color_text_buf);
    return true;
}

#endif
