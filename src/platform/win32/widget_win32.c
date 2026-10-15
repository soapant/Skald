#include <windows.h>
#include <wingdi.h>
#include <stdlib.h>
#include <string.h>
#include "platform/widget.h"
#include "platform/draw.h"
#include "platform/render_thread.h"
#include "platform/ui.h"
#include "deps/log.h"
#include "config.h"

#include "core/timer.h"
#define FLINT_WIDGET_CLASS "FlintWidgetClass"

struct PlatformWidget {
    HWND hwnd;
    int x, y, w, h;
    bool mapped;
    bool dead;
    Platform_RenderFrameFn fn;
    void *ud;
    int fps;
    unsigned long bg_hex;
    bool has_bg;
    bool no_bg;
    bool dirty;
    DWORD last_draw_ms;
    DWORD owner_tid;
    BITMAPINFO layer_bi;
    HDC layer_dc;
    HBITMAP layer_bmp;
    HGDIOBJ layer_old;
    void *layer_bits;
    int layer_w, layer_h;
    void (*on_click)(void);
    char name[64];
};

static void layer_fill(PlatformWidget *widget, unsigned int argb) {
        PROFILE_START(_pt);
unsigned int *px = (unsigned int *)widget->layer_bits;
    size_t n = (size_t)widget->layer_w * (size_t)widget->layer_h;
    for (size_t i = 0; i < n; i++) px[i] = argb;
}

static LRESULT CALLBACK FlintWidgetProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void widget_queue_click(void (*fn)(void));

static void widget_register_class(void) {
        PROFILE_START(_pt);
static LONG registered = 0;
    if (InterlockedCompareExchange(&registered, 1, 0) != 0) { PROFILE_END_VAR(_pt, "widget_register_class"); return ; }
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = FlintWidgetProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = FLINT_WIDGET_CLASS;
    if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        log_warn("widget: RegisterClassA failed err=%lu", GetLastError());
    }
}

static void layer_release(PlatformWidget *widget) {
        PROFILE_START(_pt);
if (widget->layer_dc) {
        if (widget->layer_old) SelectObject(widget->layer_dc, widget->layer_old);
        if (widget->layer_bmp) DeleteObject(widget->layer_bmp);
        DeleteDC(widget->layer_dc);
    }
    widget->layer_dc = NULL;
    widget->layer_bmp = NULL;
    widget->layer_old = NULL;
    widget->layer_bits = NULL;
    widget->layer_w = 0;
    widget->layer_h = 0;
}

static bool layer_acquire(PlatformWidget *widget) {
        PROFILE_START(_pt);
if (widget->layer_bits && widget->layer_w == widget->w && widget->layer_h == widget->h) { PROFILE_END_VAR(_pt, "layer_acquire"); return true; }
    layer_release(widget);
    memset(&widget->layer_bi, 0, sizeof(widget->layer_bi));
    widget->layer_bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    widget->layer_bi.bmiHeader.biWidth = widget->w;
    widget->layer_bi.bmiHeader.biHeight = -widget->h;
    widget->layer_bi.bmiHeader.biPlanes = 1;
    widget->layer_bi.bmiHeader.biBitCount = 32;
    widget->layer_bi.bmiHeader.biCompression = BI_RGB;
    HDC screen = GetDC(NULL);
    if (!screen) { PROFILE_END_VAR(_pt, "layer_acquire"); return false; }
    HDC dc = CreateCompatibleDC(screen);
    ReleaseDC(NULL, screen);
    if (!dc) { PROFILE_END_VAR(_pt, "layer_acquire"); return false; }
    void *bits = NULL;
    HBITMAP bmp = CreateDIBSection(dc, &widget->layer_bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bmp) {
        DeleteDC(dc);
        return false;
    }
    widget->layer_dc = dc;
    widget->layer_bmp = bmp;
    widget->layer_old = SelectObject(dc, bmp);
    widget->layer_bits = bits;
    widget->layer_w = widget->w;
    widget->layer_h = widget->h;
    { PROFILE_END_VAR(_pt, "layer_acquire"); return true; }
}

/* GDI antialiasing blends toward the destination fill and never writes the alpha
   byte, so the layer is filled opaque black and rendered once. On the black
   background each glyph pixel is already coverage-scaled text colour, which is
   exactly the premultiplied form AC_SRC_ALPHA expects, so the blue channel is a
   direct coverage readout. Rendering once per frame keeps feature render
   callbacks (and their per-frame state) intact. */
static void widget_push_layered(PlatformWidget *widget, unsigned long bg, unsigned char alpha) {
        PROFILE_START(_pt);
if (!layer_acquire(widget)) { PROFILE_END_VAR(_pt, "widget_push_layered"); return ; }

    layer_fill(widget, bg);
    Platform_SetDrawBgColor(0);
    SetBkMode(widget->layer_dc, TRANSPARENT);
    Platform_SetDrawContextWin32(widget->layer_dc);
    Platform_SetPreRenderStateWin32(NULL, NULL);
    if (widget->fn) widget->fn(widget->ud);
    GdiFlush();

    {
        unsigned int* px = (unsigned int*)widget->layer_bits;
        size_t n = (size_t)widget->layer_w * (size_t)widget->layer_h;
        if (widget->no_bg) {
            for (size_t i = 0; i < n; i++) {
                unsigned int b = px[i] & 0xFFu;
                unsigned int g = (px[i] >> 8) & 0xFFu;
                unsigned int r = (px[i] >> 16) & 0xFFu;
                unsigned int a = b > g ? b : g;
                if (r > a) a = r;
                px[i] = (px[i] & 0x00FFFFFFu) | (a << 24);
            }
        } else {
            for (size_t i = 0; i < n; i++) px[i] |= 0xFF000000u;
        }

        if (widget->no_bg) {
            for (size_t i = 0; i < n; i++) {
                if ((px[i] & 0x00FFFFFFu) != 0) px[i] = (px[i] & 0x00FFFFFFu) | 0xFF000000u;
                else px[i] = 0u;
            }
        }
    }

    HDC screen = GetDC(NULL);
    if (!screen) { PROFILE_END_VAR(_pt, "widget_push_layered"); return ; }
    POINT dst;
    dst.x = widget->x;
    dst.y = widget->y;
    SIZE size;
    size.cx = widget->layer_w;
    size.cy = widget->layer_h;
    POINT src;
    src.x = 0;
    src.y = 0;
    BLENDFUNCTION blend;
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = alpha;
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(widget->hwnd, screen, &dst, &size, widget->layer_dc, &src, 0, &blend, ULW_ALPHA);
    ReleaseDC(NULL, screen);
}

static void widget_paint_layered(PlatformWidget *widget) {
        PROFILE_START(_pt);
widget_push_layered(widget, 0xFF000000u, 255);
}

static void widget_paint(PlatformWidget *widget, HDC hdc, RECT *rect) {
        PROFILE_START(_pt);
(void)hdc;
    (void)rect;
    widget_push_layered(widget, 0xFF000000u | (widget->has_bg ? widget->bg_hex : 0u), (unsigned char)WindowOpacity);
}

static LRESULT CALLBACK FlintWidgetProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        PROFILE_START(_pt);
PlatformWidget *widget = (PlatformWidget *)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_NCCREATE: {
        CREATESTRUCTA *cs = (CREATESTRUCTA *)lParam;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    case WM_NCHITTEST: {
        if (widget && widget->on_click) return HTCLIENT;
        return HTTRANSPARENT;
    }
    case WM_LBUTTONDOWN: {
        if (widget && widget->on_click) widget_queue_click(widget->on_click);
        return 0;
    }
    case WM_PAINT: {
        if (!widget || widget->dead) break;
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (widget->no_bg) {
            widget_paint_layered(widget);
        } else if (hdc) {
            RECT rect;
            GetClientRect(hwnd, &rect);
            widget_paint(widget, hdc, &rect);
        }
        if (hdc) EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_NCDESTROY: {
        if (widget) layer_release(widget);
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
        break;
    }
    }
    { PROFILE_END_VAR(_pt, "FlintWidgetProc"); return DefWindowProcA(hwnd, msg, wParam, lParam); }
}

static void widget_frame(void *ud) {
        PROFILE_START(_pt);
PlatformWidget *widget = (PlatformWidget *)ud;
    if (!widget || widget->dead) { PROFILE_END_VAR(_pt, "widget_frame"); return ; }
    HWND hwnd = widget->hwnd;
    if (!hwnd || !IsWindow(hwnd)) { PROFILE_END_VAR(_pt, "widget_frame"); return ; }
    DWORD now = GetTickCount();
    if (!widget->dirty) {
        DWORD interval = widget->fps > 0 ? (DWORD)(1000 / widget->fps) : 1000;
        if (interval < 16) interval = 16;
        if (now - widget->last_draw_ms < interval) return;
    }
    widget->dirty = false;
    widget->last_draw_ms = now;
    InvalidateRect(hwnd, NULL, FALSE);
    UpdateWindow(hwnd);
    MSG msg;
    while (PeekMessageA(&msg, hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

#define WCLICK_MAX 32
static void (*g_wclick[WCLICK_MAX])(void);
static int g_wclick_head = 0, g_wclick_tail = 0;
static CRITICAL_SECTION g_wclick_cs;
static LONG g_wclick_cs_ready = 0;

static void wclick_lock(void) {
        PROFILE_START(_pt);
if (InterlockedCompareExchange(&g_wclick_cs_ready, 1, 0) == 0) {
        InitializeCriticalSection(&g_wclick_cs);
    }
    EnterCriticalSection(&g_wclick_cs);
}
static void wclick_unlock(void) {
        PROFILE_START(_pt);
if (InterlockedCompareExchange(&g_wclick_cs_ready, 1, 0) == 0) {
        InitializeCriticalSection(&g_wclick_cs);
    }
    LeaveCriticalSection(&g_wclick_cs);
}

static void widget_queue_click(void (*fn)(void)) {
        PROFILE_START(_pt);
if (!fn) { PROFILE_END_VAR(_pt, "widget_queue_click"); return ; }
    wclick_lock();
    int next = (g_wclick_tail + 1) % WCLICK_MAX;
    if (next != g_wclick_head) {
        g_wclick[g_wclick_tail] = fn;
        g_wclick_tail = next;
    }
    wclick_unlock();
}

void Platform_Widget_DrainClicks(void) {
        PROFILE_START(_pt);
for (;;) {
        void (*fn)(void) = NULL;
        wclick_lock();
        if (g_wclick_head != g_wclick_tail) {
            fn = g_wclick[g_wclick_head];
            g_wclick[g_wclick_head] = NULL;
            g_wclick_head = (g_wclick_head + 1) % WCLICK_MAX;
        }
        wclick_unlock();
        if (!fn) break;
        fn();
    }
}

void Platform_Widget_Invalidate(PlatformWidget *widget) {
        PROFILE_START(_pt);
if (!widget || widget->dead) { PROFILE_END_VAR(_pt, "Platform_Widget_Invalidate"); return ; }
    widget->dirty = true;
    Platform_RenderThread_Wake();
}

PlatformWidget *Platform_Widget_Create(int x, int y, int w, int h, int fps, bool no_bg, void (*on_click)(void), const char *name) {
        PROFILE_START(_pt);
if (w <= 0 || h <= 0) { PROFILE_END_VAR(_pt, "Platform_Widget_Create"); return NULL; }
    if (!Platform_RenderThread_EnsureStarted(fps)) { PROFILE_END_VAR(_pt, "Platform_Widget_Create"); return NULL; }
    widget_register_class();

    PlatformWidget *widget = (PlatformWidget *)calloc(1, sizeof(*widget));
    if (!widget) { PROFILE_END_VAR(_pt, "Platform_Widget_Create"); return NULL; }
    widget->x = x;
    widget->y = y;
    widget->w = w;
    widget->h = h;
    widget->fps = fps;
    widget->no_bg = no_bg;
    widget->on_click = on_click;
    snprintf(widget->name, sizeof(widget->name), "%s", name ? name : "?");
    widget->dirty = true;
    widget->last_draw_ms = 0;
    widget->owner_tid = GetCurrentThreadId();

    HINSTANCE hInstance = GetModuleHandle(NULL);
    DWORD exstyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE;
    if (!on_click) exstyle |= WS_EX_TRANSPARENT;
widget->hwnd = CreateWindowExA(
        exstyle,
        FLINT_WIDGET_CLASS, NULL, WS_POPUP,
        x, y, w, h, NULL, NULL, hInstance, widget
    );
    if (!widget->hwnd) {
        log_warn("widget: CreateWindowExA failed err=%lu", GetLastError());
        free(widget);
        return NULL;
    }

    if (!widget->no_bg) {
        win32_apply_rounded_corners(widget->hwnd, w, h);
    }
    SetWindowPos(widget->hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    if (!Platform_RenderThread_AddFrameCallback(widget_frame, widget)) {
        DestroyWindow(widget->hwnd);
        free(widget);
        return NULL;
    }

    unsigned long amb = Platform_GetDrawBgColor();
    if (amb) {
        widget->has_bg = true;
        widget->bg_hex = amb;
    }

    widget_frame(widget);

    ShowWindow(widget->hwnd, SW_SHOWNOACTIVATE);
    SetWindowPos(widget->hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    widget->mapped = true;
    { PROFILE_END_VAR(_pt, "Platform_Widget_Create"); return widget; }
}

static void widget_destroy_on_owner_thread(void* ud) {
        PROFILE_START(_pt);
PlatformWidget *widget = (PlatformWidget *)ud;
    if (!widget || widget->dead) { PROFILE_END_VAR(_pt, "widget_destroy_on_owner_thread"); return ; }
    widget->dead = true;
    Platform_RenderThread_RemoveFrameCallback(widget_frame, widget);
    if (widget->hwnd && IsWindow(widget->hwnd)) {
        DestroyWindow(widget->hwnd);
    }
    widget->hwnd = NULL;
    free(widget);
}

void Platform_Widget_Destroy(PlatformWidget *widget) {
        PROFILE_START(_pt);
if (!widget || widget->dead) { PROFILE_END_VAR(_pt, "Platform_Widget_Destroy"); return ; }
    if (widget->owner_tid != GetCurrentThreadId()) {
        PlatformWidget *copy = (PlatformWidget *)malloc(sizeof(*copy));
        if (copy) {
            *copy = *widget;
            widget->dead = true;
            if (Platform_RenderThread_Post(widget_destroy_on_owner_thread, copy)) return;
            free(copy);
            widget->dead = false;
        }
    }
    widget_destroy_on_owner_thread(widget);
}

bool Platform_Widget_IsValid(const PlatformWidget *widget) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "Platform_Widget_IsValid"); return widget && !widget->dead; }
}

void Platform_Widget_SetFrameCallback(PlatformWidget *widget, Platform_RenderFrameFn fn, void *ud) {
        PROFILE_START(_pt);
if (!widget || widget->dead) { PROFILE_END_VAR(_pt, "Platform_Widget_SetFrameCallback"); return ; }
    widget->fn = fn;
    widget->ud = ud;
}

void Platform_Widget_Show(PlatformWidget *widget) {
        PROFILE_START(_pt);
if (!widget || widget->dead || widget->mapped) { PROFILE_END_VAR(_pt, "Platform_Widget_Show"); return ; }
    if (!widget->hwnd || !IsWindow(widget->hwnd)) { PROFILE_END_VAR(_pt, "Platform_Widget_Show"); return ; }
    ShowWindow(widget->hwnd, SW_SHOWNOACTIVATE);
    SetWindowPos(widget->hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    widget->mapped = true;
}

void Platform_Widget_Hide(PlatformWidget *widget) {
        PROFILE_START(_pt);
if (!widget || widget->dead || !widget->mapped) { PROFILE_END_VAR(_pt, "Platform_Widget_Hide"); return ; }
    if (!widget->hwnd || !IsWindow(widget->hwnd)) { PROFILE_END_VAR(_pt, "Platform_Widget_Hide"); return ; }
    ShowWindow(widget->hwnd, SW_HIDE);
    widget->mapped = false;
}

void Platform_Widget_Move(PlatformWidget *widget, int x, int y) {
        PROFILE_START(_pt);
if (!widget || widget->dead) { PROFILE_END_VAR(_pt, "Platform_Widget_Move"); return ; }
    if (!widget->hwnd || !IsWindow(widget->hwnd)) { PROFILE_END_VAR(_pt, "Platform_Widget_Move"); return ; }
    if (widget->x == x && widget->y == y) { PROFILE_END_VAR(_pt, "Platform_Widget_Move"); return ; }
    widget->x = x;
    widget->y = y;
    SetWindowPos(widget->hwnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
}

void Platform_Widget_SetNoBg(PlatformWidget *widget, bool no_bg) {
        PROFILE_START(_pt);
if (!widget || widget->dead) { PROFILE_END_VAR(_pt, "Platform_Widget_SetNoBg"); return ; }
    widget->no_bg = no_bg;
    Platform_Widget_Invalidate(widget);
}

void Platform_Widget_SetFps(PlatformWidget *widget, int fps) {
        PROFILE_START(_pt);
if (!widget || widget->dead) { PROFILE_END_VAR(_pt, "Platform_Widget_SetFps"); return ; }
    if (fps <= 0) fps = 1;
    if (widget->fps == fps) { PROFILE_END_VAR(_pt, "Platform_Widget_SetFps"); return ; }
    widget->fps = fps;
    Platform_RenderThread_EnsureStarted(fps);
    Platform_Widget_Invalidate(widget);
}

void Platform_Widget_SetBgColor(PlatformWidget *widget, unsigned long hex) {
        PROFILE_START(_pt);
if (!widget || widget->dead) { PROFILE_END_VAR(_pt, "Platform_Widget_SetBgColor"); return ; }
    widget->has_bg = hex != 0;
    widget->bg_hex = hex;
    Platform_Widget_Invalidate(widget);
}

void Platform_Widget_Resize(PlatformWidget *widget, int w, int h) {
        PROFILE_START(_pt);
if (!widget || widget->dead) { PROFILE_END_VAR(_pt, "Platform_Widget_Resize"); return ; }
    if (w <= 0 || h <= 0) { PROFILE_END_VAR(_pt, "Platform_Widget_Resize"); return ; }
    if (!widget->hwnd || !IsWindow(widget->hwnd)) { PROFILE_END_VAR(_pt, "Platform_Widget_Resize"); return ; }
    if (widget->w == w && widget->h == h) { PROFILE_END_VAR(_pt, "Platform_Widget_Resize"); return ; }
    widget->w = w;
    widget->h = h;
    SetWindowPos(widget->hwnd, HWND_TOPMOST, widget->x, widget->y, w, h, SWP_NOACTIVATE);
    Platform_Widget_Invalidate(widget);
}


int Platform_Widget_Width(const PlatformWidget *widget) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "Platform_Widget_Width"); return widget ? widget->w : 0; }
}

int Platform_Widget_Height(const PlatformWidget *widget) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "Platform_Widget_Height"); return widget ? widget->h : 0; }
}
