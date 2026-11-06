#include "core/flapi.h"
#include "core/context.h"
#include "helpers/helpers_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif
#if defined(__linux__)
#include <X11/Xlib.h>
#include <X11/Xatom.h>
static Window music_find_window(Display *dpy, Window w, const char *title) {
  char *name = NULL;
  if (XFetchName(dpy, w, &name) && name != NULL) {
    bool match = (strcmp(name, title) == 0);
    XFree(name);
    if (match)
      return w;
  }
  Window root, parent, *children = NULL;
  unsigned int n = 0;
  if (XQueryTree(dpy, w, &root, &parent, &children, &n)) {
    for (unsigned int i = 0; i < n; i++) {
      Window found = music_find_window(dpy, children[i], title);
      if (found != None) {
        if (children)
          XFree(children);
        return found;
      }
    }
    if (children)
      XFree(children);
  }
  return None;
}
static bool music_set_string(const char *window_title, const char *atom_name,
                             const char *value) {
  Display *dpy = XOpenDisplay(NULL);
  if (!dpy || !window_title || !value) {
    if (dpy)
      XCloseDisplay(dpy);
    return false;
  }
  bool ok = false;
  Window win = music_find_window(dpy, DefaultRootWindow(dpy), window_title);
  if (win != None) {
    XChangeProperty(dpy, win, XInternAtom(dpy, atom_name, False), XA_STRING, 8,
                    PropModeReplace, (const unsigned char *)value,
                    (int)strlen(value));
    XFlush(dpy);
    ok = true;
  }
  XCloseDisplay(dpy);
  return ok;
}
static bool music_send_command(const char *window_title, const char *atom_name,
                               long command) {
  Display *dpy = XOpenDisplay(NULL);
  if (!dpy || !window_title) {
    if (dpy)
      XCloseDisplay(dpy);
    return false;
  }
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

#if defined(_WIN32)
const char *MusicPlayerPath = "C:/Projects/Music Player/music_player.exe";
#elif defined(__linux__)
const char *MusicPlayerPath =
    "~/Programming/Projects/Music Player/music_player";
#endif
#if defined(_WIN32)
const char *MusicPath = "C:/Music";
#elif defined(__linux__)
const char *MusicPath = "~/Music";
#endif

char *allowed_audio_formats[] = {"mp3", "ogg", "flac", NULL};

ListOfFiles s_music_files = {0};
static const char *s_music_items[MAX_VISIBLE_ITEMS];

void music_refresh_callback(PromptState *state) {
  size_t n = s_music_files.count;
  if (n > MAX_VISIBLE_ITEMS)
    n = MAX_VISIBLE_ITEMS;
  for (size_t i = 0; i < n; i++) {
    s_music_items[i] = s_music_files.files[i].name;
  }
  fl_populate_listbox(state->lb, state->eb->filter, s_music_items, (int)n);
}
bool play_music(char *name) {
#if defined(_WIN32)
  HWND playerHwnd = FindWindow(NULL, "Music Player");
  if (playerHwnd) {
    COPYDATASTRUCT cds;
    cds.dwData = 0;
    cds.cbData = (DWORD)(strlen(name) + 1);
    cds.lpData = (PVOID)name;
    SendMessage(playerHwnd, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds);
    return true;
  } else {
    char quoted_name[MAX_PATH + 3] = {0};
    snprintf(quoted_name, sizeof(quoted_name), "\"%s\"", name);
    fl_launch_with_args(MusicPlayerPath, quoted_name);
  }
#elif defined(__linux__)
  if (music_set_string("Music Player", "_MP_TRACK", name) &&
      music_send_command("Music Player", "_MP_TRACK_CHANGED", 1)) {
    return true;
  }
  if (strlen(MusicPlayerPath) > 0) {
    char quoted_name[MAX_PATH + 3] = {0};
    snprintf(quoted_name, sizeof(quoted_name), "\"%s\"", name);
    fl_launch_with_args(MusicPlayerPath, quoted_name);
  }
#endif
  return true;
}

bool music_enter_callback(PromptState *state) {
  char *result = NULL;
  if (!fl_is_selected(state->lb)) {
    return false;
  }
  for (size_t i = 0; i < s_music_files.count; i++) {
    if (strcmp(s_music_files.files[i].name, state->lb->selected_text) == 0) {
      result = s_music_files.files[i].path;
      break;
    }
  }
  return play_music(result);
}

void music_cmd_1(void) {
#if defined(_WIN32)
  HWND playerHwnd = FindWindow(NULL, "Music Player");
  if (playerHwnd)
    PostMessage(playerHwnd, WM_USER + 1, 1, 0);
#elif defined(__linux__)
  music_send_command("Music Player", "_MP_PLAYER_CMD", 1);
#endif
}
void music_cmd_5(void) {
#if defined(_WIN32)
  HWND playerHwnd = FindWindow(NULL, "Music Player");
  if (playerHwnd)
    PostMessage(playerHwnd, WM_USER + 1, 5, 0);
#elif defined(__linux__)
  music_send_command("Music Player", "_MP_PLAYER_CMD", 5);
#endif
}
void music_cmd_6(void) {
#if defined(_WIN32)
  HWND playerHwnd = FindWindow(NULL, "Music Player");
  if (playerHwnd)
    PostMessage(playerHwnd, WM_USER + 1, 6, 0);
#elif defined(__linux__)
  music_send_command("Music Player", "_MP_PLAYER_CMD", 6);
#endif
}
void music_cmd_2(void) {
#if defined(_WIN32)
  HWND playerHwnd = FindWindow(NULL, "Music Player");
  if (playerHwnd)
    PostMessage(playerHwnd, WM_USER + 1, 2, 0);
#elif defined(__linux__)
  music_send_command("Music Player", "_MP_PLAYER_CMD", 2);
#endif
}

void random_music(void) {
  int idx = fl_rand_num_between(0, s_music_files.count - 1);
  play_music(s_music_files.files[idx].path);
}
void music_prompt(void) {
  fl_window(.title = "🎵 Player", .refresh = music_refresh_callback,
            .enter = music_enter_callback);
}
void reload_music(void) {
  s_music_files.count = 0;
  fl_get_all_files_from_dir(MusicPath, allowed_audio_formats, &s_music_files,
                            NULL, true, FL_DIR_WALK_UNLIMITED);
}
void add_music(void) {
  char *url = fl_popup(.placeholder = "Enter youtube url for the video");
  char *name = fl_popup(
          .placeholder =
              "Enter a custom name for this song (or leave blank for default)");
  if (!url)
    return;
  char cmd[512];

  if (name && strlen(name) > 0) {
    snprintf(cmd, sizeof(cmd),
             "yt-dlp -f \"ba\" -x --audio-format mp3 --audio-quality 0 -o "
             "\"%s/%s.%%(ext)s\" \"%s\"",
             MusicPath, name, url);
  } else {
    snprintf(cmd, sizeof(cmd),
             "yt-dlp -f \"ba\" -x --audio-format mp3 --audio-quality 0 -o "
             "\"%s/%%(title)s.%%(ext)s\" \"%s\"",
             MusicPath, url);
  }
  free(name);
  free(url);
  fl_run_terminal_command(cmd);
}

FLINT_FEATURE_INIT {
  reload_music();
  fl_register_hotkey(.hotkey = "Alt+,", .cb = music_cmd_1);
  fl_register_hotkey(.hotkey = "Alt+/", .cb = music_cmd_5);
  fl_register_hotkey(.hotkey = "Alt+;", .cb = music_cmd_6);
  fl_register_hotkey(.hotkey = "Alt+'", .cb = music_cmd_2);
  fl_register_hotkey(.hotkey = "Alt+P", .cb = music_prompt);
  fl_register_hotkey(.hotkey = "Alt+Shift+P", .cb = random_music);
  fl_add_to_cc(.display_name = "Force Reload Music", .on_action = reload_music,
               .icon = "/music.png");
  fl_add_to_cc(.display_name = "Music Player", .on_action = music_prompt,
               .icon = "/music.png");
  fl_add_to_cc(.display_name = "Add Music", .on_action = add_music,
               .icon = "/music.png");
}
