#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>

const char* x11_get_login_shell(void) {
    struct passwd* pw = getpwuid(getuid());
    if (!pw || !pw->pw_shell || !pw->pw_shell[0]) {
        return NULL;
    }
    return pw->pw_shell; 
}

char default_apps_x11[DEFAULT_APP_COUNT][256];

const char* x11_default_apps_run(const char* command, int slot) {
    char* out = Platform_RunTerminalCommand((char*)command, false);
    default_apps_x11[slot][0] = '\0';
    if (out) {
        sscanf(out, "%255s", default_apps_x11[slot]);
        free(out);
    }
    return default_apps_x11[slot];
}

const char* x11_find_terminal_in_desktop_files(const char* const* names, size_t count) {
    ListOfFiles files = {0};
    Platform_GetAllFilesFromDir("/usr/share/applications", NULL, &files, NULL, true);

    const char* found = NULL;

    for (size_t j = 0; j < count && !found; j++) {
        const char* term = names[j];
        for (int i = 0; i < files.count; i++) {
            const char* desktop_file = files.files[i].name;

            if (Platform_StringContainsCaseInsensitive(desktop_file, term)) {
                found = term;
                break;
            }
        }
    }

    return found;
}


const char* x11_get_default_terminal_wayland(void) {
    const char* const candidates[] = {
        "ptyxis",
        "gnome-console",
        "foot",
        "gnome-terminal",
        "konsole",
        "alacritty",
        "kitty",
        "wezterm",
        "xfce4-terminal",
        "mate-terminal",
        "lxterminal",
        "qterminal",
        "yakuake",
    };
    size_t n = sizeof(candidates) / sizeof(candidates[0]);
    const char* out = NULL;
    if (!out) {
      out = x11_find_terminal_in_desktop_files(candidates, n);
    }
    return out ? out : NULL;
}


const char* x11_get_default_terminal_x11(void) {
    const char* const candidates[] = {
        "gnome-terminal",
        "konsole",
        "foot",
        "alacritty",
        "kitty",
        "wezterm",
        "xfce4-terminal",
        "mate-terminal",
        "lxterminal",
        "qterminal",
        "yakuake",
        "terminator",
        "tilix",
        "st",
        "stterm",
        "sakura",
        "zutty",
        "urxvt",
        "rxvt-unicode",
        "xterm",
        "x-terminal-emulator",
    };
    size_t n = sizeof(candidates) / sizeof(candidates[0]);
    const char* out;
    out = Platform_RunTerminalCommand("gsettings get org.gnome.desktop.default-applications.terminal exec",false);
    // Remove quotes from the output temporary
    if (!out) {
      out = x11_find_terminal_in_desktop_files(candidates, n);
    }
    return out ? out : NULL;
}


const char* get_default_terminal(int slot) {
    default_apps_x11[slot][0] = '\0';

    const char* term = NULL;

    const char* env = getenv("TERMINAL");
    if (env && env[0]) {
        term = env;
    }

    if (!term) {
        const char* st = getenv("XDG_SESSION_TYPE");
        bool is_wayland = (st && strcmp(st, "wayland") == 0) ||
                          getenv("WAYLAND_DISPLAY") != NULL ||
                          getenv("SWAYSOCK") != NULL || getenv("I3SOCK") != NULL ||
                          getenv("HYPRLAND_INSTANCE_SIGNATURE") != NULL ||
                          getenv("NIRI_SOCKET") != NULL || getenv("KDE_FULL_SESSION") != NULL;
        term = is_wayland ? x11_get_default_terminal_wayland()
                          : x11_get_default_terminal_x11();
    }

    if (!term) {
      return NULL;
    }

    snprintf(default_apps_x11[slot], sizeof(default_apps_x11[slot]), "%s", term);
    return default_apps_x11[slot];
}

const char* x11_get_default_shell(int slot) {
    //temporary maybe in future just use the terminal config or somethign idk
    default_apps_x11[slot][0] = '\0';
    const char* shell = getenv("SHELL");
    const char* name = strrchr(shell, '/') ? strrchr(shell, '/') + 1 : shell;
    snprintf(default_apps_x11[slot], sizeof(default_apps_x11[slot]), "%s", name);
    return default_apps_x11[slot];
}

const char* Platform_GetDefaultApp(DefaultAppKind kind) {
    if (kind < 0 || kind >= DEFAULT_APP_COUNT) return "";
    if (kind == DEFAULT_BROWSER) return x11_default_apps_run("xdg-settings get default-web-browser", DEFAULT_BROWSER);
    if (kind == DEFAULT_IMAGE_VIEWER) return x11_default_apps_run("xdg-mime query default image/png", DEFAULT_IMAGE_VIEWER);
    if (kind == DEFAULT_VIDEO_PLAYER) return x11_default_apps_run("xdg-mime query default video/mp4", DEFAULT_VIDEO_PLAYER);
    if (kind == DEFAULT_TERMINAL )    return get_default_terminal(DEFAULT_TERMINAL);
    if (kind == DEFAULT_SHELL )       return x11_get_default_shell(DEFAULT_SHELL);
    default_apps_x11[kind][0] = '\0';
    return default_apps_x11[kind];
}

int x11_default_apps_exec_token(const char* line, char* out, size_t cap) {
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
                found = x11_default_apps_exec_token(line, out, out_size);
                break;
            }
        }
        fclose(f);
        if (found) return true;
        out[0] = '\0';
    }
    return false;
}
