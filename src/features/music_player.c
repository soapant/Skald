#include "core/flapi.h"
#include "core/context.h"
#include "helpers/helpers_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <X11/Xlib.h>
#include <X11/Xatom.h>
static Window music_find_window(Display* dpy, Window w, const char* title) {
    char* name = NULL;
    if (XFetchName(dpy, w, &name) && name != NULL) {
        bool match = (strcmp(name, title) == 0);
        XFree(name);
        if (match) return w;
    }
    Window root, parent, *children = NULL;
    unsigned int n = 0;
    if (XQueryTree(dpy, w, &root, &parent, &children, &n)) {
        for (unsigned int i = 0; i < n; i++) {
            Window found = music_find_window(dpy, children[i], title);
            if (found != None) {
                if (children) XFree(children);
                return found;
            }
        }
        if (children) XFree(children);
    }
    return None;
}
static bool music_set_string(const char* window_title, const char* atom_name, const char* value) {
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy || !window_title || !value) { if (dpy) XCloseDisplay(dpy); return false; }
    bool ok = false;
    Window win = music_find_window(dpy, DefaultRootWindow(dpy), window_title);
    if (win != None) {
        XChangeProperty(dpy, win, XInternAtom(dpy, atom_name, False), XA_STRING, 8, PropModeReplace, (const unsigned char*)value, (int)strlen(value));
        XFlush(dpy);
        ok = true;
    }
    XCloseDisplay(dpy);
    return ok;
}
static bool music_send_command(const char* window_title, const char* atom_name, long command) {
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy || !window_title) { if (dpy) XCloseDisplay(dpy); return false; }
    bool ok = false;
    Window win = music_find_window(dpy, DefaultRootWindow(dpy), window_title);
    if (win != None) {
        XEvent ev = {0};
        ev.xclient.type = ClientMessage;
        ev.xclient.window = win;
        ev.xclient.message_type = XInternAtom(dpy, atom_name, False);
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = command;
        if (XSendEvent(dpy, win, False, NoEventMask, &ev) != 0) {
            XFlush(dpy);
            ok = true;
        }
    }
    XCloseDisplay(dpy);
    return ok;
}
#endif

const char* MusicPlayerPath = "{DEV}/Music Player/music_player.exe";
const char* MusicPath = "{MUSIC}";

char *allowed_audio_formats[] = {"mp3","ogg","flac", NULL};

ListOfFiles s_music_files = {0};

void music_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    s_music_files.count = 0;
    fl_get_all_files_from_dir(MusicPath, allowed_audio_formats, &s_music_files, NULL);
    
    const char** items = (const char**)malloc(s_music_files.count * sizeof(char*));
    if (!items) return;
    for (size_t i = 0; i < s_music_files.count; i++) {
        items[i] = s_music_files.files[i].name;
    }
    PopulateListBox(lb, filter, items, (int)s_music_files.count, NULL);
    free(items);
}

bool music_enter_callback(const char* selection, ListBoxInfo* lb) {
    char* result = NULL;
    for (size_t i = 0; i < s_music_files.count; i++) {
        if (strcmp(s_music_files.files[i].name, selection) == 0) {
            result = s_music_files.files[i].path;
            break;
        }
    }
    if (!result) {
        ListOfFiles tmp = {0};
        fl_get_all_files_from_dir(MusicPath, allowed_audio_formats, &tmp, NULL);
        for (size_t i = 0; i < tmp.count; i++) {
            if (strcmp(tmp.files[i].name, selection) == 0) {
                result = tmp.files[i].path;
                break;
            }
        }
        if (result) {
            char found_path[MAX_PATH];
            strncpy(found_path, result, sizeof(found_path)-1);
            found_path[sizeof(found_path)-1] = '\0';
            result = found_path;
        }
        free(tmp.files);
    }
    
    if (!result) return true;
    
#if defined(_WIN32)
    HWND playerHwnd = FindWindow(NULL, "Music Player");
    if (playerHwnd) {
        COPYDATASTRUCT cds;
        cds.dwData = 0;
        cds.cbData = (DWORD)(strlen(result) + 1);
        cds.lpData = (PVOID)result;
        SendMessage(playerHwnd, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds);
        return true;
    } else {
        char quoted_result[MAX_PATH + 3] = {0};
        snprintf(quoted_result, sizeof(quoted_result), "\"%s\"", result);
        fl_launch_with_args(MusicPlayerPath, quoted_result);
    }
#else
    if (music_set_string("Music Player", "_MP_TRACK", result) &&
        music_send_command("Music Player", "_MP_TRACK_CHANGED", 1)) {
        return true;
    }
    if (strlen(MusicPlayerPath) > 0) {
        char quoted_result[MAX_PATH + 3] = {0};
        snprintf(quoted_result, sizeof(quoted_result), "\"%s\"", result);
        fl_launch_with_args(MusicPlayerPath, quoted_result);
    }
#endif
    return true;
}

void music_cmd_1(void) {
#if defined(_WIN32)
    HWND playerHwnd = FindWindow(NULL, "Music Player");
    if (playerHwnd) PostMessage(playerHwnd, WM_USER + 1, 1, 0);
#else
    music_send_command("Music Player", "_MP_PLAYER_CMD", 1);
#endif
}
void music_cmd_5(void) {
#if defined(_WIN32)
    HWND playerHwnd = FindWindow(NULL, "Music Player");
    if (playerHwnd) PostMessage(playerHwnd, WM_USER + 1, 5, 0);
#else
    music_send_command("Music Player", "_MP_PLAYER_CMD", 5);
#endif
}
void music_cmd_6(void) {
#if defined(_WIN32)
    HWND playerHwnd = FindWindow(NULL, "Music Player");
    if (playerHwnd) PostMessage(playerHwnd, WM_USER + 1, 6, 0);
#else
    music_send_command("Music Player", "_MP_PLAYER_CMD", 6);
#endif
}
void music_cmd_2(void) {
#if defined(_WIN32)
    HWND playerHwnd = FindWindow(NULL, "Music Player");
    if (playerHwnd) PostMessage(playerHwnd, WM_USER + 1, 2, 0);
#else
    music_send_command("Music Player", "_MP_PLAYER_CMD", 2);
#endif
}
void music_prompt(void) {
    PROMPT(.title = "🎵 Player",
           .refresh = music_refresh_callback,
           .enter = music_enter_callback);
}

FLINT_FEATURE_INIT {
fl_register_hotkey("Alt+,", music_cmd_1);
    fl_register_hotkey("Alt+/", music_cmd_5);
    fl_register_hotkey("Alt+;", music_cmd_6);
    fl_register_hotkey("Alt+'", music_cmd_2);
    fl_register_hotkey("Alt+P", music_prompt);

    fl_register_feature("Music Player", music_prompt);
}
