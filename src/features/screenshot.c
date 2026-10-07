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
    fl_copy_image_from_path(path);
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

static void pk_screenshot_copy_area(void) {

}
static void pk_ocr_area_to_clipboard(void) {

}

#endif

FLINT_FEATURE_INIT {
    fl_register_hotkey("Ctrl+Shift+X", pk_screenshot_copy_area);
    fl_register_hotkey("Alt+Shift+C", pk_ocr_area_to_clipboard);
}
