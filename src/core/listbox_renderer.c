#include "core/types.h"
#include "core/context.h"
#include "core/common_ui.h"
#include "config.h"
#include "platform/draw.h"
#if !defined(_WIN32)
#include "platform/x11/drawing.h"
#endif
#include "helpers/ui.h"
#include "core/timer.h"

#if !defined(_WIN32)
void Core_DrawListBox(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap) {
    PROFILE_START(timer);
    if (!state || !state->has_listbox) { PROFILE_END_VAR(timer, "listbox_renderer:Core_DrawListBox"); return; }
    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    ListBoxInfo *lb = &state->listbox;
    if (!lb->populated_count) { Platform_DestroyFont(font); PROFILE_END_VAR(timer, "listbox_renderer:Core_DrawListBox"); return; }

    int y0 = 70;
    int itemH = FontSize + 14;
    int yOffset = 40;
    if (!WindowTitleText && !VimModeText) yOffset = 0;
    int visible = (state->height - y0 - yOffset) / itemH;
    if (visible < 1) visible = 1;
    if (visible > lb->populated_count) visible = lb->populated_count;

    if (lb->selected_index >= 0) {
        if (lb->selected_index < lb->view_offset + PaddingForScrolling)
            lb->view_offset = lb->selected_index - PaddingForScrolling;
        else if (lb->selected_index >= lb->view_offset + visible - PaddingForScrolling)
            lb->view_offset = lb->selected_index - visible + PaddingForScrolling + 1;
    }
    if (lb->view_offset < 0) lb->view_offset = 0;
    if (lb->view_offset + visible > lb->populated_count)
        lb->view_offset = lb->populated_count - visible;
    if (lb->view_offset < 0) lb->view_offset = 0;

    for (int i = 0; i < visible; i++) {
        int idx = lb->view_offset + i;
        int y = y0 + i * itemH;
        bool sel = (idx == lb->selected_index);
        if (RoundedCorners && CornerRadius>0) {
            int R=6; int t=ListboxBorderThickness; if(t<1) t=1; if(t>4) t=4;
            int bx=1, bw=state->width-2;
            if (sel) {
                Linux_FillRoundedRect(dpy, xd, vis, cmap, rgb_to_hex(COLOR_SELECTED_TEXT), OpacityAlpha16(), bx+t, y+1+t, bw-2*t, itemH-2-2*t, R-1);
            } else if (!EnableShaders || ShaderPreset==0) {
                Linux_FillRoundedRect(dpy, xd, vis, cmap, rgb_to_hex(COLOR_BG), OpacityAlpha16(), bx+t, y+1+t, bw-2*t, itemH-2-2*t, R-1);
            }
        } else {
            Platform_DrawListBackground(state, dpy, xd, vis, cmap, y, itemH, sel);
        }
        //temporary
        const char *icon_path = NULL;
        Platform_DrawListItem(state, dpy, xd, vis, cmap, font, lb, idx, y, 45, icon_path);
    }

    Platform_DestroyFont(font);
    PROFILE_END_VAR(timer, "listbox_renderer:Core_DrawListBox");
}
#else
#include <windows.h>
void Core_DrawListBox(PromptState *state, void *hdc) {
    PROFILE_START(timer);
    if (!state || !state->has_listbox || !hdc) { PROFILE_END_VAR(timer, "listbox_renderer:Core_DrawListBox"); return; }
    ListBoxInfo *lb = &state->listbox;
    HDC hdcReal = (HDC)hdc;
    // Even if empty, parent background already drawn; just return (avoid black)
    if (!lb->populated_count) { PROFILE_END_VAR(timer, "listbox_renderer:Core_DrawListBox"); return; }
    int y0 = 70;
    int itemH = FontSize + 14;
    int yOffset = 40;
    if (!WindowTitleText && !VimModeText) yOffset = 0;
    int visible = (state->height - y0 - yOffset) / itemH;
    if (visible < 1) visible = 1;
    if (visible > lb->populated_count) visible = lb->populated_count;
    if (lb->selected_index >= 0) {
        if (lb->selected_index < lb->view_offset + PaddingForScrolling)
            lb->view_offset = lb->selected_index - PaddingForScrolling;
        else if (lb->selected_index >= lb->view_offset + visible - PaddingForScrolling)
            lb->view_offset = lb->selected_index - visible + PaddingForScrolling + 1;
    }
    if (lb->view_offset < 0) lb->view_offset = 0;
    if (lb->view_offset + visible > lb->populated_count)
        lb->view_offset = lb->populated_count - visible;
    if (lb->view_offset < 0) lb->view_offset = 0;

    HFONT hFont = FlintContext_Get()->g_hMainFont;
    HFONT hOldFont = NULL;
    if (hFont) hOldFont = (HFONT)SelectObject(hdcReal, hFont);
    SetBkMode(hdcReal, TRANSPARENT);

    for (int i = 0; i < visible; i++) {
        int idx = lb->view_offset + i;
        int y = y0 + i * itemH;
        bool sel = (idx == lb->selected_index);
        // Background - cohesive with window gradient when shaders enabled
        if (RoundedCorners && CornerRadius > 0) {
            int R = 6; if (R > CornerRadius) R = CornerRadius;
            int t = ListboxBorderThickness; if (t < 1) t = 1; if (t > 4) t = 4;
            int bx = 1, bw = state->width - 2;
            RECT rcItem = {bx, y, bx + bw, y + itemH};
            // Pre-fill full item rect with window background to avoid black gaps between rounded items
            if (EnableShaders && ShaderPreset != 0) {
                Win32_DrawBackgroundSlice(hdcReal, &rcItem, COLOR_BG, state->width, state->height, rcItem.left, rcItem.top);
            } else if (!ColorfullMode) {
                HBRUSH br = CreateSolidBrush(COLOR_BG);
                FillRect(hdcReal, &rcItem, br);
                DeleteObject(br);
            }
            RECT inner = {bx + t, y + 1 + t, bx + bw - t, y + itemH - 1 - t};
            int innerR = R - 1; if (innerR < 0) innerR = 0;
            if (inner.right <= inner.left || inner.bottom <= inner.top) { inner = rcItem; innerR = R; }
            if (sel) {
                if (EnableSelectionShaders && SelectionShaderPreset != 0) Win32_DrawRoundedSelection(hdcReal, &inner, COLOR_SELECTED_TEXT, innerR);
                else if (EnableShaders && ShaderPreset != 0) Win32_DrawRoundedSelection(hdcReal, &inner, COLOR_SELECTED_TEXT, innerR);
                else if (ColorfullMode) {
                    HBRUSH br = CreateSolidBrush(GetRainbowColor((float)(idx % 100) / 100.0f, 0.55f, GradientBrightness));
                    int save = SaveDC(hdcReal);
                    HRGN rgn = CreateRoundRectRgn(inner.left, inner.top, inner.right + 1, inner.bottom + 1, innerR * 2, innerR * 2);
                    SelectClipRgn(hdcReal, rgn);
                    FillRect(hdcReal, &inner, br);
                    SelectClipRgn(hdcReal, NULL);
                    RestoreDC(hdcReal, save);
                    DeleteObject(rgn);
                    DeleteObject(br);
                } else {
                    Win32_FillRoundedRect(hdcReal, &inner, COLOR_SELECTED_TEXT, innerR);
                }
            } else {
                if (EnableShaders && ShaderPreset != 0) {
                    Win32_FillRoundedRectSlice(hdcReal, &inner, COLOR_BG, state->width, state->height, inner.left, inner.top, innerR);
                } else if (ColorfullMode) {
                    HBRUSH br = CreateSolidBrush(GetRainbowColor((float)(idx % 100) / 100.0f, 0.55f, GradientBrightness));
                    int save = SaveDC(hdcReal);
                    HRGN rgn = CreateRoundRectRgn(inner.left, inner.top, inner.right + 1, inner.bottom + 1, innerR * 2, innerR * 2);
                    SelectClipRgn(hdcReal, rgn);
                    FillRect(hdcReal, &inner, br);
                    SelectClipRgn(hdcReal, NULL);
                    RestoreDC(hdcReal, save);
                    DeleteObject(rgn);
                    DeleteObject(br);
                } else {
                    Win32_FillRoundedRect(hdcReal, &inner, COLOR_BG, innerR);
                }
            }
        } else {
            RECT rc = {0, y, state->width, y + itemH};
            if (sel) {
                if (EnableSelectionShaders && SelectionShaderPreset != 0) Win32_DrawSelectionBackground(hdcReal, &rc, COLOR_SELECTED_TEXT);
                else {
                    HBRUSH br = CreateSolidBrush(COLOR_SELECTED_TEXT);
                    FillRect(hdcReal, &rc, br);
                    DeleteObject(br);
                }
            } else {
                if (EnableShaders && ShaderPreset != 0) {
                    Win32_DrawBackgroundSlice(hdcReal, &rc, COLOR_BG, state->width, state->height, rc.left, rc.top);
                } else if (ColorfullMode) {
                    HBRUSH br = CreateSolidBrush(GetRainbowColor((float)(idx % 100) / 100.0f, 0.55f, GradientBrightness));
                    FillRect(hdcReal, &rc, br);
                    DeleteObject(br);
                } else {
                    HBRUSH br = CreateSolidBrush(COLOR_BG);
                    FillRect(hdcReal, &rc, br);
                    DeleteObject(br);
                }
            }
        }
        // Text
        const char* text = "";
        if (idx >= 0 && idx < lb->populated_count) {
            int raw = lb->matching_indices[idx];
            if (g_alias_display_active && raw >= 0 && raw < g_alias_display_count && g_alias_display[raw][0] != '\0')
                text = g_alias_display[raw];
            else
                text = lb->populated_cache[idx];
        }
        if (!sel) {
            unsigned long ncol, bcol, pcol;
            Platform_GetItemColors(idx, lb->populated_count, &ncol, &bcol, &pcol);
            SetTextColor(hdcReal, ncol);
        } else {
            SetTextColor(hdcReal, COLOR_TEXT);
        }
        RECT textRect = {45, y, state->width - 10, y + itemH};
        const char* alias_pos = strstr(text, " [");
        if (alias_pos) {
            char mainPart[512];
            size_t len = alias_pos - text;
            if (len >= 511) len = 511;
            memcpy(mainPart, text, len);
            mainPart[len] = '\0';
            DrawTextA(hdcReal, mainPart, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
            SIZE sz;
            GetTextExtentPoint32A(hdcReal, mainPart, (int)len, &sz);
            RECT aliasRect = textRect;
            aliasRect.left += sz.cx;
            SetTextColor(hdcReal, COLOR_ALIAS_TEXT);
            DrawTextA(hdcReal, alias_pos, -1, &aliasRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        } else {
            DrawTextA(hdcReal, text, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        }
    }
    if (hOldFont) SelectObject(hdcReal, hOldFont);
    PROFILE_END_VAR(timer, "listbox_renderer:Core_DrawListBox");
}
#endif
