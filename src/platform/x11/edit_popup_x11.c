#include "core/context.h"
#include "core/modal.h"
#include "bindings/vim_state.h"
#include "core/window_manager.h"
#include "wm/wm.h"
#include "platform/platform.h"
#include "config.h"
#include "core/prompt_geometry.h"
#include "platform/draw.h"
#include "platform/wm_platform.h"
#include "platform/input.h"
#include "platform/x11/input_x11.h"
#include "core/prompt_controller.h"
#include "platform/x11/window_factory_x11.h"
#include "platform/x11/drawing_x11.h"
#include "helpers/helpers_ui.h"
#include "core/timer.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

bool Platform_EditPopupCreateWindow(Popup* self, int x, int y, int w, int h) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    WindowManager* wm = (WindowManager*)self->manager;
    Display* dpy = wm ? wm->display : NULL;
    ((WMEditPopupData*)self)->base.handle.xid = x11_create_edit_popup_window(dpy, x, y, w, h);
    PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupCreateWindow");
    { PROFILE_END_VAR(_pt, "Platform_EditPopupCreateWindow"); return true; }
}

void Platform_EditPopupShowWindow(Popup* self) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    Display* dpy = ((WindowManager*)self->manager)->display;
    x11_draw_background(dpy, self->handle.xid, ((WMEditPopupData*)self)->w, ((WMEditPopupData*)self)->h);
    XMapWindow(dpy, self->handle.xid);
    XFlush(dpy);
    XSync(dpy, False);
    {
        int (*old)(Display*, XErrorEvent*) = XSetErrorHandler(x11_error_handler);
        XSetInputFocus(dpy, self->handle.xid, RevertToParent, CurrentTime);
        XSync(dpy, False);
        XSetErrorHandler(old);
        for (int _f = 0; _f < 2; _f++) {
            Window _fw = None;
            int _rev = 0;
            XGetInputFocus(dpy, &_fw, &_rev);
            if (_fw == self->handle.xid) break;
            old = XSetErrorHandler(x11_error_handler);
            XSetInputFocus(dpy, self->handle.xid, RevertToParent, CurrentTime);
            XSync(dpy, False);
            XSetErrorHandler(old);
        }
        char* _nm = NULL;
        if (XFetchName(dpy, self->handle.xid, &_nm) && _nm) {
            if (!x11_is_own_window_name(_nm)) {
                Wm_FocusByTitle(_nm);
            }
            XFree(_nm);
        }
    }
    XFlush(dpy);
    PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupShowWindow");
}

void Platform_EditPopupHideWindow(Popup* self) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    Display* dpy = get_display();
    if (dpy) { XUnmapWindow(dpy, self->handle.xid); XFlush(dpy); }
    WindowManager* wm = WindowManager_Get();
    if (wm) {
        Popup* top = WindowManager_GetTopPopup(wm);
        if (top) {
            Display* d2 = get_display();
            if (d2) {
                XSetInputFocus(d2, top->handle.xid, RevertToParent, CurrentTime);
                XFlush(d2);
                if (top->vtable && top->vtable->on_render) top->vtable->on_render(top);
                XFlush(d2);
            }
        }
    }
    PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupHideWindow");
}

void Platform_EditPopupRender(Popup* self) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    WMEditPopupData* data = (WMEditPopupData*)self;
    Display* dpy = get_display();
    if (!dpy) { PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupRender"); return; }
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(dpy, self->handle.xid, &attrs)) { PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupRender"); return; }
    Visual* vis = attrs.visual;
    int depth = attrs.depth;
    Colormap cmap = attrs.colormap;
    Pixmap dbl = XCreatePixmap(dpy, self->handle.xid, data->w, data->h, depth);
    XftDraw* xd = dbl ? XftDrawCreate(dpy, dbl, vis, cmap) : NULL;
    bool buffered = (dbl && xd);
    if (buffered) {
        x11_set_dbl_visual(dpy, vis, cmap, depth);
        Pixmap bg = x11_get_cached_background(dpy, self->handle.xid, data->w, data->h);
        if (bg) {
            GC gc = XCreateGC(dpy, dbl, 0, NULL);
            XCopyArea(dpy, bg, dbl, gc, 0, 0, data->w, data->h, 0, 0);
            XFreeGC(dpy, gc);
        } else {
            x11_draw_background_to_drawable(dpy, dbl, data->w, data->h);
        }
    } else {
        if (dbl) { XFreePixmap(dpy, dbl); dbl = None; }
        x11_draw_background(dpy, self->handle.xid, data->w, data->h);
        xd = x11_create_xft_draw(dpy, self->handle.xid);
        if (!xd) { PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupRender"); return; }
    }
    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    XftFont* xf = FH_TO_XFT(font);
    Platform_SetDrawContext(dpy, xd, vis, cmap);
    if (xf && VimModeText) {
        Platform_DrawTextRaw(PROMPT_EDITPOPUP_MODE_X, PROMPT_EDITPOPUP_MODE_Y, g_vim_mode, VimModeTextColor(), font, "LEFT", PROMPT_EDITPOPUP_MODE_PAD, FontSize);
    }
    int cursor_x = 0;
    int text_len = data->text ? strlen(data->text) : 0;
    int scroll = 0;
    int text_w = 0;
    if (xf) {
        scroll = x11_update_edit_scroll_offset(dpy, xf, data->text ? data->text : "", text_len, data->cursor, data->w, 0, &cursor_x);
        if (text_len > 0) {
            XGlyphInfo textExt;
            XftTextExtentsUtf8(dpy, xf, (FcChar8*)data->text, text_len, &textExt);
            text_w = textExt.xOff;
        }
    }
    int draw_x = PROMPT_EDITPOPUP_INSET - scroll;
    int cursor_draw_x = PROMPT_EDITPOPUP_INSET + cursor_x - scroll;
    int draw_y = data->h / 2 + PROMPT_EDITPOPUP_CENTER_OFFSET_Y;
    if (text_w > 0 && text_w <= data->w - 20 && data->align) {
        if (Platform_StringContainsCaseInsensitive(data->align, "CENTER")) {
            draw_x = (data->w - text_w) / 2;
            cursor_draw_x = draw_x + cursor_x;
        } else if (Platform_StringContainsCaseInsensitive(data->align, "RIGHT")) {
            draw_x = data->w - PROMPT_EDITPOPUP_INSET - text_w;
            cursor_draw_x = draw_x + cursor_x;
        }
    }

    if (!IsInsertMode() && data->cursor >= 0) {
        int tint_y = x11_font_cursor_y_of(font, draw_y);
        unsigned long tint = Color_Lerp(rgb_to_hex(COLOR_BG), rgb_to_hex(Modal_CursorColor()), 0.72f);
        int sel_start = 0, sel_end = 0;
        bool has_sel = VimState_GetSelection(&sel_start, &sel_end) && text_len > 0;
        if (has_sel) {
            if (sel_start < 0) sel_start = 0;
            if (sel_end >= text_len) sel_end = text_len - 1;
            if (sel_start <= sel_end && xf) {
                int sx0 = 0, sx1 = 0;
                sx0 = draw_x;
                if (sel_start > 0) {
                    XGlyphInfo e0;
                    XftTextExtentsUtf8(dpy, xf, (FcChar8*)data->text, sel_start, &e0);
                    sx0 = draw_x + e0.xOff;
                }
                if (sel_end + 1 <= text_len) {
                    XGlyphInfo e1;
                    XftTextExtentsUtf8(dpy, xf, (FcChar8*)data->text, sel_end + 1, &e1);
                    sx1 = draw_x + e1.xOff;
                }
                int cx = cursor_draw_x;
                if (sx0 < PROMPT_CARET_LO_CLAMP) sx0 = PROMPT_CARET_LO_CLAMP;
                if (sx1 > data->w - PROMPT_CARET_HI_CLAMP) sx1 = data->w - PROMPT_CARET_HI_CLAMP;
                if (cx < sx0) cx = sx0;
                if (cx > sx1) cx = sx1;
                int w = sx1 - sx0;
                if (w > 0) Platform_DrawRectRaw(sx0, tint_y, w,FontSize,tint,0,false,true,1,true);
            }
        } else {
            int tint_x = cursor_draw_x;
            if (tint_x < PROMPT_CARET_LO_CLAMP) tint_x = PROMPT_CARET_LO_CLAMP;
            if (tint_x > data->w - PROMPT_CARET_HI_CLAMP) tint_x = data->w - PROMPT_CARET_HI_CLAMP;
            Platform_DrawRectRaw(tint_x, tint_y, PROMPT_EDITPOPUP_CARET_W_NORMAL,FontSize,tint,0,false,true,1,true);
        }
    }

    if (text_len > 0) {
        if (RainbowText) {
            int nchars = 0;
            for (int p = 0; p < text_len; ) {
                unsigned char c = (unsigned char)data->text[p];
                int cl = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
                if (cl < 1) cl = 1;
                p += cl; nchars++;
            }
            int ci = 0;
            size_t pos = 0;
            while (pos < (size_t)text_len) {
                unsigned char c = (unsigned char)data->text[pos];
                int cl = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
                if (cl < 1) cl = 1;
                char buf[8];
                memcpy(buf, data->text + pos, cl);
                buf[cl] = '\\0';
                int x = draw_x + Platform_MeasureTextWidth(data->text, (int)pos, FontSize);
                unsigned long col = rgb_to_hex(GetRainbowColor(nchars > 1 ? (float)ci / (float)(nchars - 1) : 0.5f, 0.55f, GradientBrightness));
                Platform_DrawTextRaw(x, draw_y, buf, col, font, "", 0, 0);
                pos += cl; ci++;
            }
        } else {
            Platform_DrawTextRaw(draw_x, draw_y, data->text, rgb_to_hex(COLOR_TEXT), font, "", 0, 0);
        }
    } else if (data->placeholder && data->placeholder[0]) {
        unsigned long ph = Color_Lerp(rgb_to_hex(COLOR_BG), rgb_to_hex(COLOR_TEXT), 0.45f);
        Platform_DrawTextRaw(draw_x, draw_y, data->placeholder, ph, font, "", 0, 0);
    }
    if (IsInsertMode()) {
        if (cursor_draw_x < PROMPT_CARET_LO_CLAMP) cursor_draw_x = PROMPT_CARET_LO_CLAMP;
        if (cursor_draw_x > data->w - PROMPT_CARET_HI_CLAMP) cursor_draw_x = data->w - PROMPT_CARET_HI_CLAMP;
        int cursor_y = x11_font_cursor_y_of(font, draw_y);
        Platform_DrawRectRaw(cursor_draw_x, cursor_y, PROMPT_CARET_W_INSERT, FontSize,rgb_to_hex(Modal_CursorColor()),0,false,true,1,true);
    }
    Platform_SetDrawContext(NULL, NULL, NULL, 0);
    if (buffered) {
        x11_clear_dbl_visual();
        GC gc = XCreateGC(dpy, self->handle.xid, 0, NULL);
        XCopyArea(dpy, dbl, self->handle.xid, gc, 0, 0, data->w, data->h, 0, 0);
        XFreeGC(dpy, gc);
    }
    XftDrawDestroy(xd);
    if (dbl) XFreePixmap(dpy, dbl);
    Platform_DestroyFont(font);
    XFlush(dpy);
    PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupRender");
}

void Platform_EditPopupDestroyWindow(Popup* self) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    Display* dpy = get_display();
    if (dpy) XDestroyWindow(dpy, self->handle.xid);
    PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupDestroyWindow");
}

bool Platform_EditPopupOnKey(Popup* self, const PopupKeyEvent* key) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    WMEditPopupData* ed = (WMEditPopupData*)self;
    if (!ed || !ed->text) { PROFILE_END_VAR(timer, "edit_popup_x11:Platform_EditPopupOnKey"); return true; }

    int wParam = key->vk;
    bool ctrl_pressed = key->ctrl;
    bool shift_pressed = key->shift;
    bool alt_pressed = key->alt;
    (void)shift_pressed;
    (void)alt_pressed;
    if (PromptController_TryVimMotion(wParam, ctrl_pressed, shift_pressed, alt_pressed)) { PROFILE_END_VAR(timer, "edit_popup_x11:Platform_EditPopupOnKey"); return true; }

    size_t cap = ed->text_capacity > 0 ? (size_t)ed->text_capacity : 1024;
    size_t n = strlen(ed->text);
    if (ed->cursor < 0) ed->cursor = 0;
    if ((size_t)ed->cursor > n) ed->cursor = (int)n;

    if (wParam == VK_BACK) {
        if (ctrl_pressed) {
            ed->text[0] = '\0';
            ed->cursor = 0;
        } else if (ed->cursor > 0) {
            size_t ecur = (size_t)ed->cursor;
            if (!EditBufferBackspacePair(ed->text, &ecur)) {
                size_t del = ecur - 1;
                memmove(ed->text + del, ed->text + del + 1, n - del);
                ecur--;
            }
            ed->cursor = (int)ecur;
        }
        PROFILE_END_VAR(timer, "edit_popup_x11:Platform_EditPopupOnKey");
        return true;
    }
    if (wParam == VK_ESCAPE) {
        if (IsInsertMode()) {
            SetMode(MODE_NORMAL);
        } else {
            FlintContext_Get()->window.selection_complete = true;
        }
        PROFILE_END_VAR(timer, "edit_popup_x11:Platform_EditPopupOnKey");
        return true;
    }
    if (wParam == VK_LEFT) {
        if (ed->cursor > 0) ed->cursor--;
        PROFILE_END_VAR(timer, "edit_popup_x11:Platform_EditPopupOnKey");
        return true;
    }
    if (wParam == VK_RIGHT) {
        if ((size_t)ed->cursor < n) ed->cursor++;
        PROFILE_END_VAR(timer, "edit_popup_x11:Platform_EditPopupOnKey");
        return true;
    }
    if (wParam == VK_HOME) {
        ed->cursor = 0;
        PROFILE_END_VAR(timer, "edit_popup_x11:Platform_EditPopupOnKey");
        return true;
    }
    if (wParam == VK_END) {
        ed->cursor = (int)n;
        PROFILE_END_VAR(timer, "edit_popup_x11:Platform_EditPopupOnKey");
        return true;
    }
    if (wParam == VK_DELETE) {
        if ((size_t)ed->cursor < n) {
            memmove(ed->text + ed->cursor, ed->text + ed->cursor + 1, n - (size_t)ed->cursor);
        }
        PROFILE_END_VAR(timer, "edit_popup_x11:Platform_EditPopupOnKey");
        return true;
    }
    if (wParam == VK_RETURN) {
        if (ed->on_confirm) ed->on_confirm(ed->text, ed->user_data);
        ed->confirmed = true;
        FlintContext_Get()->window.selection_complete = true;
        PROFILE_END_VAR(timer, "edit_popup_x11:Platform_EditPopupOnKey");
        return true;
    }
    if (wParam == 'V' && ctrl_pressed) {
        char *clip = Platform_GetClipboardEntry();
        clip[strcspn(clip, "\r\n")] = '\0';
        if (clip[0] == '\0') { free(clip); PROFILE_END_VAR(timer, "edit_popup_x11:Platform_EditPopupOnKey"); return true; }
        size_t clip_len = strlen(clip);
        size_t avail = cap > n + 1 ? cap - n - 1 : 0;
        if (clip_len > avail) clip_len = avail;
        if (clip_len > 0) {
            memmove(ed->text + ed->cursor + clip_len, ed->text + ed->cursor, n - (size_t)ed->cursor + 1);
            memcpy(ed->text + ed->cursor, clip, clip_len);
            ed->cursor += (int)clip_len;
        }
        free(clip);
        PROFILE_END_VAR(timer, "edit_popup_x11:Platform_EditPopupOnKey");
        return true;
    }
    if (ctrl_pressed) {
        if (wParam == VK_OEM_MINUS) Platform_AdjustFontSize(-1) ; Config_Save(); PROFILE_END_VAR(timer, "edit_popup_x11:Platform_EditPopupOnKey"); return true; 
        if (wParam == VK_OEM_PLUS) Platform_AdjustFontSize(1) ; Config_Save(); PROFILE_END_VAR(timer, "edit_popup_x11:Platform_EditPopupOnKey"); return true; 
        if (wParam == VK_OEM_MINUS || wParam == VK_OEM_PLUS) { Config_Save(); PROFILE_END_VAR(timer, "edit_popup_x11:Platform_EditPopupOnKey"); return true; }
    }

    if (IsInsertMode() && key->text_len > 0) {
        bool swallow = false;
        bool trigger_press = false;
        if (pending_hotkey_vk != 0 && key->text_len == 1) {
            struct timeval tv; gettimeofday(&tv, NULL);
            long now = tv.tv_sec * 1000 + tv.tv_usec / 1000;
            if (now - pending_hotkey_timeMs < 800) {
                unsigned char c = (unsigned char)key->text[0];
                int vkc = 0;
                if (c >= 'a' && c <= 'z') vkc = c - 'a' + 'A';
                else if (c >= 'A' && c <= 'Z') vkc = c;
                if (vkc != 0 && vkc == pending_hotkey_vk) trigger_press = true;
            }
            pending_hotkey_vk = 0;
            pending_hotkey_mod = 0;
        }
        if (!trigger_press && !swallow && (key->ctrl || key->alt)) swallow = true;
        if (!swallow) {
            bool is_printable = true;
            for (int i = 0; i < key->text_len; i++) { unsigned char c = (unsigned char)key->text[i]; if (c < 0x20 || c == 0x7F) { is_printable = false; break; } }
            if (is_printable) {
                size_t cap2 = ed->text_capacity > 0 ? (size_t)ed->text_capacity : 1024;
                size_t ecur = (size_t)(ed->cursor < 0 ? 0 : ed->cursor);
                EditBufferInsertText(ed->text, cap2, &ecur, key->text, (size_t)key->text_len);
                ed->cursor = (int)ecur;
                PROFILE_END_VAR(timer, "edit_popup_x11:Platform_EditPopupOnKey");
                return true;
            }
        }
    }

    PROFILE_END_VAR(timer, "edit_popup_x11:Platform_EditPopupOnKey");
    { PROFILE_END_VAR(_pt, "Platform_EditPopupOnKey"); return false; }
}
