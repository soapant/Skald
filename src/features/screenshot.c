#include "core/flapi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SHOT_BUF 1024

#if !defined(_WIN32)

#include <unistd.h>

static const char* shot_working_dir(void) {
    char* wd = fl_get_working_dir();
    return (wd && wd[0]) ? wd : ".";
}

static void shot_env(char* buf, size_t n) {
    snprintf(buf, n, "LD_LIBRARY_PATH=%s/build/bundle:${LD_LIBRARY_PATH:-} ", shot_working_dir());
}

static void shot_clip_path(char* buf, size_t n) {
    snprintf(buf, n, "%s/flint_clip.png", shot_working_dir());
}

static void pk_screenshot_copy_area(void) {
    char env[SHOT_BUF];
    char path[SHOT_BUF];
    char cmd[SHOT_BUF * 2];
    shot_env(env, sizeof(env));
    shot_clip_path(path, sizeof(path));

    if (getenv("WAYLAND_DISPLAY")) {
        snprintf(cmd, sizeof(cmd),
                 "%s%s/external/grim -g \"$(%s/external/slurp)\" -t png %s",
                 env, shot_working_dir(), shot_working_dir(), path);
    } else {
        snprintf(cmd, sizeof(cmd), "%s%s/external/scrot -s %s", env, shot_working_dir(), path);
    }

    free(fl_run_terminal_command_blocking(cmd));
    fl_copy_file_path(path);
}

static void pk_ocr_area_to_clipboard(void) {
    char env[SHOT_BUF];
    char cmd[SHOT_BUF * 2];
    shot_env(env, sizeof(env));

    if (getenv("WAYLAND_DISPLAY")) {
        snprintf(cmd, sizeof(cmd),
                 "%s%s/external/grim -g \"$(%s/external/slurp)\" - | tesseract stdin stdout -l eng png",
                 env, shot_working_dir(), shot_working_dir());
    } else {
        char temp_path[SHOT_BUF];
        snprintf(temp_path, sizeof(temp_path), "/tmp/flint_ocr_%d", (int)getpid());
        snprintf(cmd, sizeof(cmd),
                 "%s%s/external/scrot -s %s && tesseract %s stdout -l eng; rm -f %s",
                 env, shot_working_dir(), temp_path, temp_path, temp_path);
    }

    char* out = fl_run_terminal_command_blocking(cmd);
    if (!out || !out[0]) { free(out); return; }

    size_t n = strlen(out);
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r')) out[--n] = '\0';
    if (out[0]) fl_copy_to_clipboard(out);
    free(out);
}

#else

#include <windows.h>
#include <windowsx.h>
#include <wingdi.h>
#include <shellapi.h>
#include <objidl.h>
#include <gdiplus.h>

static ULONG_PTR g_shot_gdiplus = 0;

static bool shot_gdiplus_start(void) {
    if (g_shot_gdiplus) return true;
    GdiplusStartupInput in;
    memset(&in, 0, sizeof(in));
    in.GdiplusVersion = 1;
    GpStatus st = GdiplusStartup(&g_shot_gdiplus, &in, NULL);
    if (st != Ok) {
        printf("screenshot: GdiplusStartup failed status=%d\n", (int)st);
        g_shot_gdiplus = 0;
        return false;
    }
    return true;
}

static void shot_ov_release(void);

#define SHOT_SEL_CLASS "FlintShotSelClass"
#define SHOT_SEL_FRAME 2

#define SEL_DIM    0x6E000000u
#define SEL_CLEAR  0x00000000u
#define SEL_FRAME_C 0xFFFFFFFFu

static bool g_sel_dragging = false;
static bool g_sel_done = false;
static bool g_sel_cancel = false;
static bool g_sel_has = false;
static POINT g_sel_anchor = {0, 0};
static RECT g_sel_cur = {0, 0, 0, 0};
static RECT g_sel_prev = {0, 0, 0, 0};
static RECT g_sel_result = {0, 0, 0, 0};

static int g_ov_w = 0, g_ov_h = 0;
static HDC g_ov_dc = NULL;
static HBITMAP g_ov_bmp = NULL;
static void* g_ov_bits = NULL;
static HWND g_ov_hwnd = NULL;

static void sel_fill(int l, int t, int r, int b, unsigned int argb) {
    if (!g_ov_bits) return;
    if (l < 0) l = 0;
    if (t < 0) t = 0;
    if (r > g_ov_w) r = g_ov_w;
    if (b > g_ov_h) b = g_ov_h;
    for (int y = t; y < b; y++) {
        unsigned int* row = (unsigned int*)g_ov_bits + (size_t)y * (size_t)g_ov_w;
        for (int x = l; x < r; x++) row[x] = argb;
    }
}

static void sel_paint_sel(int l, int t, int r, int b) {
    sel_fill(l, t, r, b, SEL_CLEAR);
    int f = SHOT_SEL_FRAME;
    sel_fill(l, t, r, t + f, SEL_FRAME_C);
    sel_fill(l, b - f, r, b, SEL_FRAME_C);
    sel_fill(l, t, l + f, b, SEL_FRAME_C);
    sel_fill(r - f, t, r, b, SEL_FRAME_C);
}

static void sel_push(void) {
    if (!g_ov_hwnd || !g_ov_dc) return;
    HDC screen = GetDC(NULL);
    if (!screen) return;
    POINT dst;
    dst.x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    dst.y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    POINT src;
    src.x = 0; src.y = 0;
    SIZE size;
    size.cx = g_ov_w; size.cy = g_ov_h;
    BLENDFUNCTION blend;
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    SetLayout(g_ov_dc, 0);
    UpdateLayeredWindow(g_ov_hwnd, screen, &dst, &size, g_ov_dc, &src, 0, &blend, ULW_ALPHA);
    ReleaseDC(NULL, screen);
}

static LRESULT CALLBACK ShotSelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_LBUTTONDOWN:
        g_sel_anchor.x = GET_X_LPARAM(lp);
        g_sel_anchor.y = GET_Y_LPARAM(lp);
        g_sel_dragging = true;
        g_sel_has = false;
        SetCapture(hwnd);
        return 0;
    case WM_MOUSEMOVE: {
        if (!g_sel_dragging) return 0;
        int cx = GET_X_LPARAM(lp);
        int cy = GET_Y_LPARAM(lp);
        if (cx < 0) cx = 0;
        if (cx > g_ov_w) cx = g_ov_w;
        if (cy < 0) cy = 0;
        if (cy > g_ov_h) cy = g_ov_h;
        int l = cx < g_sel_anchor.x ? cx : g_sel_anchor.x;
        int r = cx < g_sel_anchor.x ? g_sel_anchor.x : cx;
        int t = cy < g_sel_anchor.y ? cy : g_sel_anchor.y;
        int b = cy < g_sel_anchor.y ? g_sel_anchor.y : cy;
        if (r - l < SHOT_SEL_FRAME * 2) r = l + SHOT_SEL_FRAME * 2;
        if (b - t < SHOT_SEL_FRAME * 2) b = t + SHOT_SEL_FRAME * 2;
        if (r > g_ov_w) r = g_ov_w;
        if (b > g_ov_h) b = g_ov_h;
        if (g_sel_has) sel_fill(g_sel_prev.left, g_sel_prev.top, g_sel_prev.right, g_sel_prev.bottom, SEL_DIM);
        g_sel_prev.left = l; g_sel_prev.top = t; g_sel_prev.right = r; g_sel_prev.bottom = b;
        sel_paint_sel(l, t, r, b);
        g_sel_has = true;
        sel_push();
        return 0;
    }
    case WM_LBUTTONUP: {
        if (!g_sel_dragging) return 0;
        g_sel_dragging = false;
        ReleaseCapture();
        g_sel_result = g_sel_prev;
        if (g_sel_result.right - g_sel_result.left < 4) g_sel_cancel = true;
        if (g_sel_result.bottom - g_sel_result.top < 4) g_sel_cancel = true;
        g_sel_done = true;
        DestroyWindow(hwnd);
        return 0;
    }
    case WM_RBUTTONDOWN:
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE || msg == WM_RBUTTONDOWN) {
            g_sel_cancel = true;
            g_sel_done = true;
            if (g_sel_dragging) { g_sel_dragging = false; ReleaseCapture(); }
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
return DefWindowProcA(hwnd, msg, wp, lp);
}

static HDC g_cap_dc = NULL;
static HBITMAP g_cap_bmp = NULL;
static HGDIOBJ g_cap_old = NULL;
static int g_cap_vx = 0, g_cap_vy = 0, g_cap_w = 0, g_cap_h = 0;

static void shot_capture_release(void) {
    if (g_cap_dc) {
        if (g_cap_old) SelectObject(g_cap_dc, g_cap_old);
        DeleteDC(g_cap_dc);
        g_cap_dc = NULL;
    }
    if (g_cap_bmp) { DeleteObject(g_cap_bmp); g_cap_bmp = NULL; }
    g_cap_old = NULL;
}

static bool shot_capture_virtual(void) {
    shot_capture_release();
    g_cap_vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    g_cap_vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    g_cap_w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    g_cap_h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (g_cap_w <= 0 || g_cap_h <= 0) return false;

    HDC screen = GetDC(NULL);
    if (!screen) return false;

    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = g_cap_w;
    bi.bmiHeader.biHeight = -g_cap_h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = NULL;
    g_cap_dc = CreateCompatibleDC(screen);
    if (!g_cap_dc) { ReleaseDC(NULL, screen); return false; }
    g_cap_bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!g_cap_bmp || !bits) {
        if (g_cap_bmp) DeleteObject(g_cap_bmp);
        g_cap_bmp = NULL;
        DeleteDC(g_cap_dc);
        g_cap_dc = NULL;
        ReleaseDC(NULL, screen);
        printf("screenshot: virtual snapshot failed %dx%d err=%lu\n", g_cap_w, g_cap_h, GetLastError());
        return false;
    }
    g_cap_old = SelectObject(g_cap_dc, g_cap_bmp);
    BitBlt(g_cap_dc, 0, 0, g_cap_w, g_cap_h, screen, g_cap_vx, g_cap_vy, SRCCOPY);
    GdiFlush();
    ReleaseDC(NULL, screen);
    printf("screenshot: snapshotted virtual desktop %dx%d at (%d,%d)\n", g_cap_w, g_cap_h, g_cap_vx, g_cap_vy);
    return true;
}

static bool shot_select_area(RECT* out) {
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
if (vw <= 0 || vh <= 0) return false;
    if (!shot_capture_virtual()) printf("screenshot: continuing without snapshot\n");

    static LONG reg = 0;
    if (InterlockedCompareExchange(&reg, 1, 0) == 0) {
        WNDCLASSA wc;
        memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = ShotSelProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_CROSS);
        wc.lpszClassName = SHOT_SEL_CLASS;
        RegisterClassA(&wc);
    }

    g_sel_done = false;
g_sel_cancel = false;
    g_sel_dragging = false;
    g_sel_has = false;

    g_ov_w = vw;
    g_ov_h = vh;
    HDC screen = GetDC(NULL);
    if (!screen) return false;
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = vw;
    bi.bmiHeader.biHeight = -vh;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    g_ov_dc = CreateCompatibleDC(screen);
    if (!g_ov_dc) { ReleaseDC(NULL, screen); return false; }
    g_ov_bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &g_ov_bits, NULL, 0);
    ReleaseDC(NULL, screen);
    if (!g_ov_bmp || !g_ov_bits) {
        printf("screenshot: overlay DIB failed %dx%d err=%lu\n", vw, vh, GetLastError());
        shot_ov_release();
        return false;
    }
    SelectObject(g_ov_dc, g_ov_bmp);
    sel_fill(0, 0, vw, vh, SEL_DIM);

    HWND hwnd = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        SHOT_SEL_CLASS, NULL, WS_POPUP,
        vx, vy, vw, vh, NULL, NULL, GetModuleHandle(NULL), NULL);
    if (!hwnd) { printf("screenshot: overlay create failed err=%lu\n", GetLastError()); shot_ov_release(); return false; }
    g_ov_hwnd = hwnd;

    ShowWindow(hwnd, SW_SHOW);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    sel_push();

    MSG msg;
    while (!g_sel_done) {
        if (!GetMessage(&msg, NULL, 0, 0)) break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    g_ov_hwnd = NULL;
    shot_ov_release();

    if (g_sel_cancel) return false;
    if (g_sel_result.right - g_sel_result.left < 2) return false;
    if (g_sel_result.bottom - g_sel_result.top < 2) return false;
    g_sel_result.left += vx;
    g_sel_result.right += vx;
    g_sel_result.top += vy;
    g_sel_result.bottom += vy;
    if (out) *out = g_sel_result;
    return true;
}

static void shot_ov_release(void) {
    if (g_ov_dc) { DeleteDC(g_ov_dc); g_ov_dc = NULL; }
    if (g_ov_bmp) { DeleteObject(g_ov_bmp); g_ov_bmp = NULL; }
    g_ov_bits = NULL;
}

static bool pk_capture_rect(const RECT* r, const char* path) {
    if (!r || !path || !path[0]) return false;
    if (!shot_gdiplus_start()) return false;

    int w = r->right - r->left;
    int h = r->bottom - r->top;
    if (w <= 0 || h <= 0) return false;

    HDC hScreenDC = GetDC(NULL);
    if (!hScreenDC) return false;

    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = NULL;
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    if (!hMemoryDC) { printf("screenshot: CreateCompatibleDC failed\n"); ReleaseDC(NULL, hScreenDC); return false; }
    HBITMAP hBitmap = CreateDIBSection(hScreenDC, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!hBitmap || !bits) {
        printf("screenshot: CreateDIBSection failed err=%lu for %dx%d\n", GetLastError(), w, h);
        if (hBitmap) DeleteObject(hBitmap);
        DeleteDC(hMemoryDC);
        ReleaseDC(NULL, hScreenDC);
        return false;
    }

    HGDIOBJ hOldBitmap = SelectObject(hMemoryDC, hBitmap);
    if (g_cap_dc && g_cap_bmp) {
        BitBlt(hMemoryDC, 0, 0, w, h, g_cap_dc, r->left - g_cap_vx, r->top - g_cap_vy, SRCCOPY);
    } else {
        BitBlt(hMemoryDC, 0, 0, w, h, hScreenDC, r->left, r->top, SRCCOPY);
    }
    GdiFlush();
    SelectObject(hMemoryDC, hOldBitmap);

    GpBitmap* pBitmap = NULL;
    GpStatus st = GdipCreateBitmapFromHBITMAP(hBitmap, NULL, &pBitmap);
    bool saved = false;
    if (st != Ok || !pBitmap) {
        printf("screenshot: GdipCreateBitmapFromScan0 status=%d\n", (int)st);
    } else {
        UINT num = 0, size = 0;
        if (GdipGetImageEncodersSize(&num, &size) != Ok || num == 0 || size == 0) {
            printf("screenshot: GdipGetImageEncodersSize failed\n");
        } else {
            ImageCodecInfo* encoders = (ImageCodecInfo*)malloc(size);
            if (!encoders) {
                printf("screenshot: encoder alloc failed\n");
            } else if (GdipGetImageEncoders(num, size, encoders) != Ok) {
                printf("screenshot: GdipGetImageEncoders failed\n");
            } else {
                for (UINT j = 0; j < num; j++) {
                    if (wcscmp(encoders[j].MimeType, L"image/png") != 0) continue;
                    wchar_t wpath[MAX_PATH * 4];
                    int wlen = MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, (int)(sizeof(wpath) / sizeof(wpath[0])));
                    if (wlen <= 0) { printf("screenshot: path widen failed\n"); break; }
                    GpStatus sv = GdipSaveImageToFile((GpImage*)pBitmap, wpath, &encoders[j].Clsid, NULL);
                    if (sv == Ok) saved = true;
                    else printf("screenshot: GdipSaveImageToFile status=%d path='%s'\n", (int)sv, path);
                    break;
                }
            }
            free(encoders);
        }
        GdipDisposeImage((GpImage*)pBitmap);
    }

    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);
    if (!saved) remove(path);
    return saved;
}

static bool shot_dir_ok(const char* dir) {
    if (!dir || !dir[0]) return false;
    DWORD a = GetFileAttributesA(dir);
    if (a == INVALID_FILE_ATTRIBUTES) return false;
    if (!(a & FILE_ATTRIBUTE_DIRECTORY)) return false;
    return true;
}

static void shot_stage_path(char* out, size_t cap, const char* leaf) {
    char* wd = fl_get_working_dir();
    if (wd && wd[0] && shot_dir_ok(wd)) {
        snprintf(out, cap, "%s/%s", wd, leaf);
        free(wd);
        return;
    }
    if (wd && wd[0]) printf("screenshot: working dir '%s' missing, using TEMP\n", wd);
    free(wd);
    const char* t = getenv("TEMP");
    if (!t || !t[0]) t = getenv("TMP");
    if (!t || !t[0]) t = "C:/Windows/Temp";
    if (!shot_dir_ok(t)) t = "C:/Windows/Temp";
    snprintf(out, cap, "%s/%s", t, leaf);
}

static void pk_screenshot_copy_area(void) {
    RECT r;
    if (!shot_select_area(&r)) { printf("screenshot: no area selected\n"); return; }
    char path[MAX_PATH * 2];
    shot_stage_path(path, sizeof(path), "flint_clip.png");
    if (!pk_capture_rect(&r, path)) return;
    printf("screenshot: captured %dx%d at (%ld,%ld)\n",
           (int)(r.right - r.left), (int)(r.bottom - r.top), (long)r.left, (long)r.top);
    fl_copy_file_path(path);
    shot_capture_release();
}

static void pk_ocr_area_to_clipboard(void) {
    RECT full;
    HDC sc = GetDC(NULL);
    if (!sc) return;
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    full.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    full.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    full.right = full.left + vw;
    full.bottom = full.top + vh;
    ReleaseDC(NULL, sc);
    printf("screenshot: ocr capture %dx%d\n", vw, vh);

    char png[MAX_PATH * 2];
    shot_stage_path(png, sizeof(png), "flint_ocr.png");
    if (!pk_capture_rect(&full, png)) return;

    char cmd[2 * MAX_PATH + 64];
    snprintf(cmd, sizeof(cmd), "tesseract \"%s\" stdout -l eng", png);
    char* out = fl_run_terminal_command_blocking(cmd);
    remove(png);
    if (!out || !out[0]) { free(out); return; }

    size_t n = strlen(out);
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r')) out[--n] = '\0';
    if (out[0]) fl_copy_to_clipboard(out);
    free(out);
}

#endif

FLINT_FEATURE_INIT {
    fl_register_hotkey("Ctrl+Shift+X", pk_screenshot_copy_area);
    fl_register_hotkey("Alt+Shift+C", pk_ocr_area_to_clipboard);
}
