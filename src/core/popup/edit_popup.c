#include "core/context.h"
#include "core/modal.h"
#include "core/window_manager.h"
#include "core/platform.h"
#include "platform/ui.h"
#include "core/modal.h"
#include "core/feature_data.h"
#include "core/vim.h"
#include "bindings/vim_helpers.h"
#include "bindings/vim_motions.h"
#include "bindings/bindings.h"
#include "core/prompt_controller.h"
#include "config.h"
#include "core/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_flag.h"
#include "helpers/helpers_file.h"
#include "helpers/helpers_ui.h"
#include "config.h"
#include "core/feature_loader.h"
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#if !defined(_WIN32)
#include <pthread.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

#if !defined(_WIN32)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <unistd.h>
#include <stdatomic.h>
#include "platform/x11/display_x11.h"
#include "platform/x11/drawing_x11.h"
#include "platform/x11/window_factory_x11.h"
#include "platform/draw.h"
extern int WindowManager_RunLinux(WindowManager* wm);
extern void WindowManager_ProcessEventsLinux(WindowManager* wm);
extern void WindowManager_QuitLinux(WindowManager* wm);
extern void Linux_ProcessXEvent(Display* dpy, XEvent* ev, WindowManager* wm);
extern void EditBufferBackspace(char* buf, size_t* cursor);
extern bool EditBufferDeleteChar(char* buf, size_t* cursor);
#else
#include <windows.h>
#include "core/timer.h"
extern int WindowManager_RunWin32(WindowManager* wm);
extern void WindowManager_ProcessEventsWin32(WindowManager* wm);
extern void WindowManager_QuitWin32(WindowManager* wm);
#endif
typedef WMEditPopupData EditPopupDataInternal;

static void EditPopup_OnShow(Popup* self) {
    PROFILE_START(timer);
    EditPopupDataInternal* data = (EditPopupDataInternal*)self;
    data->saved_mode = g_current_mode;
    SetMode(MODE_INSERT);
    #if !defined(_WIN32)
    Display* dpy = ((WindowManager*)self->manager)->display;
    XMapWindow(dpy, self->handle.xid);
    XFlush(dpy);
    XSync(dpy, False);
    {
        int (*old)(Display*, XErrorEvent*) = XSetErrorHandler(x11_error_handler);
        XSetInputFocus(dpy, self->handle.xid, RevertToParent, CurrentTime);
        XSync(dpy, False);
        XSetErrorHandler(old);
    }
    XFlush(dpy);
    #else
    HWND hwnd = WH_TO_HWND(self->handle);
    if (hwnd && IsWindow(hwnd)) {
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);
    }
    #endif
    PROFILE_END_VAR(timer, "window_manager:EditPopup_OnShow");
}

static void EditPopup_OnHide(Popup* self) {
    PROFILE_START(timer);
    EditPopupDataInternal* data = (EditPopupDataInternal*)self;
    SetMode(data->saved_mode);
    #if !defined(_WIN32)
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
    #else
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
    #endif
    PROFILE_END_VAR(timer, "window_manager:EditPopup_OnHide");
}

static void EditPopup_OnKey(Popup* self, int vk_code, bool pressed, int modifiers) {
    PROFILE_START(timer);
    if (!pressed) { PROFILE_END_VAR(timer, "window_manager:EditPopup_OnKey"); return; }
    
    EditPopupDataInternal* data = (EditPopupDataInternal*)self;
    int vk_upper = (vk_code >= 'a' && vk_code <= 'z') ? vk_code - 32 : vk_code;
    bool ctrl = modifiers & HOTKEY_MOD_CTRL;
    bool shift = modifiers & HOTKEY_MOD_SHIFT;
    bool alt = modifiers & HOTKEY_MOD_ALT;
    if (PromptController_ShouldSwallowPending(vk_upper, modifiers)) { PROFILE_END_VAR(timer, "window_manager:EditPopup_OnKey"); return; }
    if (PromptController_TryVimMotion(vk_upper, ctrl, shift, alt)) {
        if (FlintContext_Get()->window.selection_complete) { data->confirmed=false; WindowManager_PopPopup(self->manager); FlintContext_Get()->window.selection_complete=false; }
        PROFILE_END_VAR(timer, "window_manager:EditPopup_OnKey");
        return;
    }
    if (vk_code == VK_ESCAPE) {
        if (IsInsertMode()) {
            SetMode(MODE_NORMAL);
            if (self->vtable && self->vtable->on_render) self->vtable->on_render(self);
        } else {
            data->confirmed = false;
            WindowManager_PopPopup(self->manager);
        }
        PROFILE_END_VAR(timer, "window_manager:EditPopup_OnKey");
        return;
    }
    
    if (vk_code == VK_RETURN && !(modifiers & HOTKEY_MOD_SHIFT)) {
        data->confirmed = true;
        if (data->on_confirm) {
            data->on_confirm(data->text, data->user_data);
        }
        WindowManager_PopPopup(self->manager);
        PROFILE_END_VAR(timer, "window_manager:EditPopup_OnKey");
        return;
    }
    
    if (vk_code == VK_BACK) {
        if (data->cursor > 0) {
            memmove(data->text + data->cursor - 1, data->text + data->cursor, 
                    strlen(data->text) - data->cursor + 1);
            data->cursor--;
        }
    } else if (vk_code == VK_DELETE) {
        if (data->cursor < (int)strlen(data->text)) {
            memmove(data->text + data->cursor, data->text + data->cursor + 1,
                    strlen(data->text) - data->cursor);
        }
    } else if (vk_code == VK_LEFT) {
        if (data->cursor > 0) data->cursor--;
    } else if (vk_code == VK_RIGHT) {
        if (data->cursor < (int)strlen(data->text)) data->cursor++;
    } else if (vk_code == VK_HOME) {
        data->cursor = 0;
    } else if (vk_code == VK_END) {
        data->cursor = strlen(data->text);
    } else if (vk_code >= 32 && vk_code <= 126) {
        if (!IsInsertMode()) { PROFILE_END_VAR(timer, "window_manager:EditPopup_OnKey"); return; }
        if (data->cursor < data->text_capacity - 1) {
            memmove(data->text + data->cursor + 1, data->text + data->cursor,
                    strlen(data->text) - data->cursor + 1);
            data->text[data->cursor] = (char)vk_code;
            data->cursor++;
        }
    }
    
    if (self->vtable && self->vtable->on_render) {
        self->vtable->on_render(self);
    }
    PROFILE_END_VAR(timer, "window_manager:EditPopup_OnKey");
}

static void EditPopup_OnRender(Popup* self) {
    PROFILE_START(timer);
    EditPopupDataInternal* data = (EditPopupDataInternal*)self;
#if !defined(_WIN32)
    Display* dpy = get_display();
    if (!dpy) { PROFILE_END_VAR(timer, "window_manager:EditPopup_OnRender"); return; }
    XftDraw* xd = Linux_CreateXftDraw(dpy, self->handle.xid);
    if (!xd) { PROFILE_END_VAR(timer, "window_manager:EditPopup_OnRender"); return; }
    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    XftFont* xf = FH_TO_XFT(font);
    int screen = DefaultScreen(dpy);
    Visual* visual = DefaultVisual(dpy, screen);
    Colormap cmap = DefaultColormap(dpy, screen);
    XftDrawDestroy(xd);
    Linux_DrawBackground(dpy, self->handle.xid, data->w, data->h);
    xd = Linux_CreateXftDraw(dpy, self->handle.xid);
    Platform_SetDrawContext(dpy, xd, visual, cmap);
    if (xf && VimModeText ) {
        Platform_DrawTextRaw(5, 5, g_vim_mode, VimModeTextColor(), font, "LEFT", 5,FontSize);
    }
    if (data->title && xf) {
        XGlyphInfo ext; XftTextExtentsUtf8(dpy, xf, (FcChar8*)data->title, strlen(data->title), &ext);
        int title_x = data->w - ext.xOff - 10;
        if (title_x < 50) title_x = 50;
        Platform_DrawTextRaw(title_x, 5, data->title, rgb_to_hex(COLOR_TEXT), font, "LEFT", title_x,FontSize);
    }
    int cursor_x = 0;
    int text_len = data->text ? strlen(data->text) : 0;
    int scroll = 0;
    if (xf) {
        scroll = Linux_UpdateEditScrollOffset(dpy, xf, data->text ? data->text : "", text_len, data->cursor, data->w, 0, &cursor_x);
    }
    if (text_len > 0) {
        int draw_x = 10 - scroll;
        Platform_DrawTextRaw(draw_x, 40, data->text, rgb_to_hex(COLOR_TEXT), font, "", 0, 0);
    }
    {
        int cursor_draw_x = 10 + cursor_x - scroll;
        if (cursor_draw_x < 2) cursor_draw_x = 2;
        if (cursor_draw_x > data->w - 4) cursor_draw_x = data->w - 4;
        if (IsInsertMode()) {
            Platform_DrawRectRaw(cursor_draw_x, 40, 2, FontSize, rgb_to_hex(COLOR_TEXT));
        } else {
            Platform_DrawRectRaw(cursor_draw_x, 40, 12, FontSize, rgb_to_hex(COLOR_TEXT));
        }
    }
    XftDrawDestroy(xd);
    Platform_DestroyFont(font);
    XFlush(dpy);
#endif
    PROFILE_END_VAR(timer, "window_manager:EditPopup_OnRender");
}

static void EditPopup_Destroy(Popup* self) {
    PROFILE_START(timer);
    EditPopupDataInternal* data = (EditPopupDataInternal*)self;
    free(data->text);
    free(data->title);
    #if !defined(_WIN32)
    Display* dpy = get_display();
    if (dpy) XDestroyWindow(dpy, self->handle.xid);
    #else
    HWND hwnd = WH_TO_HWND(self->handle);
    if (hwnd && IsWindow(hwnd)) DestroyWindow(hwnd);
    #endif
    free(data);
    PROFILE_END_VAR(timer, "window_manager:EditPopup_Destroy");
}

static const PopupVTable g_edit_popup_vtable = {
    .on_show = EditPopup_OnShow,
    .on_hide = EditPopup_OnHide,
    .on_key = EditPopup_OnKey,
    .on_render = EditPopup_OnRender,
    .destroy = EditPopup_Destroy
};

Popup* EditPopup_Create(WindowManager* wm, const char* initial_text,
                        const char* title, int x, int y,
                        EditPopupConfirmCallback on_confirm, void* user_data) {
    PROFILE_START(timer);
    EditPopupDataInternal* data = calloc(1, sizeof(EditPopupDataInternal));
    if (!data) { PROFILE_END_VAR(timer, "window_manager:EditPopup_Create"); return NULL; }
    int h = 100;
    int w = 1000;
    WindowManager_GetCenteredPos(w, h, &x, &y);
    
    data->base.vtable = &g_edit_popup_vtable;
    data->base.manager = wm;
    data->base.type = POPUP_EDIT;
    data->title = title ? strdup(title) : strdup("Edit");
    data->text_capacity = 1024;
    data->text = calloc(data->text_capacity, 1);
    if (initial_text) {
        strncpy(data->text, initial_text, data->text_capacity - 1);
        data->cursor = strlen(data->text);
    }
    data->on_confirm = on_confirm;
    data->user_data = user_data;
    data->x = x; data->y = y; data->w = w; data->h = h;
    data->confirmed = false;
    
    #if !defined(_WIN32)
    Display* dpy = wm->display;
    data->base.handle.xid = Linux_CreateEditPopupWindow(dpy, x, y, w, h);
    #else
    HINSTANCE hInst = GetModuleHandle(NULL);
    HWND hwnd = CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, "EditPopupClass", title ? title : "Edit", WS_POPUP, x, y, w, h, NULL, NULL, hInst, data);
    if (!hwnd) {
        free(data->text);
        free(data->title);
        free(data);
        PROFILE_END_VAR(timer, "window_manager:EditPopup_Create");
        return NULL;
    }
    data->base.handle = WH_FROM_HWND(hwnd);
    #endif
    
    PROFILE_END_VAR(timer, "window_manager:EditPopup_Create");
    return (Popup*)data;
}


