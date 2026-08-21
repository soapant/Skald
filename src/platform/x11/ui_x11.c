#include "common.h"
#include "platform/platform.h"
#include "core/types.h"
#include "core/timer.h"
#include "helpers.h"
#include "bindings/bindings.h"
#include "bindings/hotkey_definitions.h"
#include "features/focus_switcher.h"
#include "core/feature_data.h"
#include "features/music_player.h"
#include "features/video_player.h"
#include "features/process_killer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <poll.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

PromptState* g_currentPromptState = NULL;

void Platform_WaitForKeypressAsync(KeyCallback callback) {
    (void)callback;
    fprintf(stderr, "[PLATFORM] Platform_WaitForKeypressAsync not implemented yet\n");
}

char* Platform_PromptUser(const char* window_list[], const char* window_title, int count, int height, int width, int item_height,
    RefreshCallback refresh_callback, 
    EnterCallback enter_callback,
    CtrlEnterCallback ctrl_enter_callback,
    TabCallback tab_callback,
    CopyCallback copy_callback,
    RenderCallback render_callback,
    EditCallback edit_callback,
    FilterCallback filter_callback,
    HelpCallback help_callback) {
    selection_complete = false;
    output_buffer[0] = '\0';
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) return NULL;

    int screen = DefaultScreen(dpy);
    int screen_width = 1920;
    int screen_height = 1080;

    int layout_w = 900;
    int layout_h = 95 + (count > 0 ? (count * item_height) : 0);

    if (layout_h < height) layout_h = height;
    if (layout_h > width) layout_h = width;

    PromptState state = {0};
    state.items = (char**)window_list;
    state.item_count = count;
    state.cursor = 0;
    state.width = layout_w;
    state.height = layout_h;
    state.window_title = (char*)(window_title + 5);
    state.filter[0] = '\0';
    state.has_listbox = (count >= 0);
    ListBoxInfo_Init(&state.listbox);
    state.listbox.selected_index = -1;
    g_currentPromptState = &state;

    int x = (screen_width - layout_w) / 2;
    int y = (screen_height - layout_h) / 2;

    unsigned long bg_color = rgb_to_hex(COLOR_BG);

    Window win = XCreateSimpleWindow(
        dpy,
        RootWindow(dpy, screen),
        x, y,
        layout_w, layout_h,
        0,              
        bg_color,       
        bg_color        
    );

    Atom wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom wm_state_fullscreen = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    
    Atom wm_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom wm_type_dialog = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    XChangeProperty(dpy, win, wm_type, XA_ATOM, 32,
                    PropModeReplace, (unsigned char*)&wm_type_dialog, 1);

    XClassHint* class_hint = XAllocClassHint();
    class_hint->res_name = "colored";
    class_hint->res_class = "colored";
    XSetClassHint(dpy, win, class_hint);
    XFree(class_hint);

    XWMHints* wm_hints = XAllocWMHints();
    wm_hints->flags = InputHint;
    wm_hints->input = True;
    XSetWMHints(dpy, win, wm_hints);
    XFree(wm_hints);

    XSelectInput(dpy, win, KeyPressMask | ExposureMask);

    XMapWindow(dpy, win);
    XFlush(dpy);

    g_refresh_callback = refresh_callback;
    g_enter_callback = enter_callback;
    g_ctrl_enter_callback = ctrl_enter_callback;
    g_tab_callback = tab_callback;
    g_copy_callback = copy_callback;
    g_render_callback = render_callback;
    g_edit_callback = edit_callback;
    g_filter_callback = filter_callback;
    g_help_callback = help_callback;
    g_title = window_title;
    g_current_feature = (char*)window_title + 5;
    SetCurrentFeatureName(g_current_feature);
    if (g_refresh_callback) g_refresh_callback(state.filter, hListBox, state.has_listbox ? &state.listbox : NULL);

    XEvent ev;
    int running = 1;
    while (running) {
        XNextEvent(dpy, &ev);
        if (ev.type == KeyPress) {
            if (!Linux_HandlePromptKey(&ev,&state) && (!state.has_listbox || state.listbox.selected_index == -1)) {
              char buf[32];
              KeySym ks;
              int len = XLookupString(&ev.xkey, buf, sizeof(buf), &ks, NULL);
              if (len > 0) {
                  size_t n = strlen(state.filter);
                  if (n + len < sizeof(state.filter) - 1) {
                      memcpy(state.filter + n, buf, len);
                      state.filter[n + len] = '\0';
                  }
              }
              state.cursor += 1;
            }
        }
        Linux_RenderPromptUser(dpy,&win, &state);
        if (selection_complete) {
            running = 0;
        }
    }

    g_currentPromptState = NULL;
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return output_buffer;
}

char* Platform_SimplePromptUser(const char* prompt, const char* window_title,int height, int width, int item_height) {
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) return NULL;

    int screen = DefaultScreen(dpy);
    int screen_width = DisplayWidth(dpy, screen);
    int screen_height = DisplayHeight(dpy, screen);

    int layout_w = 900;
    int layout_h = 95 + (1 > 0 ? (1 * item_height) : 0);

    if (layout_h < height) layout_h = height;
    if (layout_h > width) layout_h = width;

    int x = (screen_width - layout_w) / 2;
    int y = (screen_height - layout_h) / 2;

    unsigned long bg_color = rgb_to_hex(COLOR_BG);

    Window win = XCreateSimpleWindow(
        dpy,
        RootWindow(dpy, screen),
        x, y,
        layout_w, layout_h,
        0,              
        bg_color,       
        bg_color        
    );

    Atom wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom wm_state_fullscreen = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    
    Atom wm_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom wm_type_dialog = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    XChangeProperty(dpy, win, wm_type, XA_ATOM, 32,
                    PropModeReplace, (unsigned char*)&wm_type_dialog, 1);

    XClassHint* class_hint = XAllocClassHint();
    class_hint->res_name = "colored";
    class_hint->res_class = "colored";
    XSetClassHint(dpy, win, class_hint);
    XFree(class_hint);

    XSelectInput(dpy, win, KeyPressMask | ExposureMask);

    XMapWindow(dpy, win);
    XFlush(dpy);

    XEvent ev;
    int running = 1;

    while (running) {
        XNextEvent(dpy, &ev);
        if (ev.type == KeyPress) {
            // Linux_HandlePromptKey(&ev,&state);
            if (selection_complete) {
                running = 0;
            }
        }
    }

    g_currentPromptState = NULL;
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return output_buffer;
    return NULL;
}


typedef struct {
    int dead;
    Display* dpy;
    Window win;
    XftDraw* xd;
    FontHandle font;
    int timeout;
} MsgWindow;

static pthread_mutex_t g_msg_mtx = PTHREAD_MUTEX_INITIALIZER;
static MsgWindow* g_msg_window = NULL;

static void destroy_msg_window(MsgWindow* m) {
    XftDrawDestroy(m->xd);
    Platform_DestroyFont(m->font);
    XCloseDisplay(m->dpy);
}

static void* msg_destroy_thread(void* arg) {
    MsgWindow* m = (MsgWindow*)arg;
    usleep((useconds_t)m->timeout * 1000000);
    pthread_mutex_lock(&g_msg_mtx);
    if (!m->dead) {
        if (g_msg_window == m) g_msg_window = NULL;
        destroy_msg_window(m);
    }
    pthread_mutex_unlock(&g_msg_mtx);
    free(m);
    return NULL;
}

static void replace_msg_window(MsgWindow* m) {
    pthread_mutex_lock(&g_msg_mtx);
    MsgWindow* old = g_msg_window;
    g_msg_window = m;
    if (old) {
        old->dead = 1;
        destroy_msg_window(old);
    }
    pthread_mutex_unlock(&g_msg_mtx);
}

void Platform_DisplayMessage(const char* message, int centerX, int centerY, int maxWidth, int maxHeight, int timeout) {
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "[PLATFORM] Platform_DisplayMessage: XOpenDisplay failed\n");
        return;
    }

    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    if (!FH_TO_XFT(font)) {
        fprintf(stderr, "[PLATFORM] Platform_DisplayMessage: font load failed\n");
        XCloseDisplay(dpy);
        return;
    }
    fprintf(stderr, "[MSG] font='%s' size=%d\n", UsedFont, FontSize);

    int screen = DefaultScreen(dpy);

    int textW = 0, textH = 0;
    Platform_GetTextExtents(font, message, maxWidth - 30, &textW, &textH);

    int winWidth = maxWidth;
    int winHeight = maxHeight;
    if (textH + 20 > winHeight) winHeight = textH + 20;

    unsigned long bg_color = rgb_to_hex(COLOR_BG);
    unsigned long text_color = rgb_to_hex(COLOR_TEXT);
    int topLeftX = centerX - (winWidth / 2);
    int topLeftY = centerY - (winHeight / 2);

    Window win = XCreateSimpleWindow(
        dpy,
        RootWindow(dpy, screen),
        topLeftX, topLeftY, 
        winWidth, winHeight,
        0,
        bg_color,
        bg_color
    );

    Atom wm_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom wm_type_dialog = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    XChangeProperty(dpy, win, wm_type, XA_ATOM, 32,
                    PropModeReplace, (unsigned char*)&wm_type_dialog, 1);

    XClassHint* class_hint = XAllocClassHint();
    class_hint->res_name = "flashtext";
    class_hint->res_class = "flashtext";
    XSetClassHint(dpy, win, class_hint);
    XFree(class_hint);

    XWMHints* wm_hints = XAllocWMHints();
    wm_hints->flags = InputHint;
    wm_hints->input = False;
    XSetWMHints(dpy, win, wm_hints);
    XFree(wm_hints);

    XSelectInput(dpy, win, ExposureMask | KeyPressMask);

    XftDraw* xd = XftDrawCreate(dpy, win, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen));
    Visual* visual = DefaultVisual(dpy, screen);
    Colormap cmap = DefaultColormap(dpy, screen);
    XRenderColor rc = {
        ((text_color >> 16) & 0xFF) * 0x101,
        ((text_color >> 8) & 0xFF) * 0x101,
        (text_color & 0xFF) * 0x101,
        0xFFFF
    };
    XftColor color;
    int alloc_ok = XftColorAllocValue(dpy, visual, cmap, &rc, &color);

    XMapWindow(dpy, win);
    XFlush(dpy);

    struct pollfd pfd = { .fd = ConnectionNumber(dpy), .events = POLLIN };
    while (poll(&pfd, 1, 300) > 0) {
        XEvent ev;
        if (XCheckWindowEvent(dpy, win, ExposureMask, &ev)) {
            break;
        }
    }

    int startY = (winHeight - textH) / 2;

    draw_wrapped_text(dpy, xd, &color, font, message,startY,"CENTER",0);

    XFlush(dpy);

    MsgWindow* m = (MsgWindow*)malloc(sizeof(MsgWindow));
    m->dead = 0;
    m->dpy = dpy;
    m->win = win;
    m->xd = xd;
    m->font = font;
    m->timeout = timeout;

    replace_msg_window(m);

    pthread_t tid;
    pthread_create(&tid, NULL, msg_destroy_thread, m);
    pthread_detach(tid);
}

void Platform_SettingsPopUp(void) {
    fprintf(stderr, "[PLATFORM] Platform_SettingsPopUp not implemented yet\n");
}


char* Platform_EditTextPopup(const char* initial_text, int centerX, int centerY, int maxWidth, int maxHeight) {
    (void)initial_text; (void)centerX; (void)centerY; (void)maxWidth; (void)maxHeight;
    fprintf(stderr, "[PLATFORM] Platform_EditTextPopup not implemented yet\n");
    return NULL;
}

char* Platform_SearchPopup(void) {
    fprintf(stderr, "[PLATFORM] Platform_SearchPopup not implemented yet\n");
    return NULL;
}
// On HyperLand There Is No Tray
int Platform_InitializeTrayIcon(void) {
    return -1;
}

#define MAX_KEYBOARDS 16

typedef struct {
    int fd;
    char path[256];
} SystemKeyboard;

static SystemKeyboard active_kbs[MAX_KEYBOARDS];
static int active_kb_count = 0;

void ScanAndHookKeyboards(void) {
    DIR *dir = opendir("/dev/input");
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && active_kb_count < MAX_KEYBOARDS) {
        if (strncmp(entry->d_name, "event", 5) == 0) {
            char full_path[256];
            snprintf(full_path, sizeof(full_path), "/dev/input/%s", entry->d_name);

            int fd = open(full_path, O_RDONLY | O_NONBLOCK);
            if (fd < 0) continue;

            unsigned long ev_bits[EV_MAX / 8 + 1] = {0};
            if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) >= 0) {
                if (ev_bits[0] & (1 << EV_KEY)) {
                    active_kbs[active_kb_count].fd = fd;
                    strncpy(active_kbs[active_kb_count].path, full_path, 255);
                    active_kb_count++;
                } else {
                    close(fd);
                }
            } else {
                close(fd);
            }
        }
    }
    closedir(dir);
}


bool g_Linux_CtrlHeld  = false;
bool g_Linux_AltHeld   = false;
bool g_Linux_ShiftHeld = false;

void HandleGlobalKeyEvent(int key_code, int value) {
    bool is_pressed = (value == 1);
    bool is_released = (value == 0);
    
    if (key_code == KEY_LEFTCTRL || key_code == KEY_RIGHTCTRL) {
        g_Linux_CtrlHeld = is_pressed;
    }
    if (key_code == KEY_LEFTALT || key_code == KEY_RIGHTALT) {
        g_Linux_AltHeld = is_pressed;
    }
    if (key_code == KEY_LEFTSHIFT || key_code == KEY_RIGHTSHIFT) {
        g_Linux_ShiftHeld = is_pressed;
    }
    
    
    if (!is_pressed) return;
    
    
    double seconds = Platform_GetUniversalTimeSeconds();
    if (seconds >= 300) {
        Platform_ResetUniversalTimer();
        printf("SUPPOSED TO CLEAR FILE\n");
    }

    char fgTitle[256] = {0};
    Linux_GetActiveWindowTitle(fgTitle, sizeof(fgTitle));

    int pressed_vk = ConvertLinuxKeyToWindowsVK(key_code);
    
    int modifiers = HOTKEY_MOD_NONE;
    if (g_Linux_AltHeld)   modifiers |= HOTKEY_MOD_ALT;
    if (g_Linux_ShiftHeld) modifiers |= HOTKEY_MOD_SHIFT;
    if (g_Linux_CtrlHeld)  modifiers |= HOTKEY_MOD_CTRL;

    WinTimer timer = TimerStart();
    if (ProcessHotkey(G_HOTKEY_BINDINGS, HOTKEY_BINDING_COUNT, 
          pressed_vk, modifiers, fgTitle)) {
    }
    TimerEnd(timer, "ProcessHotkey");

    // if (g_Linux_ShiftHeld && !IsInsertMode() && WH_TO_HWND(hPopupWnd) != NULL) {
    //     if (WH_TO_HWND(g_hEditPopup) == NULL) {
    //         if (pressed_vk == COMMAND_MODE_KEY) {
    //             SetMode(MODE_INSERT);
    //             char* output = Platform_EditTextPopup("", 960, 540, 1000, 300);
    //             if (output) {
    //                 HandleCommandExecution(output);
    //             }
    //             return;
    //         }
    //     }
    // }
    
    // if (WH_TO_HWND(g_hSearchPopup) == NULL && WH_TO_HWND(hPopupWnd) != NULL && IsNormalMode()) {
    //     if (pressed_vk == SEARCH_PROGRAM_KEY) {
    //         Platform_SearchPopup();
    //     }
    // }

    // WinTimer timer2 = TimerStart();
    // if (g_Linux_CtrlHeld) {
    //     if (pressed_vk == HELP_SCREEN_KEY) {
    //         if (g_help_callback != NULL) {
    //             g_help_callback();
    //             TimerEnd(timer2, "HelpScreen");
    //         }
    //     }
    // }
    // TimerEnd(timer2, "HelpScreenCheck");
    
    // WinTimer timer3 = TimerStart();
    // if (IsNormalMode()) {
    //     if (g_waiting_for_key) {
    //         g_waiting_for_key = false;
    //         if (g_key_callback) {
    //             g_key_callback(pressed_vk); // Pass converted key variable down
    //             TimerEnd(timer3, "KeyCallback");
    //         }
    //         return;
    //     }       
    // }
    // TimerEnd(timer3, "NormalModeCheck");

    // MotionAction vimAction = MOTION_NONE;
    // const char* matchedSequence = NULL;
    // int typedCount = 0;

    // WinTimer timer4 = TimerStart();
    // if (CheckVimMotion(pressed_vk, modifiers, &vimAction, &matchedSequence, &typedCount)) {
    //     ExecuteMotion(vimAction, matchedSequence, typedCount);
    //     TimerEnd(timer4, "ExecuteMotion");
    //     return;
    // }
    // TimerEnd(timer4, "CheckVimMotion");

    WinTimer timer5 = TimerStart();

    if (CheckTextSnippetBindings(pressed_vk, modifiers)) {
        return;
    }
    TimerEnd(timer5, "CheckTextSnippetBindings");

    if (CheckMacroBindings(pressed_vk, modifiers)) {
        return;
    }

    WinTimer timer7 = TimerStart();
    if (CheckAppHotkeyBindings(pressed_vk, modifiers)) {
        return;
    }
    TimerEnd(timer7, "CheckAppHotkeyBindings");

    WinTimer timer8 = TimerStart();
    if (CheckMiscHotkeyBindings(pressed_vk, modifiers)) {
        return;
    }
    TimerEnd(timer8, "CheckMiscHotkeyBindings");

    WinTimer timer9 = TimerStart();
    if (CheckQuickLinkHotkeyBindings(pressed_vk, modifiers)) {
        return;
    }
    TimerEnd(timer9, "CheckQuickLinkHotkeyBindings");

    WinTimer timer10 = TimerStart();
    if (HandleFocusSwitcher(pressed_vk, modifiers, fgTitle)) {
        return;
    }
    TimerEnd(timer10, "HandleFocusSwitcher");

    WinTimer timer11 = TimerStart();
    if (HandleMusicPlayer(pressed_vk, modifiers, fgTitle)) {
        return;
    }
    TimerEnd(timer11, "HandleMusicPlayer");

    WinTimer timer12 = TimerStart();
    if (HandleVideoPlayer(pressed_vk, modifiers, fgTitle)) {
        return;
    }
    TimerEnd(timer12, "HandleVideoPlayer");

    WinTimer timer13 = TimerStart();
    if (HandleBulkKillProcess(pressed_vk, modifiers, fgTitle)) {
        return;
    }
    TimerEnd(timer13, "HandleBulkKillProcess");
}

void Platform_RunHotKeyMessageLoop(void) {
    ScanAndHookKeyboards();
    if (active_kb_count == 0) {
        fprintf(stderr, "[ERROR] No structural keyboard nodes located under /dev/input/\n");
        return;
    }

    printf("[SYSTEM] Low-Level Kernel Hook installed across %d input streams.\n", active_kb_count);

    struct pollfd *fds = calloc(active_kb_count, sizeof(struct pollfd));
    for (int i = 0; i < active_kb_count; i++) {
        fds[i].fd = active_kbs[i].fd;
        fds[i].events = POLLIN;
    }

    struct input_event ev;

    while (true) {
        int poll_res = poll(fds, active_kb_count, -1);
        if (poll_res < 0) break;

        for (int i = 0; i < active_kb_count; i++) {
            if (fds[i].revents & POLLIN) {
                while (read(fds[i].fd, &ev, sizeof(struct input_event)) > 0) {
                    if (ev.type == EV_KEY) {
                        HandleGlobalKeyEvent(ev.code, ev.value);
                    }
                }
            }
        }
    }

    for (int i = 0; i < active_kb_count; i++) {
        close(active_kbs[i].fd);
    }
    free(fds);
}
// On X11 Window Initalization Doesnt Need To Be Done
void Platform_InitUI(void) {
}
// Same Thing Here
void Platform_ShutdownUI(void) {
}
