#include "platform/platform.h"
#include "helpers/helpers_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>

#include "core/timer.h"
const char* x11_get_login_shell(void) {
        PROFILE_START(_pt);
struct passwd* pw = getpwuid(getuid());
    if (!pw || !pw->pw_shell || !pw->pw_shell[0]) {
        return NULL;
    }
    { PROFILE_END_VAR(_pt, "x11_get_login_shell"); return pw->pw_shell; } 
}

char default_apps_x11[DEFAULT_APP_COUNT][256];

const char* x11_default_apps_run(const char* command, int slot) {
        PROFILE_START(_pt);
char* out = Platform_RunTerminalCommand((char*)command, false);
    default_apps_x11[slot][0] = '\0';
    if (out) {
        sscanf(out, "%255s", default_apps_x11[slot]);
        free(out);
    }
    { PROFILE_END_VAR(_pt, "x11_default_apps_run"); return default_apps_x11[slot]; }
}

#define APPDIR_CACHE_SLOTS 8
static ListOfFiles g_ad_cache[APPDIR_CACHE_SLOTS];
static char g_ad_path[APPDIR_CACHE_SLOTS][1024];
static bool g_ad_valid[APPDIR_CACHE_SLOTS];

static ListOfFiles* x11_applications_dir_files(const char* dir) {
        PROFILE_START(_pt);
static int next_slot = 0;
    for (int i = 0; i < APPDIR_CACHE_SLOTS; i++) {
        if (g_ad_valid[i] && strcmp(g_ad_path[i], dir) == 0) return &g_ad_cache[i];
    }
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) { PROFILE_END_VAR(_pt, "x11_applications_dir_files"); return NULL; }
    int slot = next_slot;
    next_slot = (next_slot + 1) % APPDIR_CACHE_SLOTS;
    if (g_ad_valid[slot]) {
        free(g_ad_cache[slot].files);
        g_ad_cache[slot].files = NULL;
        g_ad_cache[slot].count = 0;
        g_ad_cache[slot].capacity = 0;
    }
    Platform_GetAllFilesFromDir(dir, NULL, &g_ad_cache[slot], NULL, true);
    snprintf(g_ad_path[slot], sizeof(g_ad_path[slot]), "%s", dir);
    g_ad_valid[slot] = true;
    { PROFILE_END_VAR(_pt, "x11_applications_dir_files"); return &g_ad_cache[slot]; }
}

static const char* match_in_applications_dir(const char* dir, const char* const* names, size_t count) {
        PROFILE_START(_pt);
ListOfFiles* files = x11_applications_dir_files(dir);
    if (!files) { PROFILE_END_VAR(_pt, "match_in_applications_dir"); return NULL; }
    const char* found = NULL;
    for (size_t i = 0; i < files->count && !found; i++) {
        for (size_t j = 0; j < count; j++) {
            if (Platform_StringContainsCaseInsensitive(files->files[i].name, names[j])) {
                found = names[j];
                break;
            }
        }
    }
    { PROFILE_END_VAR(_pt, "match_in_applications_dir"); return found; }
}

void x11_default_apps_free_cache(void) {
        PROFILE_START(_pt);
for (int i = 0; i < APPDIR_CACHE_SLOTS; i++) {
        if (!g_ad_valid[i]) continue;
        free(g_ad_cache[i].files);
        g_ad_cache[i].files = NULL;
        g_ad_cache[i].count = 0;
        g_ad_cache[i].capacity = 0;
        g_ad_path[i][0] = '\0';
        g_ad_valid[i] = false;
    }
}

const char* x11_find_terminal_in_desktop_files(const char* const* names, size_t count) {
        PROFILE_START(_pt);
char dir[1024];
    const char* home = getenv("HOME");
    if (home && home[0]) {
        snprintf(dir, sizeof(dir), "%s/.local/share/applications", home);
        const char* hit = match_in_applications_dir(dir, names, count);
        if (hit) return hit;
    }
    const char* data_dirs = getenv("XDG_DATA_DIRS");
    if (!data_dirs || !data_dirs[0]) data_dirs = "/usr/local/share:/usr/share";
    char buf[4096];
    snprintf(buf, sizeof(buf), "%s", data_dirs);
    char* save = buf;
    for (char* tok = strtok_r(buf, ":", &save); tok; tok = strtok_r(NULL, ":", &save)) {
        snprintf(dir, sizeof(dir), "%s/applications", tok);
        const char* hit = match_in_applications_dir(dir, names, count);
        if (hit) return hit;
    }
    { PROFILE_END_VAR(_pt, "x11_find_terminal_in_desktop_files"); return match_in_applications_dir("/usr/share/applications", names, count); }
}


const char* x11_get_default_terminal_wayland(void) {
        PROFILE_START(_pt);
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
    { PROFILE_END_VAR(_pt, "x11_get_default_terminal_wayland"); return out ? out : NULL; }
}


const char* x11_get_default_terminal_x11(void) {
        PROFILE_START(_pt);
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
    const char* out = Platform_RunTerminalCommand("gsettings get org.gnome.desktop.default-applications.terminal exec",false);
    char trimmed[256];
    trimmed[0] = '\0';
    if (out) {
        const char* p = out;
        while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
        if (*p == '\'') p++;
        snprintf(trimmed, sizeof(trimmed), "%s", p);
        char* end = trimmed + strlen(trimmed);
        while (end > trimmed && (end[-1] == '\'' || end[-1] == ' ' || end[-1] == '\n' || end[-1] == '\r')) *--end = '\0';
        free(out);
        if (strchr(trimmed, ' ') || strchr(trimmed, '/') || strchr(trimmed, ':')) {
            trimmed[0] = '\0';
        }
        for (size_t i = 0; trimmed[0]; i++) {
            if (!isprint((unsigned char)trimmed[i])) trimmed[0] = '\0';
        }
    }
    if (trimmed[0]) { PROFILE_END_VAR(_pt, "x11_get_default_terminal_x11"); return trimmed; }
    { PROFILE_END_VAR(_pt, "x11_get_default_terminal_x11"); return x11_find_terminal_in_desktop_files(candidates, n); }
}


const char* get_default_terminal(int slot) {
        PROFILE_START(_pt);
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
    { PROFILE_END_VAR(_pt, "get_default_terminal"); return default_apps_x11[slot]; }
}
const char* get_default_browser(int slot) {
        PROFILE_START(_pt);
default_apps_x11[slot][0] = '\0';

    const char *env = getenv("BROWSER");
    if (env && env[0]) {
        snprintf(default_apps_x11[slot], sizeof(default_apps_x11[slot]), "%s", env);
        return default_apps_x11[slot];
    }

    StringList l = File_ReadLines("~/.config/mimeapps.list", 65536);
    for (size_t i = 0; i < l.count; i++) {
        const char *line = l.items[i];
        if (!Platform_StringContainsCaseInsensitive(line, "text/html")) continue;
        char *value = String_CutAfter(line, '=');
        if (!value) break;
        char *first = String_CutBefore(value, ';');
        free(value);
        if (!first) break;
        char *trimmed = String_Trim(first);
        free(first);
        if (trimmed) {
            char *stem = String_CutBefore(trimmed, '.');
            if (stem) {
                if (stem[0]) snprintf(default_apps_x11[slot], sizeof(default_apps_x11[slot]), "%s", stem);
                free(stem);
            }
            free(trimmed);
        }
        break;
    }
    StringList_Free(&l);

    { PROFILE_END_VAR(_pt, "get_default_browser"); return default_apps_x11[slot]; }
}

const char* get_default_shell(int slot) {
        PROFILE_START(_pt);
//temporary maybe in future just use the terminal config or somethign idk
    default_apps_x11[slot][0] = '\0';
    const char* shell = getenv("SHELL");
    const char* name = strrchr(shell, '/') ? strrchr(shell, '/') + 1 : shell;
    snprintf(default_apps_x11[slot], sizeof(default_apps_x11[slot]), "%s", name);
    { PROFILE_END_VAR(_pt, "get_default_shell"); return default_apps_x11[slot]; }
}

const char* Platform_GetDefaultApp(DefaultAppKind kind) {
        PROFILE_START(_pt);
if (kind < 0 || kind >= DEFAULT_APP_COUNT) { PROFILE_END_VAR(_pt, "Platform_GetDefaultApp"); return ""; }
    if (kind == DEFAULT_BROWSER) { PROFILE_END_VAR(_pt, "Platform_GetDefaultApp"); return get_default_browser(DEFAULT_BROWSER); }
    if (kind == DEFAULT_TERMINAL )    { PROFILE_END_VAR(_pt, "Platform_GetDefaultApp"); return get_default_terminal(DEFAULT_TERMINAL); }
    if (kind == DEFAULT_SHELL )       { PROFILE_END_VAR(_pt, "Platform_GetDefaultApp"); return get_default_shell(DEFAULT_SHELL); }
    default_apps_x11[kind][0] = '\0';
    { PROFILE_END_VAR(_pt, "Platform_GetDefaultApp"); return default_apps_x11[kind]; }
}

int x11_default_apps_exec_token(const char* line, char* out, size_t cap) {
        PROFILE_START(_pt);
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
    { PROFILE_END_VAR(_pt, "x11_default_apps_exec_token"); return n > 0; }
}

bool Platform_ResolveDesktopExec(const char* desktop_id, char* out, size_t out_size) {
        PROFILE_START(_pt);
if (!desktop_id || !desktop_id[0] || !out || out_size == 0) { PROFILE_END_VAR(_pt, "Platform_ResolveDesktopExec"); return false; }
    if (strchr(desktop_id, '/')) { PROFILE_END_VAR(_pt, "Platform_ResolveDesktopExec"); return false; }
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
        static char content[16384];
        if (File_ReadAll(path, content, (long)sizeof(content)) <= 0) continue;
        bool found = false;
        char* cur = content;
        while (*cur) {
            char* nl = strchr(cur, '\n');
            if (nl) *nl = '\0';
            char* line = cur;
            cur = nl ? nl + 1 : cur + strlen(cur);
            if (strncmp(line, "Exec=", 5) == 0) {
                found = x11_default_apps_exec_token(line, out, out_size);
                break;
            }
        }
        if (found) return true;
        out[0] = '\0';
    }
    { PROFILE_END_VAR(_pt, "Platform_ResolveDesktopExec"); return false; }
}
