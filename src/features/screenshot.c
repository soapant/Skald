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
#include <objidl.h>
#include <gdiplus.h>

static ULONG_PTR g_shot_gdiplus = 0;

static bool shot_gdiplus_start(void) {
    if (g_shot_gdiplus) return true;
    GdiplusStartupInput in;
    memset(&in, 0, sizeof(in));
    if (GdiplusStartup(&g_shot_gdiplus, &in, NULL) != Ok) { g_shot_gdiplus = 0; return false; }
    return true;
}

static bool pk_capture_to(const char* path) {
    if (!path || !path[0]) return false;
    if (!shot_gdiplus_start()) return false;

    int cx = GetSystemMetrics(SM_CXSCREEN);
    int cy = GetSystemMetrics(SM_CYSCREEN);
    if (cx <= 0 || cy <= 0) return false;

    HDC hScreenDC = GetDC(NULL);
    if (!hScreenDC) return false;
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap = hMemoryDC ? CreateCompatibleBitmap(hScreenDC, cx, cy) : NULL;
    HGDIOBJ hOldBitmap = NULL;
    GpBitmap* pBitmap = NULL;
    bool saved = false;

    if (hBitmap) {
        hOldBitmap = SelectObject(hMemoryDC, hBitmap);
        BitBlt(hMemoryDC, 0, 0, cx, cy, hScreenDC, 0, 0, SRCCOPY);
        if (GdipCreateBitmapFromHBITMAP(hBitmap, NULL, &pBitmap) == Ok && pBitmap) {
            UINT num = 0, size = 0;
            if (GdipGetImageEncodersSize(&num, &size) == Ok && num > 0 && size > 0) {
                ImageCodecInfo* encoders = (ImageCodecInfo*)malloc(size);
                if (encoders) {
                    if (GdipGetImageEncoders(num, size, encoders) == Ok) {
                        for (UINT j = 0; j < num; j++) {
                            if (wcscmp(encoders[j].MimeType, L"image/png") != 0) continue;
                            wchar_t wpath[MAX_PATH * 2];
                            int wlen = MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, (int)(sizeof(wpath) / sizeof(wpath[0])));
                            if (wlen > 0 && GdipSaveImageToFile((GpImage*)pBitmap, wpath, &encoders[j].Clsid, NULL) == Ok) saved = true;
                            break;
                        }
                    }
                    free(encoders);
                }
            }
            GdipDisposeImage((GpImage*)pBitmap);
        }
    }

    if (hOldBitmap) SelectObject(hMemoryDC, hOldBitmap);
    if (hBitmap) DeleteObject(hBitmap);
    if (hMemoryDC) DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);
    if (!saved) remove(path);
    return saved;
}

static void pk_screenshot_copy_area(void) {
    char path[MAX_PATH];
    char* wd = fl_get_working_dir();
    snprintf(path, sizeof(path), "%s/flint_clip.png", (wd && wd[0]) ? wd : ".");
    free(wd);
    if (!pk_capture_to(path)) return;
    fl_copy_file_path(path);
}

static void pk_ocr_area_to_clipboard(void) {
    char png[MAX_PATH];
    char* wd = fl_get_working_dir();
    snprintf(png, sizeof(png), "%s/flint_ocr.png", (wd && wd[0]) ? wd : ".");
    free(wd);
    if (!pk_capture_to(png)) return;

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
