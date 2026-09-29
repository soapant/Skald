#include <windows.h>
#include <wingdi.h>
#include <stdlib.h>
#include "platform/widget.h"
#include "platform/draw.h"
#include "platform/render_thread.h"
#include "platform/ui.h"
#include "deps/log.h"
#include "config.h"

#define FLINT_WIDGET_CLASS "FlintWidgetClass"

struct PlatformWidget {
    HWND hwnd;
    int x, y, w, h;
    bool mapped;
    bool dead;
    Platform_RenderFrameFn fn;
    void *ud;
    int fps;
};

static LRESULT CALLBACK FlintWidgetProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static void widget_register_class(void) {
    static LONG registered = 0;
    if (InterlockedCompareExchange(&registered, 1, 0) != 0) return;
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

static void widget_paint(PlatformWidget *widget, HDC hdc, RECT *rect) {
    if (EnableShaders && ShaderPreset != 0) {
        win32_draw_background(hdc, rect);
    } else {
        HBRUSH brush = CreateSolidBrush(COLOR_BG);
        FillRect(hdc, rect, brush);
        DeleteObject(brush);
    }
    SetBkMode(hdc, TRANSPARENT);
    Platform_SetDrawContextWin32(hdc);
    Platform_SetPreRenderStateWin32(NULL, NULL);
    if (widget->fn) widget->fn(widget->ud);
}

static LRESULT CALLBACK FlintWidgetProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PlatformWidget *widget = (PlatformWidget *)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_NCCREATE: {
        CREATESTRUCTA *cs = (CREATESTRUCTA *)lParam;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    case WM_PAINT: {
        if (!widget || widget->dead) break;
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (hdc) {
            RECT rect;
            GetClientRect(hwnd, &rect);
            widget_paint(widget, hdc, &rect);
            EndPaint(hwnd, &ps);
        }
        return 0;
    }
    case WM_NCDESTROY: {
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
        break;
    }
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static void widget_frame(void *ud) {
    PlatformWidget *widget = (PlatformWidget *)ud;
    if (!widget || widget->dead) return;
    HWND hwnd = widget->hwnd;
    if (!hwnd || !IsWindow(hwnd)) return;
    InvalidateRect(hwnd, NULL, FALSE);
    UpdateWindow(hwnd);
    MSG msg;
    while (PeekMessageA(&msg, hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

PlatformWidget *Platform_Widget_Create(int x, int y, int w, int h, int fps) {
    if (w <= 0 || h <= 0) return NULL;
    if (!Platform_RenderThread_EnsureStarted(fps)) return NULL;
    widget_register_class();

    PlatformWidget *widget = (PlatformWidget *)calloc(1, sizeof(*widget));
    if (!widget) return NULL;
    widget->x = x;
    widget->y = y;
    widget->w = w;
    widget->h = h;
    widget->fps = fps;

    HINSTANCE hInstance = GetModuleHandle(NULL);
    widget->hwnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        FLINT_WIDGET_CLASS, NULL, WS_POPUP,
        x, y, w, h, NULL, NULL, hInstance, widget
    );
    if (!widget->hwnd) {
        log_warn("widget: CreateWindowExA failed err=%lu", GetLastError());
        free(widget);
        return NULL;
    }

    win32_apply_rounded_corners(widget->hwnd, w, h);
    SetLayeredWindowAttributes(widget->hwnd, 0, (BYTE)WindowOpacity, LWA_ALPHA);
    ShowWindow(widget->hwnd, SW_SHOWNOACTIVATE);
    SetWindowPos(widget->hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    widget->mapped = true;

    if (!Platform_RenderThread_AddFrameCallback(widget_frame, widget)) {
        DestroyWindow(widget->hwnd);
        free(widget);
        return NULL;
    }
    log_info("widget created hwnd=%p at %d,%d %dx%d fps=%d", (void *)widget->hwnd, x, y, w, h, fps);
    return widget;
}

void Platform_Widget_Destroy(PlatformWidget *widget) {
    if (!widget || widget->dead) return;
    widget->dead = true;
    Platform_RenderThread_RemoveFrameCallback(widget_frame, widget);
    if (widget->hwnd && IsWindow(widget->hwnd)) {
        DestroyWindow(widget->hwnd);
    }
    widget->hwnd = NULL;
    log_info("widget destroyed");
    free(widget);
}

bool Platform_Widget_IsValid(const PlatformWidget *widget) {
    return widget && !widget->dead;
}

void Platform_Widget_SetFrameCallback(PlatformWidget *widget, Platform_RenderFrameFn fn, void *ud) {
    if (!widget || widget->dead) return;
    widget->fn = fn;
    widget->ud = ud;
}

void Platform_Widget_Show(PlatformWidget *widget) {
    if (!widget || widget->dead || widget->mapped) return;
    if (!widget->hwnd || !IsWindow(widget->hwnd)) return;
    ShowWindow(widget->hwnd, SW_SHOWNOACTIVATE);
    SetWindowPos(widget->hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    widget->mapped = true;
}

void Platform_Widget_Hide(PlatformWidget *widget) {
    if (!widget || widget->dead || !widget->mapped) return;
    if (!widget->hwnd || !IsWindow(widget->hwnd)) return;
    ShowWindow(widget->hwnd, SW_HIDE);
    widget->mapped = false;
}

int Platform_Widget_Width(const PlatformWidget *widget) {
    return widget ? widget->w : 0;
}

int Platform_Widget_Height(const PlatformWidget *widget) {
    return widget ? widget->h : 0;
}
