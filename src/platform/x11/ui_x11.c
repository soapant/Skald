#include "core/context.h"
#include "core/modal.h"
#include "config.h"
#include "core/modal.h"
#include "core/common_ui.h"
#include "core/platform.h"
#include "core/types.h"
#include "core/timer.h"
#include "config.h"
#include "core/platform.h"
#include "helpers/hotkey.h"
#include "helpers/alias.h"
#include "helpers/flag.h"
#include "helpers/file.h"
#include "helpers/ui.h"
#include "core/hotkey_manager.h"
#include "bindings/bindings.h"
#include "platform/subclasses.h"
#include "core/feature_loader.h"
#include "core/feature_data.h"
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
#include <sys/time.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xft/Xft.h>
#include "platform/x11/display.h"
#include "platform/x11/drawing.h"
#include "platform/x11/window_factory.h"
#include "platform/x11/input.h"


extern int pending_hotkey_vk;
extern int pending_hotkey_mod;
extern long pending_hotkey_timeMs;

static bool should_use_grab(void) { return x11_should_use_grab(); }

typedef struct {
    EditorMode mode;
    SequenceState seq;
    int last_index;
} PerFeatureVimState;

HashMap* g_perFeatureVimMap = NULL;
PerFeatureVimState* GetPerFeatureState(const char* feature) {
    PROFILE_START(timer);
    if (!feature) { PROFILE_END_VAR(timer, "ui_x11:GetPerFeatureState"); return NULL; }
    if (!g_perFeatureVimMap) g_perFeatureVimMap = HashMap_Create(32);
    void* val = HashMap_Get(g_perFeatureVimMap, feature);
    PROFILE_END_VAR(timer, "ui_x11:GetPerFeatureState");
    return (PerFeatureVimState*)val;
}
void SavePerFeatureState(const char* feature, EditorMode mode,int last_index) {
    PROFILE_START(timer);
    if (!feature) { PROFILE_END_VAR(timer, "ui_x11:SavePerFeatureState"); return; }
    if (!g_perFeatureVimMap) g_perFeatureVimMap = HashMap_Create(32);
    PerFeatureVimState* st = (PerFeatureVimState*)HashMap_Get(g_perFeatureVimMap, feature);
    if (!st) {
        st = (PerFeatureVimState*)calloc(1, sizeof(PerFeatureVimState));
        if (!st) { PROFILE_END_VAR(timer, "ui_x11:SavePerFeatureState"); return; }
        HashMap_Put(g_perFeatureVimMap, feature, st);
    }
    st->mode = mode;
    st->last_index = last_index;
    PROFILE_END_VAR(timer, "ui_x11:SavePerFeatureState");
}

void CleanupPerFeatureVimMap(void) {
    PROFILE_START(timer);
    if (!g_perFeatureVimMap) { PROFILE_END_VAR(timer, "ui_x11:CleanupPerFeatureVimMap"); return; }
    for (int i = 0; i < g_perFeatureVimMap->size; i++) {
        HashMapEntry* entry = g_perFeatureVimMap->buckets[i];
        while (entry) {
            free(entry->value);
            entry = entry->next;
        }
    }
    HashMap_Destroy(g_perFeatureVimMap);
    g_perFeatureVimMap = NULL;
    PROFILE_END_VAR(timer, "ui_x11:CleanupPerFeatureVimMap");
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
    PROFILE_START(timer);
    if (m->win != None && m->dpy) {
        XDestroyWindow(m->dpy, m->win);
        XFlush(m->dpy);
        m->win = None;
    }
    if (m->xd) { XftDrawDestroy(m->xd); m->xd = NULL; }
    Platform_DestroyFont(m->font);
    m->font = (FontHandle){0};
    PROFILE_END_VAR(timer, "ui_x11:destroy_msg_window");
}

void Platform_DismissMessage(void) {
    PROFILE_START(timer);
    pthread_mutex_lock(&g_msg_mtx);
    MsgWindow* old = g_msg_window;
    if (old) {
        g_msg_window = NULL;
        old->dead = 1;
        destroy_msg_window(old);
    }
    pthread_mutex_unlock(&g_msg_mtx);
    PROFILE_END_VAR(timer, "ui_x11:Platform_DismissMessage");
}

static void* msg_destroy_thread(void* arg) {
    PROFILE_START(timer);
    MsgWindow* m = (MsgWindow*)arg;
    usleep((useconds_t)m->timeout * 1000000);
    pthread_mutex_lock(&g_msg_mtx);
    if (!m->dead) {
        if (g_msg_window == m) g_msg_window = NULL;
        destroy_msg_window(m);
    }
    pthread_mutex_unlock(&g_msg_mtx);
    free(m);
    PROFILE_END_VAR(timer, "ui_x11:msg_destroy_thread");
    return NULL;
}

static void replace_msg_window(MsgWindow* m) {
    PROFILE_START(timer);
    pthread_mutex_lock(&g_msg_mtx);
    MsgWindow* old = g_msg_window;
    g_msg_window = m;
    if (old) {
        old->dead = 1;
        destroy_msg_window(old);
    }
    pthread_mutex_unlock(&g_msg_mtx);
    PROFILE_END_VAR(timer, "ui_x11:replace_msg_window");
}

bool Linux_GetMainMonitorGeometry(Display* dpy, int* ox, int* oy, int* ow, int* oh) {
    PROFILE_START(timer);
    Window root = DefaultRootWindow(dpy);
    XRRScreenResources* res = XRRGetScreenResourcesCurrent(dpy, root);
    if (!res) { PROFILE_END_VAR(timer, "ui_x11:Linux_GetMainMonitorGeometry"); return false; }
    RROutput primary = XRRGetOutputPrimary(dpy, root);

    int fx = -1, fy = -1;
    Window fw = None; int revert;
    XGetInputFocus(dpy, &fw, &revert);
    if (fw != None && fw != PointerRoot) {
        XWindowAttributes a;
        int wx = 0, wy = 0; Window child;
        if (XGetWindowAttributes(dpy, fw, &a) &&
            XTranslateCoordinates(dpy, fw, root, a.width / 2, a.height / 2, &wx, &wy, &child)) {
            fx = wx; fy = wy;
        }
    }

    bool found = false;
    long best_score = -1;
    for (int c = 0; c < res->ncrtc; c++) {
        XRRCrtcInfo* ci = XRRGetCrtcInfo(dpy, res, res->crtcs[c]);
        if (!ci || ci->noutput == 0 || ci->width == 0) { if (ci) XRRFreeCrtcInfo(ci); continue; }
        long score = (long)ci->width * (long)ci->height;      
        bool contains_focus = (fx >= ci->x && fx < ci->x + ci->width &&
                               fy >= ci->y && fy < ci->y + ci->height);
        if (contains_focus) score += 1000000000L;             
        else if (primary != None && ci->outputs[0] == primary) score += 500000000L;
        if (score > best_score) {
            best_score = score;
            *ox = ci->x; *oy = ci->y; *ow = ci->width; *oh = ci->height;
            found = true;
        }
        XRRFreeCrtcInfo(ci);
    }
    XRRFreeScreenResources(res);
    PROFILE_END_VAR(timer, "ui_x11:Linux_GetMainMonitorGeometry");
    return found;
}

void Platform_DisplayMessage(const char* message, int centerX, int centerY, int maxWidth, int maxHeight, int timeout, ColorRef color) {
    PROFILE_START(timer);
    Display* dpy = get_display();
    if (!dpy) {
        fprintf(stderr, "[PLATFORM] Platform_DisplayMessage: no X11 display\n");
        PROFILE_END_VAR(timer, "ui_x11:Platform_DisplayMessage");
        return;
    }

    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    if (!FH_TO_XFT(font)) {
        fprintf(stderr, "[PLATFORM] Platform_DisplayMessage: font load failed\n");
        PROFILE_END_VAR(timer, "ui_x11:Platform_DisplayMessage");
        return;
    }

    int screen = DefaultScreen(dpy);

    int textW = 0, textH = 0;
    Platform_GetTextExtents(font, message, maxWidth - 30, &textW, &textH);

    int text_len = strlen(message);
    int internalWrappedMaxWidth = (FontSize * text_len) * 1.5;

    int winWidth = internalWrappedMaxWidth;
    if (winWidth > maxWidth) winWidth = maxWidth;
    
    if (textW + 40 > winWidth) winWidth = textW + 40;

    int winHeight = maxHeight;
    if (textH + 20 > winHeight) winHeight = textH + 20;

    unsigned long bg_color = rgb_to_hex(COLOR_BG);
    unsigned long text_color = rgb_to_hex(color);

    int mox = 0, moy = 0, mow = 1920, moh = 1080;
    bool have_mon = Linux_GetMainMonitorGeometry(dpy, &mox, &moy, &mow, &moh);
    int px = mox + (int)((centerX / 1920.0) * mow);
    int py = moy + (int)((centerY / 1080.0) * moh);
    int topLeftX = px - (winWidth / 2);
    int topLeftY = py - (winHeight / 2);
    (void)have_mon;
    if (topLeftX < mox) topLeftX = mox;
    if (topLeftY < moy) topLeftY = moy;
    if (topLeftX + winWidth > mox + mow) topLeftX = mox + mow - winWidth;
    if (topLeftY + winHeight > moy + moh) topLeftY = moy + moh - winHeight;


    Window prev_focus = None;
    int prev_revert = 0;
    XGetInputFocus(dpy, &prev_focus, &prev_revert);

    Window win = Linux_CreateDisplayMessageWindow(dpy, topLeftX, topLeftY, winWidth, winHeight);

    XftDraw* xd = Linux_CreateXftDraw(dpy, win);
    Visual* visual = DefaultVisual(dpy, screen);
    Colormap cmap = DefaultColormap(dpy, screen);
    XRenderColor rc = {
        ((text_color >> 16) & 0xFF) * 0x101,
        ((text_color >> 8) & 0xFF) * 0x101,
        (text_color & 0xFF) * 0x101,
        0xFFFF
    };
    XftColor text_xft_color;
    int alloc_ok = XftColorAllocValue(dpy, visual, cmap, &rc, &text_xft_color);

    XMapWindow(dpy, win);
    XMoveWindow(dpy, win, topLeftX, topLeftY);
    XFlush(dpy);

    if (prev_focus != None && prev_focus != PointerRoot && prev_focus != win) {
        XSetInputFocus(dpy, prev_focus, RevertToParent, CurrentTime);
        XFlush(dpy);
    }
    Linux_DrawBackground(dpy, win, winWidth, winHeight);

    struct pollfd pfd = { .fd = ConnectionNumber(dpy), .events = POLLIN };
    while (poll(&pfd, 1, 300) > 0) {
        XEvent ev;
        if (XCheckWindowEvent(dpy, win, ExposureMask, &ev)) {
            break;
        }
    }

    int startY = (winHeight - textH) / 2;


    if (textW < winWidth && textW > 0) {
        XftFont* xf = FH_TO_XFT(font);
        int y = startY + xf->ascent;
        int drawX = (winWidth - textW) / 2;
        if (drawX < 0) drawX = 0;
        XftDrawStringUtf8(xd, &text_xft_color, xf, drawX, y, (FcChar8*)message, strlen(message));
    } else {
        draw_wrapped_text(dpy, xd,&text_xft_color, font, message,startY,"CENTER",0);
    }

    XFlush(dpy);
    if (alloc_ok) XftColorFree(dpy, visual, cmap, &text_xft_color);

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
    PROFILE_END_VAR(timer, "ui_x11:Platform_DisplayMessage");
}

void Platform_SettingsPopUp(void) {
    PROFILE_START(timer);
    fprintf(stderr, "[PLATFORM] Platform_SettingsPopUp not implemented yet\n");
    PROFILE_END_VAR(timer, "ui_x11:Platform_SettingsPopUp");
}

char* Platform_SearchPopup(void) {
    PROFILE_START(timer);
    fprintf(stderr, "[PLATFORM] Platform_SearchPopup not implemented yet\n");
    PROFILE_END_VAR(timer, "ui_x11:Platform_SearchPopup");
    return NULL;
}

int Platform_InitializeTrayIcon(void) {
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "ui_x11:Platform_InitializeTrayIcon");
    return -1;
}



static int init_uinput(void) { return x11_init_uinput(); }
static void destroy_uinput(void) { x11_destroy_uinput(); }
static void uinput_emit(const struct input_event *ev) { x11_uinput_emit(ev); }

void ScanAndHookKeyboards(void) { x11_scan_keyboards(); }
bool HandleGlobalKeyEvent(int key_code, int value) { return x11_handle_global_key(key_code, value); }

void Platform_RunHotKeyMessageLoop(void) {
    PROFILE_START(timer);
    bool use_grab = should_use_grab();
    if (!use_grab) {
        g_x11_uinput_fd = -1;
    } else {
        g_x11_uinput_fd = init_uinput();
        if (g_x11_uinput_fd < 0) {
            fprintf(stderr, "[SYSTEM] continuing without uinput (no swallow)\n");
        }
    }

    ScanAndHookKeyboards();
    if (g_x11_keyboard_count == 0) {
        fprintf(stderr, "[ERROR] No structural keyboard nodes located under /dev/input/\n");
        destroy_uinput();
        PROFILE_END_VAR(timer, "ui_x11:Platform_RunHotKeyMessageLoop");
        return;
    }

    struct pollfd *fds = calloc(g_x11_keyboard_count, sizeof(struct pollfd));
    for (int i = 0; i < g_x11_keyboard_count; i++) {
        fds[i].fd = g_x11_keyboards[i].fd;
        fds[i].events = POLLIN;
    }

    struct input_event ev;
    const long HOTKEY_STALE_EVENT_MS = 200;

    while (true) {
        int poll_res = poll(fds, g_x11_keyboard_count, -1);
        if (poll_res < 0) break;

        for (int i = 0; i < g_x11_keyboard_count; i++) {
            if (fds[i].revents & POLLIN) {
                bool grabbed = g_x11_device_grabbed[i];
                while (read(fds[i].fd, &ev, sizeof(struct input_event)) > 0) {
                    
                    if (ev.type != EV_KEY) {
                        if (grabbed && g_x11_uinput_fd >= 0) uinput_emit(&ev);
                        continue;
                    }

                    struct timeval now;
                    gettimeofday(&now, NULL);
                    long age_ms = (now.tv_sec - ev.time.tv_sec) * 1000L
                                + (now.tv_usec - ev.time.tv_usec) / 1000L;
                    if (age_ms > HOTKEY_STALE_EVENT_MS) {
                        bool is_modifier = (ev.code == KEY_LEFTALT || ev.code == KEY_RIGHTALT ||
                                            ev.code == KEY_LEFTCTRL || ev.code == KEY_RIGHTCTRL ||
                                            ev.code == KEY_LEFTSHIFT || ev.code == KEY_RIGHTSHIFT ||
                                            ev.code == KEY_LEFTMETA || ev.code == KEY_RIGHTMETA);
                        if (!is_modifier) {
                            
                            
                            
                            if (ev.value == 1 && !grabbed) {
                                if (HandleGlobalKeyEvent(ev.code, ev.value)) {
                                    continue;
                                }
                            }
                            if (grabbed && g_x11_uinput_fd >= 0) uinput_emit(&ev);
                            continue;
                        }
                        if (ev.value == 1) {
                            if (grabbed && g_x11_uinput_fd >= 0) uinput_emit(&ev);
                            continue;
                        }
                    }

                    
                    
                    if (ev.value == 0 && ev.code <= KEY_MAX && g_x11_suppress_release[ev.code]) {
                        g_x11_suppress_release[ev.code] = false;
                        
                        continue;
                    }
                    if (ev.value == 2 && ev.code <= KEY_MAX && g_x11_suppress_release[ev.code]) {
                        
                        continue;
                    }

                    if (HandleGlobalKeyEvent(ev.code, ev.value)) {
                        
                        if (ev.value == 1 && ev.code <= KEY_MAX) {
                            g_x11_suppress_release[ev.code] = true;
                        } else if (ev.value == 2 && ev.code <= KEY_MAX) {
                            
                            g_x11_suppress_release[ev.code] = true;
                        }
                        continue;
                    }
                    
                    if (grabbed) {
                        if (g_x11_uinput_fd >= 0) {
                            uinput_emit(&ev);
                        } else {
                            static bool warned = false;
                            if (!warned) {
                                fprintf(stderr, "[WARN] grabbed but no uinput - key %d lost. Check /dev/uinput perms (need input group or ACL)\n", ev.code);
                                warned = true;
                            }
                        }
                    } else {
                        
                    }
                }
            }
        }
    }

    
    for (int i = 0; i < g_x11_keyboard_count; i++) {
        ioctl(g_x11_keyboards[i].fd, EVIOCGRAB, 0);
        close(g_x11_keyboards[i].fd);
    }
    free(fds);
    destroy_uinput();
    PROFILE_END_VAR(timer, "ui_x11:Platform_RunHotKeyMessageLoop");
}


void Platform_InitUI(void) {
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "ui_x11:Platform_InitUI");
}

void Platform_ShutdownUI(void) {
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "ui_x11:Platform_ShutdownUI");
}