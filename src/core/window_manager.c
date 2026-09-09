#include "core/context.h"
#include "core/modal.h"
#include "core/window_manager.h"
#include "core/platform.h"
#include "core/common_ui.h"
#include "core/modal.h"
#include "core/feature_data.h"
#include "core/vim.h"
#include "bindings/vim_helpers.h"
#include "bindings/vim_motions.h"
#include "bindings/bindings.h"
#include "core/number_shortcut.h"
#include "core/prompt_controller.h"
#include "helpers.h"
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
#include "platform/x11/display.h"
#include "platform/x11/drawing.h"
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

static WindowManager* g_window_manager = NULL;

#if defined(_WIN32)
// Stubs for Linux-only symbols referenced by generic code but not available on Windows
int pending_hotkey_vk = 0;
int pending_hotkey_mod = 0;
long pending_hotkey_timeMs = 0;
bool g_linux_ctrl_held = false;
bool g_linux_alt_held = false;
bool g_linux_shift_held = false;
bool g_Linux_CtrlHeld = false;
bool g_Linux_AltHeld = false;
bool g_Linux_ShiftHeld = false;
void SavePerFeatureState(const char* feature, EditorMode mode, int last_index) {
    WinTimer timer = TimerStart();
    (void)feature; (void)mode; (void)last_index;
    TimerEnd(timer, "window_manager:SavePerFeatureState");
}
#endif

static void on_c_file_changed(void) {
    printf("A .c file was modified!\n");
    fflush(stdout);
}

#if !defined(_WIN32)
static void* feature_watcher_thread(void* arg) {
    (void)arg;
    while (1) {
        char* filename = Platform_WatchForCFileChanges(FeaturePath, on_c_file_changed);
        if (filename) {
            printf("Changed file: %s\n", filename);
            fflush(stdout);
            char* base = strrchr(filename, '/');
            if (!base) base = strrchr(filename, '\\');
            if (base) base++; else base = filename;
            char* dot = strrchr(base, '.');
            if (dot) *dot = '\0';
            char cmd[1024];
            snprintf(cmd, sizeof(cmd), "gcc -I include -I/usr/include/freetype2 -w -g -fPIC -shared -o build/features/%s.so src/features/%s.c 2>&1", base, base);
            printf("[WATCHER] recompiling %s -> %s\n", filename, cmd); fflush(stdout);
            int ret = system(cmd);
            if (ret == 0) {
                printf("[WATCHER] reload %s\n", base); fflush(stdout);
                FeatureLoader_Reload(base);
            } else {
                printf("[WATCHER] compile failed for %s (%d)\n", base, ret); fflush(stdout);
            }
            free(filename);
        }
        usleep(500000);
    }
    return NULL;
}
#else
static DWORD WINAPI feature_watcher_thread_win(LPVOID arg) {
    (void)arg;
    while (1) {
        char* filename = Platform_WatchForCFileChanges(FeaturePath, on_c_file_changed);
        if (filename) {
            printf("Changed file: %s\n", filename);
            fflush(stdout);
            char* base = strrchr(filename, '/');
            if (!base) base = strrchr(filename, '\\');
            if (base) base++; else base = filename;
            char* dot = strrchr(base, '.');
            if (dot) *dot = '\0';
            // Windows cross-compile from Linux watcher not needed at runtime; just reload .dll if already built
            FeatureLoader_Reload(base);
            free(filename);
        }
        Sleep(500);
    }
    return 0;
}
#endif

WindowManager* WindowManager_Create(void) {
    WinTimer timer = TimerStart();
    WindowManager* wm = calloc(1, sizeof(WindowManager));
    if (!wm) { TimerEnd(timer, "window_manager:WindowManager_Create"); return NULL; }
    wm->popup_capacity = 8;
    wm->popups = calloc(wm->popup_capacity, sizeof(Popup*));
    if (!wm->popups) {
        free(wm);
        TimerEnd(timer, "window_manager:WindowManager_Create");
        return NULL;
    }
    
    #if !defined(_WIN32)
    wm->display = get_display();
    if (!wm->display) {
        free(wm->popups);
        free(wm);
        TimerEnd(timer, "window_manager:WindowManager_Create");
        return NULL;
    }
    #endif
    
    wm->ctx = FlintContext_Get();
    g_window_manager = wm;
#if !defined(_WIN32)
    pthread_t tid;
    pthread_create(&tid, NULL, feature_watcher_thread, NULL);
    pthread_detach(tid);
#else
    CreateThread(NULL, 0, feature_watcher_thread_win, NULL, 0, NULL);
#endif
    TimerEnd(timer, "window_manager:WindowManager_Create");
    return wm;
}

void WindowManager_Destroy(WindowManager* wm) {
    WinTimer timer = TimerStart();
    if (!wm) { TimerEnd(timer, "window_manager:WindowManager_Destroy"); return; }
    
    for (int i = 0; i < wm->popup_count; i++) {
        if (wm->popups[i] && wm->popups[i]->vtable && wm->popups[i]->vtable->destroy) {
            wm->popups[i]->vtable->destroy(wm->popups[i]);
        }
    }
    
    free(wm->popups);
    
    #if !defined(_WIN32)
    #endif
    
    if (g_window_manager == wm) g_window_manager = NULL;
    free(wm);
    TimerEnd(timer, "window_manager:WindowManager_Destroy");
}

WindowManager* WindowManager_Get(void) {
    WinTimer timer = TimerStart();
    TimerEnd(timer, "window_manager:WindowManager_Get");
    return g_window_manager;
}

int WindowManager_Run(WindowManager* wm) {
    WinTimer timer = TimerStart();
    if (!wm) { TimerEnd(timer, "window_manager:WindowManager_Run"); return -1; }
    
    #if defined(_WIN32)
    TimerEnd(timer, "window_manager:WindowManager_Run");
    return WindowManager_RunWin32(wm);
    #else
    TimerEnd(timer, "window_manager:WindowManager_Run");
    return WindowManager_RunLinux(wm);
    #endif
    TimerEnd(timer, "window_manager:WindowManager_Run");
}

void WindowManager_Quit(WindowManager* wm) {
    WinTimer timer = TimerStart();
    if (!wm) { TimerEnd(timer, "window_manager:WindowManager_Quit"); return; }
    
    #if defined(_WIN32)
    WindowManager_QuitWin32(wm);
    #else
    WindowManager_QuitLinux(wm);
    #endif
    TimerEnd(timer, "window_manager:WindowManager_Quit");
}

Popup* WindowManager_PushPopup(WindowManager* wm, Popup* popup) {
    WinTimer timer = TimerStart();
    if (!wm || !popup) { TimerEnd(timer, "window_manager:WindowManager_PushPopup"); return NULL; }
    
    if (wm->popup_count >= wm->popup_capacity) {
        int new_cap = wm->popup_capacity * 2;
        Popup** new_popups = realloc(wm->popups, new_cap * sizeof(Popup*));
        if (!new_popups) { TimerEnd(timer, "window_manager:WindowManager_PushPopup"); return NULL; }
        wm->popups = new_popups;
        wm->popup_capacity = new_cap;
    }
    
    popup->manager = wm;
    popup->visible = true;
    wm->popups[wm->popup_count++] = popup;
    
    if (popup->vtable && popup->vtable->on_show) {
        popup->vtable->on_show(popup);
    }
    
    TimerEnd(timer, "window_manager:WindowManager_PushPopup");
    return popup;
}

void WindowManager_PopPopup(WindowManager* wm) {
    if (!wm || wm->popup_count == 0) return;
    
    Popup* popup = wm->popups[--wm->popup_count];
    if (popup->vtable && popup->vtable->on_hide) {
        popup->vtable->on_hide(popup);
    }
    popup->visible = false;
    popup->manager = NULL;
}

Popup* WindowManager_GetTopPopup(WindowManager* wm) {
    if (!wm || wm->popup_count == 0) return NULL;
    return wm->popups[wm->popup_count - 1];
}

void WindowManager_ProcessEvents(WindowManager* wm) {
    #if defined(_WIN32)
    WindowManager_ProcessEventsWin32(wm);
    #else
    WindowManager_ProcessEventsLinux(wm);
    #endif
}

char* WindowManager_ShowPrompt(WindowManager* wm, const PromptSpec* spec) {
    WinTimer timer = TimerStart();
    if (!wm || !spec) { TimerEnd(timer, "window_manager:WindowManager_ShowPrompt"); return NULL; }
#if defined(_WIN32)
    extern char* Platform_PromptUser(const PromptSpec* spec);
    char* res = Platform_PromptUser(spec);
    TimerEnd(timer, "window_manager:WindowManager_ShowPrompt");
    return res;
#else
    Popup* popup = PromptPopup_Create(wm, spec);
    if (!popup) { TimerEnd(timer, "window_manager:WindowManager_ShowPrompt"); return NULL; }
    Display* dpy = wm->display;
    XEvent ev;
    while (wm->running && wm->popup_count > 0) {
        if (XPending(dpy) > 0) {
            XNextEvent(dpy, &ev);
            Linux_ProcessXEvent(dpy, &ev, wm);
        } else {
            usleep(1000);
        }
    }
    if (popup->manager == NULL) {
        if (popup->vtable && popup->vtable->destroy) popup->vtable->destroy(popup);
    } else {
        WindowManager_PopPopup(wm);
        if (popup->vtable && popup->vtable->destroy) popup->vtable->destroy(popup);
    }
    if (dpy) { XFlush(dpy); XSync(dpy, False); }
    FlintContext* _c = FlintContext_Get();
    // sync deprecated global
    TimerEnd(timer, "window_manager:WindowManager_ShowPrompt");
    return _c->window.output_buffer[0] ? _c->window.output_buffer : NULL;
#endif
    TimerEnd(timer, "window_manager:WindowManager_ShowPrompt");
}

char* WindowManager_ShowSimplePrompt(WindowManager* wm, const SimplePromptSpec* spec) {
    WinTimer timer = TimerStart();
    if (!wm || !spec) { TimerEnd(timer, "window_manager:WindowManager_ShowSimplePrompt"); return NULL; }
#if defined(_WIN32)
    extern char* Platform_SimplePromptUser(const SimplePromptSpec* spec);
    char* res = Platform_SimplePromptUser(spec);
    TimerEnd(timer, "window_manager:WindowManager_ShowSimplePrompt");
    return res;
#else
    Popup* popup = SimplePromptPopup_Create(wm, spec);
    if (!popup) { TimerEnd(timer, "window_manager:WindowManager_ShowSimplePrompt"); return NULL; }
    Display* dpy = wm->display;
    XEvent ev;
    while (wm->running && wm->popup_count > 0) {
        if (XPending(dpy) > 0) {
            XNextEvent(dpy, &ev);
            Linux_ProcessXEvent(dpy, &ev, wm);
        } else {
            usleep(1000);
        }
    }
    if (popup->manager == NULL) {
        if (popup->vtable && popup->vtable->destroy) popup->vtable->destroy(popup);
    } else {
        WindowManager_PopPopup(wm);
        if (popup->vtable && popup->vtable->destroy) popup->vtable->destroy(popup);
    }
    if (dpy) { XFlush(dpy); XSync(dpy, False); }
    FlintContext* _c2 = FlintContext_Get();
    TimerEnd(timer, "window_manager:WindowManager_ShowSimplePrompt");
    return _c2->window.output_buffer[0] ? _c2->window.output_buffer : NULL;
#endif
    TimerEnd(timer, "window_manager:WindowManager_ShowSimplePrompt");
}

// ============ Edit Popup Implementation ============

typedef WMEditPopupData EditPopupDataInternal;

static void EditPopup_OnShow(Popup* self) {
    WinTimer timer = TimerStart();
    EditPopupDataInternal* data = (EditPopupDataInternal*)self;
    data->saved_mode = g_current_mode;
    SetMode(MODE_INSERT);
    extern bool g_Linux_CtrlHeld, g_Linux_AltHeld, g_Linux_ShiftHeld;
    extern bool g_linux_ctrl_held, g_linux_alt_held, g_linux_shift_held;
    g_Linux_CtrlHeld = g_Linux_AltHeld = g_Linux_ShiftHeld = false;
    g_linux_ctrl_held = g_linux_alt_held = g_linux_shift_held = false;
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
    #endif
    TimerEnd(timer, "window_manager:EditPopup_OnShow");
}

static void EditPopup_OnHide(Popup* self) {
    WinTimer timer = TimerStart();
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
    #endif
    TimerEnd(timer, "window_manager:EditPopup_OnHide");
}

static void EditPopup_OnKey(Popup* self, int vk_code, bool pressed, int modifiers) {
    WinTimer timer = TimerStart();
    if (!pressed) { TimerEnd(timer, "window_manager:EditPopup_OnKey"); return; }
    
    EditPopupDataInternal* data = (EditPopupDataInternal*)self;
    int vk_upper = (vk_code >= 'a' && vk_code <= 'z') ? vk_code - 32 : vk_code;
    bool ctrl = modifiers & HOTKEY_MOD_CTRL;
    bool shift = modifiers & HOTKEY_MOD_SHIFT;
    bool alt = modifiers & HOTKEY_MOD_ALT;
    if (PromptController_ShouldSwallowPending(vk_upper, modifiers)) { TimerEnd(timer, "window_manager:EditPopup_OnKey"); return; }
    if (PromptController_TryVimMotion(vk_upper, ctrl, shift, alt)) {
        if (FlintContext_Get()->window.selection_complete) { data->confirmed=false; WindowManager_PopPopup(self->manager); FlintContext_Get()->window.selection_complete=false; }
        TimerEnd(timer, "window_manager:EditPopup_OnKey");
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
        TimerEnd(timer, "window_manager:EditPopup_OnKey");
        return;
    }
    
    if (vk_code == VK_RETURN && !(modifiers & HOTKEY_MOD_SHIFT)) {
        data->confirmed = true;
        if (data->on_confirm) {
            data->on_confirm(data->text, data->user_data);
        }
        WindowManager_PopPopup(self->manager);
        TimerEnd(timer, "window_manager:EditPopup_OnKey");
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
    } else if (vk_code >= 32 && vk_code <= 126) { // Printable
        if (!IsInsertMode()) { TimerEnd(timer, "window_manager:EditPopup_OnKey"); return; }
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
    TimerEnd(timer, "window_manager:EditPopup_OnKey");
}

static void EditPopup_OnRender(Popup* self) {
    WinTimer timer = TimerStart();
    EditPopupDataInternal* data = (EditPopupDataInternal*)self;
#if !defined(_WIN32)
    Display* dpy = get_display();
    if (!dpy) { TimerEnd(timer, "window_manager:EditPopup_OnRender"); return; }
    XftDraw* xd = Linux_CreateXftDraw(dpy, self->handle.xid);
    if (!xd) { TimerEnd(timer, "window_manager:EditPopup_OnRender"); return; }
    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    XftFont* xf = FH_TO_XFT(font);
    int screen = DefaultScreen(dpy);
    Visual* visual = DefaultVisual(dpy, screen);
    Colormap cmap = DefaultColormap(dpy, screen);
    XftColor bg, fg, title_col, vim_col;
    Linux_XftAllocColor(dpy, visual, cmap, rgb_to_hex(COLOR_BG), 0xFFFF, &bg);
    Linux_XftAllocColor(dpy, visual, cmap, rgb_to_hex(COLOR_TEXT), 0xFFFF, &fg);
    Linux_XftAllocColor(dpy, visual, cmap, rgb_to_hex(COLOR_TEXT), 0xFFFF, &title_col);
    Linux_XftAllocColor(dpy, visual, cmap, VimModeTextColor(), 0xFFFF, &vim_col);
    // clear - unified background handles rounded + shaders
    XftColorFree(dpy, visual, cmap, &bg);
    XftDrawDestroy(xd);
    Linux_DrawBackground(dpy, self->handle.xid, data->w, data->h);
    xd = Linux_CreateXftDraw(dpy, self->handle.xid);
    Linux_XftAllocColor(dpy, visual, cmap, rgb_to_hex(COLOR_BG), 0xFFFF, &bg);
    if (xf) {
        draw_wrapped_text(dpy, xd, &vim_col, font, g_vim_mode, 5, "LEFT", 5);
    }
    if (data->title && xf) {
        XGlyphInfo ext; XftTextExtentsUtf8(dpy, xf, (FcChar8*)data->title, strlen(data->title), &ext);
        int title_x = data->w - ext.xOff - 10;
        if (title_x < 50) title_x = 50;
        draw_wrapped_text(dpy, xd, &title_col, font, data->title, 5, "LEFT", title_x);
    }
    int cursor_x = 0;
    int text_len = data->text ? strlen(data->text) : 0;
    int scroll = 0;
    if (xf) {
        scroll = Linux_UpdateEditScrollOffset(dpy, xf, data->text ? data->text : "", text_len, data->cursor, data->w, 0, &cursor_x);
    }
    if (xf && text_len > 0) {
        XGlyphInfo ext;
        XftTextExtentsUtf8(dpy, xf, (FcChar8*)data->text, text_len, &ext);
        int draw_x = 10 - scroll;
        int draw_y = 40 + xf->ascent;
        XftDrawStringUtf8(xd, &fg, xf, draw_x, draw_y, (FcChar8*)data->text, text_len);
    }
    if (xf) {
        int cursor_draw_x = 12 + cursor_x - scroll;
        if (cursor_draw_x < 10) cursor_draw_x = 10;
        if (cursor_draw_x > data->w - 10) cursor_draw_x = data->w - 10;
        if (IsInsertMode()) {
            XftDrawRect(xd, &fg, cursor_draw_x, 47, 2, FontSize);
        } else {
            XftDrawRect(xd, &fg, cursor_draw_x, 47, 12, FontSize);
        }
    }
    XftColorFree(dpy, visual, cmap, &bg);
    XftColorFree(dpy, visual, cmap, &fg);
    XftColorFree(dpy, visual, cmap, &title_col);
    XftColorFree(dpy, visual, cmap, &vim_col);
    XftDrawDestroy(xd);
    Platform_DestroyFont(font);
    XFlush(dpy);
#endif
    TimerEnd(timer, "window_manager:EditPopup_OnRender");
}

static void EditPopup_Destroy(Popup* self) {
    WinTimer timer = TimerStart();
    EditPopupDataInternal* data = (EditPopupDataInternal*)self;
    free(data->text);
    free(data->title);
    #if !defined(_WIN32)
    Display* dpy = get_display();
    if (dpy) XDestroyWindow(dpy, self->handle.xid);
    #endif
    free(data);
    TimerEnd(timer, "window_manager:EditPopup_Destroy");
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
    WinTimer timer = TimerStart();
    EditPopupDataInternal* data = calloc(1, sizeof(EditPopupDataInternal));
    if (!data) { TimerEnd(timer, "window_manager:EditPopup_Create"); return NULL; }
    int h = 100;
    int w = 1000;
    
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
    #endif
    
    TimerEnd(timer, "window_manager:EditPopup_Create");
    return (Popup*)data;
}


typedef struct {
    Popup base;
    char* message;
    int timeout_ms;
    uint64_t start_time;
} MessagePopupData;

static void MessagePopup_OnShow(Popup* self) {
    WinTimer timer = TimerStart();
    #if !defined(_WIN32)
    Display* dpy = ((WindowManager*)self->manager)->display;
    XMapWindow(dpy, self->handle.xid);
    XFlush(dpy);
    #endif
    TimerEnd(timer, "window_manager:MessagePopup_OnShow");
}

static void MessagePopup_Destroy(Popup* self) {
    WinTimer timer = TimerStart();
    MessagePopupData* data = (MessagePopupData*)self;
    free(data->message);
    #if !defined(_WIN32)
    Display* dpy = get_display();
    if (dpy) XDestroyWindow(dpy, self->handle.xid);
    #endif
    free(data);
    TimerEnd(timer, "window_manager:MessagePopup_Destroy");
}

static const PopupVTable g_message_popup_vtable = {
    .on_show = MessagePopup_OnShow,
    .on_hide = NULL,
    .on_key = NULL,
    .on_render = NULL,
    .destroy = MessagePopup_Destroy
};

Popup* MessagePopup_Create(WindowManager* wm, const char* message, int timeout_ms) {
    WinTimer timer = TimerStart();
    MessagePopupData* data = calloc(1, sizeof(MessagePopupData));
    if (!data) { TimerEnd(timer, "window_manager:MessagePopup_Create"); return NULL; }
    
    data->base.vtable = &g_message_popup_vtable;
    data->base.manager = wm;
    data->message = strdup(message);
    data->timeout_ms = timeout_ms;
    
    #if !defined(_WIN32)
    Display* dpy = wm->display;
    data->base.handle.xid = Linux_CreateDisplayMessageWindow(dpy, 0, 0, 400, 100);
    #endif
    
    TimerEnd(timer, "window_manager:MessagePopup_Create");
    return WindowManager_PushPopup(wm, (Popup*)data);
}

// ============ Prompt Popup (list-based) ============

typedef struct {
    Popup base;
    PromptSpec spec;
    PromptState* state;
    int x, y, w, h;
} PromptPopupData;

static void PromptPopup_OnShow(Popup* self) {
    WinTimer timer = TimerStart();
    PromptPopupData* data = (PromptPopupData*)self;
    WindowManager* wm = (WindowManager*)self->manager;
    FlintContext* ctx = FlintContext_Get();
    ctx->window.selection_complete = false;
    ctx->window.output_buffer[0] = '\0';
    // deprecated globals sync
    extern bool g_Linux_CtrlHeld, g_Linux_AltHeld, g_Linux_ShiftHeld;
    extern bool g_linux_ctrl_held, g_linux_alt_held, g_linux_shift_held;
    g_Linux_CtrlHeld = g_Linux_AltHeld = g_Linux_ShiftHeld = false;
    g_linux_ctrl_held = g_linux_alt_held = g_linux_shift_held = false;
    
    #if !defined(_WIN32)
    Display* dpy = wm->display;
    data->base.handle.xid = Linux_CreatePromptWindow(dpy, data->x, data->y, data->w, data->h);
    XSelectInput(dpy, data->base.handle.xid, KeyPressMask | ExposureMask | FocusChangeMask);
    XMapWindow(dpy, data->base.handle.xid);
    XFlush(dpy);
    XSync(dpy, False);
    // wait until viewable then focus (retry, hyprland/xwayland needs time)
    for (int _r=0; _r<20; _r++) {
        XWindowAttributes _a;
        if (XGetWindowAttributes(dpy, data->base.handle.xid, &_a) && _a.map_state == IsViewable) break;
        usleep(5000);
        XSync(dpy, False);
    }
    {
        int (*old)(Display*, XErrorEvent*) = XSetErrorHandler(x11_error_handler);
        XSetInputFocus(dpy, data->base.handle.xid, RevertToParent, CurrentTime);
        XSync(dpy, False);
        XSetErrorHandler(old);
    }
    XFlush(dpy);
    #endif
    
    data->state = calloc(1, sizeof(PromptState));
    if (!data->state) { TimerEnd(timer, "window_manager:PromptPopup_OnShow"); return; }
    
    data->state->items = (char**)data->spec.items;
    data->state->item_count = data->spec.count;
    data->state->cursor = 0;
    data->state->width = data->w;
    data->state->height = data->h;
    const char* p = strchr(data->spec.title, ' ');
    data->state->window_title = (char*)(p ? p+1 : data->spec.title);
    data->state->filter[0] = '\0';
    data->state->has_listbox = (data->spec.count >= 0);
    ListBoxInfo_Init(&data->state->listbox);
    data->state->listbox.selected_index = -1;
    extern EditorMode g_last_mode;
    if (ModePersistsThroughFeatures) {
        SetModeWithListBox(g_last_mode, &data->state->listbox);
        if (RememberLastIndex && last_index >= 0 && data->state->has_listbox) {
            data->state->listbox.selected_index = last_index;
        }
    } else {
        SetModeWithListBox(MODE_INSERT, &data->state->listbox);
    }
    self->user_data = data->state;
    
    FlintContext* ctx2 = FlintContext_Get();
    if (!wm->ctx) wm->ctx = ctx2;
    // new canonical storage
    ctx2->prompt_callbacks.refresh = data->spec.refresh;
    ctx2->prompt_callbacks.enter = data->spec.enter;
    ctx2->prompt_callbacks.ctrl_enter = data->spec.ctrl_enter;
    ctx2->prompt_callbacks.tab = data->spec.tab;
    ctx2->prompt_callbacks.copy = data->spec.copy;
    ctx2->prompt_callbacks.render = data->spec.render;
    ctx2->prompt_callbacks.edit = data->spec.edit;
    ctx2->prompt_callbacks.filter = data->spec.filter;
    ctx2->prompt_callbacks.help = data->spec.help;
    // g_alias_callback was alias to prompt_callbacks.alias - keep existing value
    ctx2->window.current_feature = (char*)(p ? p+1 : data->spec.title);
    ctx2->window.title = data->spec.title;
    ctx2->window.callbacks = ctx2->prompt_callbacks;
    // deprecated globals sync for external .so compatibility
    SetCurrentFeatureName(ctx2->window.current_feature);
    if (data->spec.refresh) data->spec.refresh(data->state->filter, FlintContext_Get()->window.hListBox, data->state->has_listbox ? &data->state->listbox : NULL);
    TimerEnd(timer, "window_manager:PromptPopup_OnShow");
}

static void PromptPopup_OnKey(Popup* self, int vk_code, bool pressed, int modifiers) {
    WinTimer timer = TimerStart();
    if (!pressed) { TimerEnd(timer, "window_manager:PromptPopup_OnKey"); return; }
    PromptPopupData* data = (PromptPopupData*)self;
    if (!data->state) { TimerEnd(timer, "window_manager:PromptPopup_OnKey"); return; }
    
    #if !defined(_WIN32)
    bool ctrl_pressed = modifiers & HOTKEY_MOD_CTRL;
    bool shift_pressed = modifiers & HOTKEY_MOD_SHIFT;
    bool alt_pressed = modifiers & HOTKEY_MOD_ALT;
    int vk_upper = (vk_code >= 'a' && vk_code <= 'z') ? vk_code - 32 : vk_code;
    
    if (PromptController_ShouldSwallowPending(vk_upper, modifiers)) { TimerEnd(timer, "window_manager:PromptPopup_OnKey"); return; }
    if (PromptController_TryVimMotion(vk_upper, ctrl_pressed, shift_pressed, alt_pressed)) {
        FlintContext* _c = FlintContext_Get();
        if (_c->window.selection_complete) { WindowManager_PopPopup(self->manager); _c->window.selection_complete = false; }
        TimerEnd(timer, "window_manager:PromptPopup_OnKey");
        return;
    }
    
    if (vk_code == VK_ESCAPE) {
        if (IsInsertMode()) {
            SetMode(MODE_NORMAL);
            if (self->vtable && self->vtable->on_render) self->vtable->on_render(self);
        } else {
            FlintContext_Get()->window.selection_complete = true;
            WindowManager_PopPopup(self->manager);
        }
        TimerEnd(timer, "window_manager:PromptPopup_OnKey");
        return;
    }
    
    if (vk_code == VK_BACK) {
        if (ctrl_pressed) {
            data->state->filter[0] = '\0';
            data->state->cursor = 0;
        } else {
            EditBufferBackspace(data->state->filter, &data->state->cursor);
        }
        if (data->spec.refresh) data->spec.refresh(data->state->filter, FlintContext_Get()->window.hListBox, data->state->has_listbox ? &data->state->listbox : NULL);
        TimerEnd(timer, "window_manager:PromptPopup_OnKey");
        return;
    }
    
    if (vk_code >= '0' && vk_code <= '9') {
        if (ctrl_pressed) {
            HandleNumberShortcut(vk_code, FlintContext_Get()->window.hListBox, FlintContext_Get()->window.g_ActiveEditControl, data->state->has_listbox ? &data->state->listbox : NULL);
            TimerEnd(timer, "window_manager:PromptPopup_OnKey");
            return;
        }
    }
    
    if (vk_code == VK_RIGHT) {
        CursorForward(&data->state->cursor, strlen(data->state->filter));
        TimerEnd(timer, "window_manager:PromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_LEFT) {
        CursorBack(&data->state->cursor, strlen(data->state->filter));
        TimerEnd(timer, "window_manager:PromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_UP) {
        if (ctrl_pressed) {
            VimContext ctx = Vim_FromState(data->state);
            SetCursorToStartOfText(&ctx);
        } else if (data->state->has_listbox) {
            UI_NavigateUp(&data->state->listbox, FlintContext_Get()->window.hListBox);
        }
        TimerEnd(timer, "window_manager:PromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_DOWN) {
        if (ctrl_pressed) {
            VimContext ctx = Vim_FromState(data->state);
            SetCursorToEndOfText(&ctx);
        } else if (data->state->has_listbox) {
            UI_NavigateDown(&data->state->listbox, FlintContext_Get()->window.hListBox);
        }
        TimerEnd(timer, "window_manager:PromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_DELETE) {
        if (EditBufferDeleteChar(data->state->filter, &data->state->cursor)) {
            if (data->spec.refresh) data->spec.refresh(data->state->filter, FlintContext_Get()->window.hListBox, data->state->has_listbox ? &data->state->listbox : NULL);
        }
        TimerEnd(timer, "window_manager:PromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_HOME) {
        VimContext ctx = Vim_FromState(data->state);
        SetCursorToStartOfText(&ctx);
        TimerEnd(timer, "window_manager:PromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_END) {
        VimContext ctx = Vim_FromState(data->state);
        SetCursorToEndOfText(&ctx);
        TimerEnd(timer, "window_manager:PromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_TAB) {
        if (data->spec.tab) data->spec.tab(&data->state->listbox);
        TimerEnd(timer, "window_manager:PromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_RETURN) {
        bool is_ctrl = ctrl_pressed;
        const char* sel = NULL;
        char filter_copy[sizeof(data->state->filter)];
        if (data->state->has_listbox && UI_IsListBoxSelected(&data->state->listbox)) {
            sel = ListBoxInfo_GetSelectedText(&data->state->listbox);
        } else {
            strncpy(filter_copy, data->state->filter, sizeof(filter_copy) - 1);
            filter_copy[sizeof(filter_copy) - 1] = '\0';
            sel = filter_copy;
        }
        bool should_close = false;
        if (is_ctrl) {
            if (data->spec.ctrl_enter) should_close = data->spec.ctrl_enter(sel, data->state->has_listbox ? &data->state->listbox : NULL);
            else should_close = false;
        } else {
            if (data->spec.enter) should_close = data->spec.enter(sel, data->state->has_listbox ? &data->state->listbox : NULL);
            else should_close = true;
        }
        FlintContext_Get()->window.selection_complete = should_close;
        if (should_close) WindowManager_PopPopup(self->manager);
        TimerEnd(timer, "window_manager:PromptPopup_OnKey");
        return;
    }
    
    if (vk_code >= 32 && vk_code <= 126) {
        if (IsInsertMode()) {
            int len = 1;
            char buf[2] = { (char)vk_code, '\0' };
            size_t n = strlen(data->state->filter);
            if (n + len < sizeof(data->state->filter) - 1) {
                if (data->state->cursor < 0) data->state->cursor = 0;
                if ((size_t)data->state->cursor > n) data->state->cursor = (int)n;
                memmove(data->state->filter + data->state->cursor + len,
                        data->state->filter + data->state->cursor,
                        n - data->state->cursor + 1);
                memcpy(data->state->filter + data->state->cursor, buf, len);
                data->state->cursor += len;
                if (data->spec.refresh) data->spec.refresh(data->state->filter, FlintContext_Get()->window.hListBox, data->state->has_listbox ? &data->state->listbox : NULL);
            }
        }
    }
    #endif
    FlintContext* _c2 = FlintContext_Get();
    if (_c2->window.selection_complete) {
        WindowManager_PopPopup(self->manager);
        _c2->window.selection_complete = false;
    }
    TimerEnd(timer, "window_manager:PromptPopup_OnKey");
}

static void PromptPopup_OnHide(Popup* self) {
    WinTimer timer = TimerStart();
    PromptPopupData* data = (PromptPopupData*)self;
    #if !defined(_WIN32)
    Display* dpy = get_display();
    if (dpy) {
        XUnmapWindow(dpy, self->handle.xid);
        XFlush(dpy);
    }
    #endif
    extern EditorMode g_last_mode;
    extern void SavePerFeatureState(const char* feature, EditorMode mode, int last_index);
    if (data->state) {
        if (!KeepSelectionAfterClose) {
            last_index = 0;
            if (data->state->has_listbox) data->state->listbox.selected_index = 0;
        } else {
            last_index = data->state->listbox.selected_index;
        }
        g_last_mode = g_current_mode;
        FlintContext* _c3 = FlintContext_Get();
        const char* feat = _c3->window.current_feature;
        if (ModePersistsThroughFeatures && feat) {
            SavePerFeatureState(feat, g_current_mode, last_index);
        }
    }
    TimerEnd(timer, "window_manager:PromptPopup_OnHide");
}

static void PromptPopup_OnRender(Popup* self) {
    WinTimer timer = TimerStart();
    PromptPopupData* data = (PromptPopupData*)self;
    if (!data->state) { TimerEnd(timer, "window_manager:PromptPopup_OnRender"); return; }
    
    #if !defined(_WIN32)
    Display* dpy = ((WindowManager*)self->manager)->display;
    Linux_RenderPromptUser(dpy, &self->handle.xid, data->state);
    #endif
    TimerEnd(timer, "window_manager:PromptPopup_OnRender");
}

static void PromptPopup_Destroy(Popup* self) {
    WinTimer timer = TimerStart();
    PromptPopupData* data = (PromptPopupData*)self;
    #if !defined(_WIN32)
    Display* dpy = get_display();
    if (dpy) XDestroyWindow(dpy, self->handle.xid);
    #endif
    free(data->state);
    free(data);
    TimerEnd(timer, "window_manager:PromptPopup_Destroy");
}

static const PopupVTable g_prompt_popup_vtable = {
    .on_show = PromptPopup_OnShow,
    .on_hide = PromptPopup_OnHide,
    .on_key = PromptPopup_OnKey,
    .on_render = PromptPopup_OnRender,
    .destroy = PromptPopup_Destroy
};

Popup* PromptPopup_Create(WindowManager* wm, const PromptSpec* spec) {
    WinTimer timer = TimerStart();
    if (!wm || !spec) { TimerEnd(timer, "window_manager:PromptPopup_Create"); return NULL; }
    
    int screen_width = 1920;
    int screen_height = 1080;
    int layout_w = 900;
    int layout_h = 95 + (spec->count > 0 ? (spec->count * spec->item_height) : 0);
    
    if (layout_h < spec->height) layout_h = spec->height;
    if (layout_h > spec->width) layout_h = spec->width;
    
    int x = (screen_width - layout_w) / 2;
    int y = (screen_height - layout_h) / 2;
    
    PromptPopupData* data = calloc(1, sizeof(PromptPopupData));
    if (!data) { TimerEnd(timer, "window_manager:PromptPopup_Create"); return NULL; }
    
    data->base.vtable = &g_prompt_popup_vtable;
    data->base.manager = wm;
    data->base.type = POPUP_PROMPT;
    data->spec = *spec;
    data->x = x; data->y = y; data->w = layout_w; data->h = layout_h;
    
    TimerEnd(timer, "window_manager:PromptPopup_Create");
    return WindowManager_PushPopup(wm, (Popup*)data);
}

// ============ SimplePrompt Popup ============

typedef struct {
    Popup base;
    SimplePromptSpec spec;
    SimplePromptState* state;
    int x, y, w, h;
} SimplePromptPopupData;

static void SimplePromptPopup_OnShow(Popup* self) {
    WinTimer timer = TimerStart();
    SimplePromptPopupData* data = (SimplePromptPopupData*)self;
    WindowManager* wm = (WindowManager*)self->manager;
    FlintContext_Get()->window.selection_complete = false;
    FlintContext_Get()->window.output_buffer[0] = '\0';
    extern bool g_Linux_CtrlHeld, g_Linux_AltHeld, g_Linux_ShiftHeld;
    extern bool g_linux_ctrl_held, g_linux_alt_held, g_linux_shift_held;
    g_Linux_CtrlHeld = g_Linux_AltHeld = g_Linux_ShiftHeld = false;
    g_linux_ctrl_held = g_linux_alt_held = g_linux_shift_held = false;
    
    #if !defined(_WIN32)
    Display* dpy = wm->display;
    data->base.handle.xid = Linux_CreateSimplePromptWindow(dpy, data->x, data->y, data->w, data->h);
    XSelectInput(dpy, data->base.handle.xid, KeyPressMask | ExposureMask);
    XMapWindow(dpy, data->base.handle.xid);
    XFlush(dpy);
    XSync(dpy, False);
    {
        int (*old)(Display*, XErrorEvent*) = XSetErrorHandler(x11_error_handler);
        XSetInputFocus(dpy, data->base.handle.xid, RevertToParent, CurrentTime);
        XSync(dpy, False);
        XSetErrorHandler(old);
    }
    XFlush(dpy);
    #endif
    
    data->state = calloc(1, sizeof(SimplePromptState));
    if (!data->state) { TimerEnd(timer, "window_manager:SimplePromptPopup_OnShow"); return; }
    
    data->state->display_message = (char*)data->spec.prompt;
    data->state->user_input[0] = '\0';
    data->state->cursor = 0;
    data->state->width = data->w;
    data->state->height = data->h;
    const char* p = strchr(data->spec.title, ' ');
    data->state->window_title = (char*)(p ? p+1 : data->spec.title);
    if (!data->state->window_title) data->state->window_title = (char*)data->spec.title;
    self->user_data = data->state;
    
    SetMode(MODE_INSERT);
    TimerEnd(timer, "window_manager:SimplePromptPopup_OnShow");
}

static void SimplePromptPopup_OnKey(Popup* self, int vk_code, bool pressed, int modifiers) {
    WinTimer timer = TimerStart();
    if (!pressed) { TimerEnd(timer, "window_manager:SimplePromptPopup_OnKey"); return; }
    SimplePromptPopupData* data = (SimplePromptPopupData*)self;
    if (!data->state) { TimerEnd(timer, "window_manager:SimplePromptPopup_OnKey"); return; }
    
    #if !defined(_WIN32)
    if (vk_code == VK_ESCAPE) {
        if (IsInsertMode()) {
            SetMode(MODE_NORMAL);
            if (self->vtable && self->vtable->on_render) self->vtable->on_render(self);
        } else {
            FlintContext_Get()->window.selection_complete = true;
            WindowManager_PopPopup(self->manager);
        }
        TimerEnd(timer, "window_manager:SimplePromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_BACK) {
        if (data->state->cursor > 0) {
            size_t n = strlen(data->state->user_input);
            memmove(data->state->user_input + data->state->cursor - 1,
                    data->state->user_input + data->state->cursor,
                    n - data->state->cursor + 1);
            data->state->cursor--;
        }
        TimerEnd(timer, "window_manager:SimplePromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_LEFT) {
        if (data->state->cursor > 0) data->state->cursor--;
        TimerEnd(timer, "window_manager:SimplePromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_RIGHT) {
        if (data->state->cursor < (int)strlen(data->state->user_input)) data->state->cursor++;
        TimerEnd(timer, "window_manager:SimplePromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_HOME) {
        data->state->cursor = 0;
        TimerEnd(timer, "window_manager:SimplePromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_END) {
        data->state->cursor = strlen(data->state->user_input);
        TimerEnd(timer, "window_manager:SimplePromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_RETURN) {
        FlintContext* _c = FlintContext_Get();
        strncpy(_c->window.output_buffer, data->state->user_input, sizeof(_c->window.output_buffer)-1);
        _c->window.output_buffer[sizeof(_c->window.output_buffer)-1]='\0';
        _c->window.selection_complete = true;
        WindowManager_PopPopup(self->manager);
        TimerEnd(timer, "window_manager:SimplePromptPopup_OnKey");
        return;
    }
    if (vk_code >= 32 && vk_code <= 126) {
        if (IsInsertMode()) {
            size_t n = strlen(data->state->user_input);
            if (n < sizeof(data->state->user_input)-1) {
                if (data->state->cursor < 0) data->state->cursor=0;
                if ((size_t)data->state->cursor > n) data->state->cursor=n;
                memmove(data->state->user_input + data->state->cursor + 1,
                        data->state->user_input + data->state->cursor,
                        n - data->state->cursor + 1);
                data->state->user_input[data->state->cursor] = (char)vk_code;
                data->state->cursor++;
            }
        }
        TimerEnd(timer, "window_manager:SimplePromptPopup_OnKey");
        return;
    }
    {
        XEvent ev = {0};
        ev.type = KeyPress;
        ev.xkey.keycode = vk_code;
        ev.xkey.state = modifiers;
        Linux_HandleSimplePromptKey(&ev, data->state);
    }
    #endif
    FlintContext* _c4 = FlintContext_Get();
    if (_c4->window.selection_complete) {
        WindowManager_PopPopup(self->manager);
        _c4->window.selection_complete = false;
    }
    TimerEnd(timer, "window_manager:SimplePromptPopup_OnKey");
}

static void SimplePromptPopup_OnHide(Popup* self) {
    WinTimer timer = TimerStart();
    #if !defined(_WIN32)
    Display* dpy = get_display();
    if (dpy) {
        XUnmapWindow(dpy, self->handle.xid);
        XFlush(dpy);
    }
    #endif
    TimerEnd(timer, "window_manager:SimplePromptPopup_OnHide");
}

static void SimplePromptPopup_OnRender(Popup* self) {
    WinTimer timer = TimerStart();
    SimplePromptPopupData* data = (SimplePromptPopupData*)self;
    if (!data->state) { TimerEnd(timer, "window_manager:SimplePromptPopup_OnRender"); return; }
    
    #if !defined(_WIN32)
    Display* dpy = ((WindowManager*)self->manager)->display;
    Linux_RenderSimplePromptUser(dpy, &self->handle.xid, data->state);
    #endif
    TimerEnd(timer, "window_manager:SimplePromptPopup_OnRender");
}

static void SimplePromptPopup_Destroy(Popup* self) {
    WinTimer timer = TimerStart();
    SimplePromptPopupData* data = (SimplePromptPopupData*)self;
    #if !defined(_WIN32)
    Display* dpy = get_display();
    if (dpy) XDestroyWindow(dpy, self->handle.xid);
    #endif
    free(data->state);
    free(data);
    TimerEnd(timer, "window_manager:SimplePromptPopup_Destroy");
}

static const PopupVTable g_simple_prompt_popup_vtable = {
    .on_show = SimplePromptPopup_OnShow,
    .on_hide = SimplePromptPopup_OnHide,
    .on_key = SimplePromptPopup_OnKey,
    .on_render = SimplePromptPopup_OnRender,
    .destroy = SimplePromptPopup_Destroy
};

Popup* SimplePromptPopup_Create(WindowManager* wm, const SimplePromptSpec* spec) {
    WinTimer timer = TimerStart();
    if (!wm || !spec) { TimerEnd(timer, "window_manager:SimplePromptPopup_Create"); return NULL; }
    
    int screen_width = 1920;
    int screen_height = 1080;
    int layout_w = 900;
    int layout_h = 95 + (1 > 0 ? (1 * spec->item_height) : 0);
    
    if (layout_h < spec->height) layout_h = spec->height;
    if (layout_h > spec->width) layout_h = spec->width;
    
    int x = (screen_width - layout_w) / 2;
    int y = (screen_height - layout_h) / 2;
    
    SimplePromptPopupData* data = calloc(1, sizeof(SimplePromptPopupData));
    if (!data) { TimerEnd(timer, "window_manager:SimplePromptPopup_Create"); return NULL; }
    
    data->base.vtable = &g_simple_prompt_popup_vtable;
    data->base.manager = wm;
    data->base.type = POPUP_SIMPLE;
    data->spec = *spec;
    data->x = x; data->y = y; data->w = layout_w; data->h = layout_h;
    
    TimerEnd(timer, "window_manager:SimplePromptPopup_Create");
    return WindowManager_PushPopup(wm, (Popup*)data);
}

// Search popup stub
Popup* SearchPopup_Create(WindowManager* wm, const char* title, int x, int y, int w, int h,
                          EditPopupConfirmCallback on_confirm, void* user_data) {
    WinTimer timer = TimerStart();
    (void)wm; (void)title; (void)x; (void)y; (void)w; (void)h;
    (void)on_confirm; (void)user_data;
    TimerEnd(timer, "window_manager:SearchPopup_Create");
    return NULL; // TODO
}
