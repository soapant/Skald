#ifndef PIXEL_UTILS_H
#define PIXEL_UTILS_H

#include <windows.h>
#include <stdio.h>

// Gets cursor position and pixel color at that position
// Returns true if successful, false otherwise
// Output: x, y, r, g, b
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

#endif
