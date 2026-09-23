#include "core/platform.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char g_default_apps_x11[DEFAULT_APP_COUNT][256];

const char* default_apps_x11_run(const char* command, int slot) {
    char* out = Platform_RunTerminalCommand((char*)command, false);
    g_default_apps_x11[slot][0] = '\0';
    if (out) {
        sscanf(out, "%255s", g_default_apps_x11[slot]);
        free(out);
    }
    return g_default_apps_x11[slot];
}
const char* get_default_terminal_x11(int slot) {
    char* list_of_common_terminals[] = {
        "ptyxis",
        "gnome-console",
        "kgx",
        "gnome-terminal",
        "konsole",
        "xfce4-terminal",
        "mate-terminal",
        "lxterminal",
        "qterminal",
        "yakuake",
        "st",
        "stterm",
        "sakura",
        "zutty",
        "urxvt",
        "rxvt-unicode",
        "xterm",
        "alacritty",
        "kitty",
        "foot",
        "wezterm",
        "terminator",
        "tilix",
        "x-terminal-emulator",
    };
    g_default_apps_x11[slot][0] = '\0';
    char* out = "";
    out = getenv("TERMINAL") ? getenv("TERMINAL") : NULL;

    if (!out) {
      out = Platform_RunTerminalCommand("gsettings get org.gnome.desktop.default-applications.terminal exec",false);
      // Remove quotes from the output temporary
    }
    if (!out) {

    }

    if (out) {
        sscanf(out, "%255s", g_default_apps_x11[slot]);
        free(out);
        return g_default_apps_x11[slot];
    }else {
      //temporary
      printf("Could not find default terminal");
      exit(1);
    }
}

const char* Platform_GetDefaultApp(DefaultAppKind kind) {
    if (kind < 0 || kind >= DEFAULT_APP_COUNT) return "";
    if (kind == DEFAULT_BROWSER) return default_apps_x11_run("xdg-settings get default-web-browser", DEFAULT_BROWSER);
    if (kind == DEFAULT_IMAGE_VIEWER) return default_apps_x11_run("xdg-mime query default image/png", DEFAULT_IMAGE_VIEWER);
    if (kind == DEFAULT_VIDEO_PLAYER) return default_apps_x11_run("xdg-mime query default video/mp4", DEFAULT_VIDEO_PLAYER);
    if (kind == DEFAULT_TERMINAL )    return default_apps_x11_run("xdg-mime query default video/mp4", DEFAULT_TERMINAL);
    g_default_apps_x11[kind][0] = '\0';
    return g_default_apps_x11[kind];
}

int default_apps_x11_exec_token(const char* line, char* out, size_t cap) {
    const char* p = line + 5;
    while (*p == ' ' || *p == '\t') p++;
    int quoted = (*p == '"');
    if (quoted) p++;
    size_t n = 0;
    while (*p && *p != '\n' && n + 1 < cap) {
        if (quoted && *p == '"') break;
        if (!quoted && (*p == ' ' || *p == '\t')) break;
        out[n++] = *p++;
    }
    out[n] = '\0';
    return n > 0;
}

bool Platform_ResolveDesktopExec(const char* desktop_id, char* out, size_t out_size) {
    if (!desktop_id || !desktop_id[0] || !out || out_size == 0) return false;
    if (strchr(desktop_id, '/')) return false;
    out[0] = '\0';
    char dirs[1024];
    const char* data_home = getenv("XDG_DATA_HOME");
    const char* data_dirs = getenv("XDG_DATA_DIRS");
    if (!data_dirs || !data_dirs[0]) data_dirs = "/usr/local/share:/usr/share";
    if (data_home && data_home[0]) snprintf(dirs, sizeof(dirs), "%s:%s", data_home, data_dirs);
    else snprintf(dirs, sizeof(dirs), "%s/.local/share:%s", getenv("HOME") ? getenv("HOME") : "/root", data_dirs);
    char* save = NULL;
    for (char* d = strtok_r(dirs, ":", &save); d; d = strtok_r(NULL, ":", &save)) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/applications/%s", d, desktop_id);
        FILE* f = fopen(path, "r");
        if (!f) continue;
        char line[1024];
        bool found = false;
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "Exec=", 5) == 0) {
                found = default_apps_x11_exec_token(line, out, out_size);
                break;
            }
        }
        fclose(f);
        if (found) return true;
        out[0] = '\0';
    }
    return false;
}
