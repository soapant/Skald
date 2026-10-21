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
#include "core/editbox.h"
#include "core/prompt_controller.h"
#include "helpers/helpers_ui.h"
#include "deps/log.h"
#include "config.h"
#include <windows.h>
#include <string.h>
#include <stdio.h>

void win32_fill_popup_key_text(PopupKeyEvent* key) {
        PROFILE_START(_pt);
if (!key) { PROFILE_END_VAR(_pt, "win32_fill_popup_key_text"); return ; }
    BYTE state[256];
    memset(state, 0, sizeof(state));
    if (GetKeyState(VK_SHIFT) & 0x8000) { state[VK_SHIFT] = 0x80; state[VK_LSHIFT] = 0x80; state[VK_RSHIFT] = 0x80; }
    if (GetKeyState(VK_CONTROL) & 0x8000) { state[VK_CONTROL] = 0x80; state[VK_LCONTROL] = 0x80; state[VK_RCONTROL] = 0x80; }
    if (GetKeyState(VK_MENU) & 0x8000) { state[VK_MENU] = 0x80; state[VK_LMENU] = 0x80; state[VK_RMENU] = 0x80; }
    UINT sc = MapVirtualKeyA((UINT)key->vk, MAPVK_VK_TO_VSC);
    wchar_t wbuf[8];
    int wlen = ToUnicodeEx((UINT)key->vk, sc, state, wbuf, 8, 0, GetKeyboardLayout(0));
    if (wlen <= 0) { PROFILE_END_VAR(_pt, "win32_fill_popup_key_text"); return ; }
    char out[32];
    int olen = WideCharToMultiByte(CP_UTF8, 0, wbuf, wlen, out, (int)sizeof(out) - 1, NULL, NULL);
    if (olen <= 0) { PROFILE_END_VAR(_pt, "win32_fill_popup_key_text"); return ; }
    out[olen] = '\0';
    key->text_len = olen < (int)sizeof(key->text) ? olen : (int)sizeof(key->text) - 1;
    memcpy(key->text, out, (size_t)key->text_len);
}



bool Platform_EditPopupCreateWindow(Popup* self, int x, int y, int w, int h) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    WMEditPopupData* data = (WMEditPopupData*)self;
    HINSTANCE hInst = GetModuleHandle(NULL);
    HWND hwnd = CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, "EditPopupClass","", WS_POPUP | WS_CLIPCHILDREN, x, y, w, h, NULL, NULL, hInst, data);
    if (!hwnd) { PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupCreateWindow"); return false; }
    win32_apply_rounded_corners(hwnd, w, h);
    SetLayeredWindowAttributes(hwnd, 0, (BYTE)WindowOpacity, LWA_ALPHA);
    data->base.handle = WH_FROM_HWND(hwnd);
    PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupCreateWindow");
    { PROFILE_END_VAR(_pt, "Platform_EditPopupCreateWindow"); return true; }
}

void Platform_EditPopupShowWindow(Popup* self) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    HWND hwnd = WH_TO_HWND(self->handle);
    if (hwnd && IsWindow(hwnd)) {
        ShowWindow(hwnd, SW_SHOWNA);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW | SWP_ASYNCWINDOWPOS);
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        UpdateWindow(hwnd);
    }
    PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupShowWindow");
}

void Platform_EditPopupHideWindow(Popup* self) {
        PROFILE_START(_pt);
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
        PROFILE_START(_pt);
PROFILE_START(timer);
    WMEditPopupData* data = (WMEditPopupData*)self;
    HWND hwnd = WH_TO_HWND(self->handle);
    if (!data || !hwnd || !IsWindow(hwnd)) { PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupRender"); return; }

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rect;
    GetClientRect(hwnd, &rect);

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

    if (EnableShaders && ShaderPreset != 0) win32_draw_background(paint, &rect);
    else { HBRUSH br = CreateSolidBrush(COLOR_BG); FillRect(paint, &rect, br); DeleteObject(br); }

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
    int text_w = 0;
    int ascent = 0;
    int descent = 0;
    if (font.hfont) {
        TEXTMETRIC tm;
        memset(&tm, 0, sizeof(tm));
        GetTextMetrics(paint, &tm);
        ascent = tm.tmAscent;
        descent = tm.tmDescent;
        if (data->cursor > 0) cursor_x = Platform_MeasureTextWidth(data->text ? data->text : "", data->cursor, FontSize);
        if (text_len > 0) text_w = Platform_MeasureTextWidth(data->text, text_len, FontSize);
    }
    int draw_y = h / 2 + PROMPT_EDITPOPUP_CENTER_OFFSET_Y;
    int sel_start = 0, sel_end = 0;
    bool has_sel = false;
    Editbox_ResolveSelection(NULL, text_len, false, &sel_start, &sel_end, &has_sel);

    FlEditboxSpec eb;
    memset(&eb, 0, sizeof(eb));
    eb.text = data->text ? data->text : "";
    eb.text_len = text_len;
    eb.cursor = data->cursor;
    eb.sel_start = sel_start;
    eb.sel_end = sel_end;
    eb.has_selection = has_sel;
    eb.sel_mode = EDITBOX_SEL_INCLUSIVE;
    eb.insert_mode = IsInsertMode();
    eb.x = 0;
    eb.y = draw_y;
    eb.w = w;
    eb.inset = PROMPT_EDITPOPUP_INSET;
    eb.visible_w = w - PROMPT_EDIT_SCROLL_PAD;
    eb.scroll = 0;
    eb.cursor_x = cursor_x;
    eb.text_w = text_w;
    eb.text_y = draw_y;
    eb.caret_y = Editbox_CaretY(draw_y, ascent, descent);
    eb.caret_h = PromptCaretHeight();
    eb.caret_w_normal = PROMPT_EDITPOPUP_CARET_W_NORMAL;
    eb.caret_w_insert = PROMPT_CARET_W_INSERT;
    eb.align = data->align;
    eb.placeholder_dy = 0;
    eb.rainbow_limit_x = w - PROMPT_EDITPOPUP_INSET;
    eb.placeholder = data->placeholder;
    eb.text_color = rgb_to_hex(COLOR_TEXT);
    eb.font = font;

    Editbox_Render(&eb, NULL);

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
        PROFILE_START(_pt);
PROFILE_START(timer);
    HWND hwnd = WH_TO_HWND(self->handle);
    if (hwnd && IsWindow(hwnd)) DestroyWindow(hwnd);
    PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupDestroyWindow");
}

bool Platform_EditPopupOnKey(Popup* self, const PopupKeyEvent* key) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    WMEditPopupData* ed = (WMEditPopupData*)self;
    if (!ed || !ed->text) { PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey"); return true; }

    int wParam = key->vk;
    bool ctrl_pressed = key->ctrl;
    bool shift_pressed = key->shift;
    bool alt_pressed = key->alt;
    (void)shift_pressed;
    (void)alt_pressed;
    if (ctrl_pressed && (wParam == 'A' || wParam == 'a')) {
        int len = (int)strlen(ed->text);
        VimState_BeginSelection(0);
        VimState_UpdateSelection(len);
        ed->cursor = len;
        PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey"); return true;
    }
    if (ctrl_pressed && (wParam == 'C' || wParam == 'c')) {
        int ss=0, se=0;
        if (VimState_GetSelection(&ss, &se)) {
            if (se < ss) { int t=ss; ss=se; se=t; }
            int len=(int)strlen(ed->text);
            if (ss<0) ss=0; if (se>len) se=len;
            if (se>ss) {
                char buf[1024];
                int n=se-ss; if (n>=(int)sizeof(buf)) n=(int)sizeof(buf)-1;
                memcpy(buf, ed->text+ss, n); buf[n]='\0';
                Platform_CopyToClipboard(buf);
            }
        }
        PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey"); return true;
    }
    if (PromptController_TryVimMotion(wParam, ctrl_pressed, shift_pressed, alt_pressed)) { PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey"); return true; }

    size_t cap = ed->text_capacity > 0 ? (size_t)ed->text_capacity : 1024;
    size_t n = strlen(ed->text);
    if (ed->cursor < 0) ed->cursor = 0;
    if ((size_t)ed->cursor > n) ed->cursor = (int)n;

    if (wParam == VK_BACK) {
        int ss=0, se=0;
        if (VimState_GetSelection(&ss, &se)) {
            if (se < ss) { int t=ss; ss=se; se=t; }
            size_t nn = strlen(ed->text);
            if (ss<0) ss=0; if ((size_t)se>nn) se=(int)nn;
            if (se>ss) {
                memmove(ed->text+ss, ed->text+se, nn-se+1);
                ed->cursor = ss;
                VimState_EndSelection();
                PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey"); return true;
            }
            VimState_EndSelection();
        }
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
        if (EnableVimMode && IsInsertMode()) {
            SetMode(MODE_NORMAL);
        } else {
            FlintContext_Get()->window.selection_complete = true;
        }
        PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey");
        return true;
    }
    if (wParam == VK_LEFT) {
        VimState_EndSelection();
        if (ed->cursor > 0) ed->cursor--;
        PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey");
        return true;
    }
    if (wParam == VK_RIGHT) {
        VimState_EndSelection();
        if ((size_t)ed->cursor < n) ed->cursor++;
        PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey");
        return true;
    }
    if (wParam == VK_HOME) {
        VimState_EndSelection();
        ed->cursor = 0;
        PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey");
        return true;
    }
    if (wParam == VK_END) {
        VimState_EndSelection();
        ed->cursor = (int)n;
        PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey");
        return true;
    }
    if (wParam == VK_DELETE) {
        int ss=0, se=0;
        if (VimState_GetSelection(&ss, &se)) {
            if (se < ss) { int t=ss; ss=se; se=t; }
            size_t nn = strlen(ed->text);
            if (ss<0) ss=0; if ((size_t)se>nn) se=(int)nn;
            if (se>ss) {
                memmove(ed->text+ss, ed->text+se, nn-se+1);
                ed->cursor = ss;
                VimState_EndSelection();
                PROFILE_END_VAR(timer, "edit_popup_win32:Platform_EditPopupOnKey"); return true;
            }
            VimState_EndSelection();
        }
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
                int ss=0, se=0;
                if (VimState_GetSelection(&ss, &se)) {
                    if (se < ss) { int t=ss; ss=se; se=t; }
                    size_t nn = strlen(ed->text);
                    if (ss<0) ss=0; if ((size_t)se>nn) se=(int)nn;
                    if (se>ss) {
                        memmove(ed->text+ss, ed->text+se, nn-se+1);
                        ed->cursor = ss;
                    }
                    VimState_EndSelection();
                }
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
    { PROFILE_END_VAR(_pt, "Platform_EditPopupOnKey"); return false; }
}
