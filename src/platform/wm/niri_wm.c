#include "wm/niri_wm.h"
#include "deps/log.h"
#include "config.h"
#include "core/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_flag.h"
#include "helpers/helpers_text.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <glob.h>
#include <time.h>
#include <sys/stat.h>

static bool niri_sock_live(const char* sock) {
    if (!sock || !sock[0]) return false;
    struct stat st;
    return stat(sock, &st) == 0;
}

bool niri_is_active(void) {
    const char* cur = getenv("NIRI_SOCKET");
    if (niri_sock_live(cur)) return true;
    if (cur && cur[0]) unsetenv("NIRI_SOCKET");
    const char* rtd = getenv("XDG_RUNTIME_DIR");
    if (!rtd || !rtd[0]) return false;
    char pat[512];
    snprintf(pat, sizeof(pat), "%s/niri*.sock", rtd);
    glob_t g;
    memset(&g, 0, sizeof(g));
    if (glob(pat, 0, NULL, &g) != 0 || g.gl_pathc == 0) {
        globfree(&g);
        return false;
    }
    const char* best = NULL;
    time_t best_mtime = 0;
    for (size_t i = 0; i < g.gl_pathc; i++) {
        struct stat st;
        if (stat(g.gl_pathv[i], &st) != 0) continue;
        if (!best || st.st_mtime >= best_mtime) {
            best = g.gl_pathv[i];
            best_mtime = st.st_mtime;
        }
    }
    if (best) {
        setenv("NIRI_SOCKET", best, 1);
        log_error("niri detected sock='%s'", best);
        globfree(&g);
        return true;
    }
    globfree(&g);
    return false;
}

bool get_title_niri(char *buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) return false;
    buffer[0] = '\0';
    if (!niri_is_active()) return false;
    return false;
}

bool get_app_id_niri(char *buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) return false;
    buffer[0] = '\0';
    if (!niri_is_active()) return false;
    return false;
}

char* paste_niri(char* content) {
    if (!content) return NULL;
    if (!niri_is_active()) return NULL;
    return NULL;
}

bool focus_window_title_niri(char* title) {
    if (!title || !*title) return false;
    if (!niri_is_active()) return false;
    return false;
}

Fl_WindowList get_window_titles_niri() {
    Fl_WindowList list = {0};
    if (!niri_is_active()) return list;
    return list;
}

Fl_WindowList get_window_names_niri() {
    Fl_WindowList list = {0};
    if (!niri_is_active()) return list;
    return list;
}

Fl_Pos get_cursor_position_niri(void) {
    if (!niri_is_active()) return (Fl_Pos){0, 0};
    return (Fl_Pos){0, 0};
}

bool close_active_window_niri(void) {
    if (!niri_is_active()) return false;
    return false;
}

bool toggle_fullscreen_active_niri(void) {
    if (!niri_is_active()) return false;
    return false;
}

bool move_active_window_to_monitor_niri(bool right) {
    (void)right;
    if (!niri_is_active()) return false;
    return false;
}

bool move_active_window_left_niri(void) {
    if (!niri_is_active()) return false;
    return false;
}

bool move_active_window_right_niri(void) {
    if (!niri_is_active()) return false;
    return false;
}

bool focus_window_left_niri(void) {
    if (!niri_is_active()) return false;
    return false;
}

bool focus_window_right_niri(void) {
    if (!niri_is_active()) return false;
    return false;
}

bool minimize_active_window_niri(void) {
    if (!niri_is_active()) return false;
    return false;
}
