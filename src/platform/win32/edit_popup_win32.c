#include "core/context.h"
#include "core/modal.h"
#include "bindings/vim_state.h"
#include "core/window_manager.h"
#include "core/timer.h"
#include "platform/wm_platform.h"
#include "platform/ui.h"
#include "platform/draw.h"
#include "platform/platform.h"
#include "platform/win32/common_ui_win32.h"
#include "core/prompt_geometry.h"
#include "core/prompt_controller.h"
#include "helpers/helpers_ui.h"
#include "deps/log.h"
#include "config.h"
#include <windows.h>
#include <string.h>
#include <stdio.h>

void win32_fill_popup_key_text(PopupKeyEvent* key) {
    if (!key) return;
    BYTE state[256];
    memset(state, 0, sizeof(state));
    UINT sc = MapVirtualKeyA((UINT)key->vk, MAPVK_VK_TO_VSC);
    wchar_t wbuf[8];
    int wlen = ToUnicodeEx((UINT)key->vk, sc, state, wbuf, 8, 0, GetKeyboardLayout(0));
    if (wlen <= 0) return;
    char out[32];
    int olen = WideCharToMultiByte(CP_UTF8, 0, wbuf, wlen, out, (int)sizeof(out) - 1, NULL, NULL);
    if (olen <= 0) return;
    out[olen] = '\0';
    key->text_len = olen < (int)sizeof(key->text) ? olen : (int)sizeof(key->text) - 1;
    memcpy(key->text, out, (size_t)key->text_len);
}

static int edit_popup_scroll_offset(HDC hdc, const char* text, int text_len, int cursor, int width, int prev_scroll, int* out_cursor_x) {
    PROFILE_START(timer);
    int visible_w = width - PROMPT_EDIT_SCROLL_PAD;
    int cursor_x = 0;
    int text_w = 0;
    if (hdc && text) {
        if (cursor > 0) cursor_x = Platform_MeasureTextWidth(text, cursor, FontSize);
        if (text_len > 0) text_w = Platform_MeasureTextWidth(text, text_len, FontSize);
    }
    int scroll = prev_scroll;
    int margin = 0;
    if (cursor_x - scroll > visible_w - margin) scroll = cursor_x - visible_w + margin;
    if (cursor_x - scroll < margin) scroll = cursor_x - margin;
    if (scroll < 0) scroll = 0;
    if (text_w - scroll < visible_w && scroll > 0) {
        if (text_w < visible_w) scroll = 0;
        else scroll = text_w - visible_w;
    }
    if (cursor_x - scroll < 0) scroll = cursor_x;
    if (cursor_x - scroll > visible_w) scroll = cursor_x - visible_w;
    if (out_cursor_x) *out_cursor_x = cursor_x;
    PROFILE_END_VAR(timer, "edit_popup_win32:edit_popup_scroll_offset");
    return scroll;
}

static int edit_popup_cursor_y(HDC hdc, int box_top) {
    TEXTMETRIC tm;
    memset(&tm, 0, sizeof(tm));
    GetTextMetrics(hdc, &tm);
    int line_h = tm.tmAscent + tm.tmDescent;
    if (line_h <= 0 || FontSize <= 0) return box_top;
    return box_top + (line_h - FontSize) / 2;
}

bool Platform_EditPopupCreateWindow(Popup* self, int x, int y, int w, int h) {
    PROFILE_START(timer);
    WMEditPopupData* data = (WMEditPopupData*)self;
    HINSTANCE hInst = GetModuleHandle(NULL);
    HWND hwnd = CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, "EditPopupClass","", WS_POPUP | WS_CLIPCHILDREN, x, y, w, h, NULL, NULL, hInst, data);
    if (!hwnd) { PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupCreateWindow"); return false; }
    win32_apply_rounded_corners(hwnd, w, h);
    SetLayeredWindowAttributes(hwnd, 0, (BYTE)WindowOpacity, LWA_ALPHA);
    data->base.handle = WH_FROM_HWND(hwnd);
    PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupCreateWindow");
    return true;
}

void Platform_EditPopupShowWindow(Popup* self) {
    PROFILE_START(timer);
    HWND hwnd = WH_TO_HWND(self->handle);
    if (hwnd && IsWindow(hwnd)) {
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);
    }
    PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupShowWindow");
}

void Platform_EditPopupHideWindow(Popup* self) {
    PROFILE_START(timer);
    HWND hwnd = WH_TO_HWND(self->handle);
    if (hwnd && IsWindow(hwnd)) ShowWindow(hwnd, SW_HIDE);
    WindowManager* wm = WindowManager_Get();
    if (wm) {
        Popup* top = WindowManager_GetTopPopup(wm);
        if (top && WH_TO_HWND(top->handle) && IsWindow(WH_TO_HWND(top->handle))) {
            SetForegroundWindow(WH_TO_HWND(top->handle));
            SetFocus(WH_TO_HWND(top->handle));
            if (top->vtable && top->vtable->on_render) top->vtable->on_render(top);
        }
    }
    PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupHideWindow");
}

void Platform_EditPopupRender(Popup* self) {
    PROFILE_START(timer);
    WMEditPopupData* data = (WMEditPopupData*)self;
    HWND hwnd = WH_TO_HWND(self->handle);
    if (!data || !hwnd || !IsWindow(hwnd)) { PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupRender"); return; }

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rect;
    GetClientRect(hwnd, &rect);

    if (EnableShaders && ShaderPreset != 0) win32_draw_background(hdc, &rect);
    else { HBRUSH br = CreateSolidBrush(COLOR_BG); FillRect(hdc, &rect, br); DeleteObject(br); }

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = memDC ? CreateCompatibleBitmap(hdc, rect.right, rect.bottom) : NULL;
    HBITMAP oldBmp = NULL;
    HDC paint = hdc;
    if (memDC && memBmp && rect.right > 0 && rect.bottom > 0) {
        oldBmp = (HBITMAP)SelectObject(memDC, memBmp);
        paint = memDC;
    } else {
        if (memDC) DeleteDC(memDC);
        if (memBmp) DeleteObject(memBmp);
        memDC = NULL;
        memBmp = NULL;
    }

    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;

    Platform_SetDrawContextWin32(paint);
    FontHandle font = Platform_CreateFont(FontSize, UsedFont);
    if (font.hfont) SelectObject(paint, font.hfont);

    if (g_vim_mode != NULL && VimModeText) {
        Platform_DrawTextRaw(PROMPT_EDITPOPUP_MODE_X, PROMPT_EDITPOPUP_MODE_Y, g_vim_mode, VimModeTextColor(), font, "LEFT", PROMPT_EDITPOPUP_MODE_PAD, FontSize);
    }

    int cursor_x = 0;
    int text_len = data->text ? (int)strlen(data->text) : 0;
    int scroll = 0;
    int text_w = 0;
    scroll = edit_popup_scroll_offset(paint, data->text ? data->text : "", text_len, data->cursor, w, 0, &cursor_x);
    if (text_len > 0) text_w = Platform_MeasureTextWidth(data->text, text_len, FontSize);

    int draw_x = PROMPT_EDITPOPUP_INSET - scroll;
    int cursor_draw_x = PROMPT_EDITPOPUP_INSET + cursor_x - scroll;
    int draw_y = h / 2 + PROMPT_EDITPOPUP_CENTER_OFFSET_Y;
    if (text_w > 0 && text_w <= w - 20 && data->align) {
        if (Platform_StringContainsCaseInsensitive(data->align, "CENTER")) {
            draw_x = (w - text_w) / 2;
            cursor_draw_x = draw_x + cursor_x;
        } else if (Platform_StringContainsCaseInsensitive(data->align, "RIGHT")) {
            draw_x = w - PROMPT_EDITPOPUP_INSET - text_w;
            cursor_draw_x = draw_x + cursor_x;
        }
    }

    if (!IsInsertMode() && data->cursor >= 0) {
        int tint_y = edit_popup_cursor_y(paint, draw_y);
        unsigned long tint = LerpColor(COLOR_BG, Modal_CursorColor(), 0.72f);
        int sel_start = 0, sel_end = 0;
        bool has_sel = VimState_GetSelection(&sel_start, &sel_end) && text_len > 0;
        if (has_sel) {
            if (sel_start < 0) sel_start = 0;
            if (sel_end >= text_len) sel_end = text_len - 1;
            if (sel_start <= sel_end) {
                int sx0 = draw_x + Platform_MeasureTextWidth(data->text, sel_start, FontSize);
                int sx1 = draw_x + Platform_MeasureTextWidth(data->text, sel_end + 1, FontSize);
                int cx = cursor_draw_x;
                if (sx0 < PROMPT_CARET_LO_CLAMP) sx0 = PROMPT_CARET_LO_CLAMP;
                if (sx1 > w - PROMPT_CARET_HI_CLAMP) sx1 = w - PROMPT_CARET_HI_CLAMP;
                if (cx < sx0) cx = sx0;
                if (cx > sx1) cx = sx1;
                int selw = sx1 - sx0;
                if (selw > 0) Platform_DrawRectRawOpaque(sx0, tint_y, selw, FontSize, tint);
            }
        } else {
            int tint_x = cursor_draw_x;
            if (tint_x < PROMPT_CARET_LO_CLAMP) tint_x = PROMPT_CARET_LO_CLAMP;
            if (tint_x > w - PROMPT_CARET_HI_CLAMP) tint_x = w - PROMPT_CARET_HI_CLAMP;
            Platform_DrawRectRawOpaque(tint_x, tint_y, PROMPT_EDITPOPUP_CARET_W_NORMAL, FontSize, tint);
        }
    }

    if (text_len > 0) {
        Platform_DrawTextRaw(draw_x, draw_y, data->text, rgb_to_hex(COLOR_TEXT), font, "", 0, 0);
    } else if (data->placeholder && data->placeholder[0]) {
        unsigned long ph = Color_Lerp(rgb_to_hex(COLOR_BG), rgb_to_hex(COLOR_TEXT), 0.45f);
        Platform_DrawTextRaw(draw_x, draw_y, data->placeholder, ph, font, "", 0, 0);
    }

    if (IsInsertMode()) {
        if (cursor_draw_x < PROMPT_CARET_LO_CLAMP) cursor_draw_x = PROMPT_CARET_LO_CLAMP;
        if (cursor_draw_x > w - PROMPT_CARET_HI_CLAMP) cursor_draw_x = w - PROMPT_CARET_HI_CLAMP;
        int cursor_y = edit_popup_cursor_y(paint, draw_y);
        Platform_DrawRectRawOpaque(cursor_draw_x, cursor_y, PROMPT_CARET_W_INSERT, FontSize, Modal_CursorColor());
    }

    Platform_SetDrawContextWin32(NULL);
    Platform_DestroyFont(font);

    if (memDC && memBmp) {
        BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top, memDC, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);
    }
    EndPaint(hwnd, &ps);
    PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupRender");
}

void Platform_EditPopupDestroyWindow(Popup* self) {
    PROFILE_START(timer);
    HWND hwnd = WH_TO_HWND(self->handle);
    if (hwnd && IsWindow(hwnd)) DestroyWindow(hwnd);
    PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupDestroyWindow");
}

bool Platform_EditPopupOnKey(Popup* self, const PopupKeyEvent* key) {
    PROFILE_START(timer);
    WMEditPopupData* ed = (WMEditPopupData*)self;
    if (!ed || !ed->text) { PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey"); return true; }

    int wParam = key->vk;
    bool ctrl_pressed = key->ctrl;
    bool shift_pressed = key->shift;
    bool alt_pressed = key->alt;
    (void)shift_pressed;
    (void)alt_pressed;
    if (PromptController_TryVimMotion(wParam, ctrl_pressed, shift_pressed, alt_pressed)) { PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey"); return true; }

    size_t cap = ed->text_capacity > 0 ? (size_t)ed->text_capacity : 1024;
    size_t n = strlen(ed->text);
    if (ed->cursor < 0) ed->cursor = 0;
    if ((size_t)ed->cursor > n) ed->cursor = (int)n;

    if (wParam == VK_BACK) {
        if (ctrl_pressed) {
            ed->text[0] = '\0';
            ed->cursor = 0;
        } else if (ed->cursor > 0) {
            size_t del = (size_t)ed->cursor - 1;
            memmove(ed->text + del, ed->text + del + 1, n - del);
            ed->cursor--;
        }
        PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey");
        return true;
    }
    if (wParam == VK_ESCAPE) {
        if (IsInsertMode()) {
            SetMode(MODE_NORMAL);
        } else {
            FlintContext_Get()->window.selection_complete = true;
        }
        PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey");
        return true;
    }
    if (wParam == VK_LEFT) {
        if (ed->cursor > 0) ed->cursor--;
        PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey");
        return true;
    }
    if (wParam == VK_RIGHT) {
        if ((size_t)ed->cursor < n) ed->cursor++;
        PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey");
        return true;
    }
    if (wParam == VK_HOME) {
        ed->cursor = 0;
        PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey");
        return true;
    }
    if (wParam == VK_END) {
        ed->cursor = (int)n;
        PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey");
        return true;
    }
    if (wParam == VK_DELETE) {
        if ((size_t)ed->cursor < n) {
            memmove(ed->text + ed->cursor, ed->text + ed->cursor + 1, n - (size_t)ed->cursor);
        }
        PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey");
        return true;
    }
    if (wParam == VK_RETURN) {
        if (ed->on_confirm) ed->on_confirm(ed->text, ed->user_data);
        ed->confirmed = true;
        FlintContext_Get()->window.selection_complete = true;
        PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey");
        return true;
    }
    if (wParam == 'V' && ctrl_pressed) {
        char *clip = Platform_GetClipboardEntry();
        if (!clip) { PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey"); return true; }
        clip[strcspn(clip, "\r\n")] = '\0';
        if (clip[0] == '\0') { free(clip); PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey"); return true; }
        size_t clip_len = strlen(clip);
        size_t avail = cap > n + 1 ? cap - n - 1 : 0;
        if (clip_len > avail) clip_len = avail;
        if (clip_len > 0) {
            memmove(ed->text + ed->cursor + clip_len, ed->text + ed->cursor, n - (size_t)ed->cursor + 1);
            memcpy(ed->text + ed->cursor, clip, clip_len);
            ed->cursor += (int)clip_len;
        }
        free(clip);
        PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey");
        return true;
    }
    if (ctrl_pressed) {
        if (wParam == VK_OEM_MINUS) { Platform_AdjustFontSize(-1); Config_Save(); PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey"); return true; }
        if (wParam == VK_OEM_PLUS) { Platform_AdjustFontSize(1); Config_Save(); PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey"); return true; }
    }

    if (IsInsertMode() && key->text_len > 0) {
        bool swallow = false;
        if (!swallow && (key->ctrl || key->alt)) swallow = true;
        if (!swallow) {
            bool is_printable = true;
            for (int i = 0; i < key->text_len; i++) { unsigned char c = (unsigned char)key->text[i]; if (c < 0x20 || c == 0x7F) { is_printable = false; break; } }
            if (is_printable) {
                size_t cap2 = ed->text_capacity > 0 ? (size_t)ed->text_capacity : 1024;
                size_t m = strlen(ed->text);
                if (ed->cursor < 0) ed->cursor = 0;
                if ((size_t)ed->cursor > m) ed->cursor = (int)m;
                if (m + (size_t)key->text_len < cap2 - 1) {
                    memmove(ed->text + ed->cursor + key->text_len, ed->text + ed->cursor, m - (size_t)ed->cursor + 1);
                    memcpy(ed->text + ed->cursor, key->text, key->text_len);
                    ed->cursor += key->text_len;
                }
                PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey");
                return true;
            }
        }
    }

    PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey");
    return false;
}
