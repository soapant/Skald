#include "core/context.h"
#include "core/modal.h"
#include "config.h"
#include "core/modal.h"
#include "core/common_ui.h"
#include "core/platform.h"
#include "core/types.h"
#include "core/timer.h"
#include "helpers.h"
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


extern int pending_hotkey_vk;
extern int pending_hotkey_mod;
extern long pending_hotkey_timeMs;

static bool should_use_grab(void) {
    WinTimer timer = TimerStart();
    if (getenv("WAYLAND_DISPLAY")) { TimerEnd(timer, "ui_x11:should_use_grab"); return false; }
    if (getenv("HYPRLAND_INSTANCE_SIGNATURE")) { TimerEnd(timer, "ui_x11:should_use_grab"); return false; }
    const char* t = getenv("XDG_SESSION_TYPE");
    if (t && strcmp(t, "wayland") == 0) { TimerEnd(timer, "ui_x11:should_use_grab"); return false; }
    TimerEnd(timer, "ui_x11:should_use_grab");
    return true;
}

typedef struct {
    EditorMode mode;
    SequenceState seq;
    int last_index;
} PerFeatureVimState;

HashMap* g_perFeatureVimMap = NULL;
PerFeatureVimState* GetPerFeatureState(const char* feature) {
    WinTimer timer = TimerStart();
    if (!feature) { TimerEnd(timer, "ui_x11:GetPerFeatureState"); return NULL; }
    if (!g_perFeatureVimMap) g_perFeatureVimMap = HashMap_Create(32);
    void* val = HashMap_Get(g_perFeatureVimMap, feature);
    TimerEnd(timer, "ui_x11:GetPerFeatureState");
    return (PerFeatureVimState*)val;
}
void SavePerFeatureState(const char* feature, EditorMode mode,int last_index) {
    WinTimer timer = TimerStart();
    if (!feature) { TimerEnd(timer, "ui_x11:SavePerFeatureState"); return; }
    if (!g_perFeatureVimMap) g_perFeatureVimMap = HashMap_Create(32);
    PerFeatureVimState* st = (PerFeatureVimState*)HashMap_Get(g_perFeatureVimMap, feature);
    if (!st) {
        st = (PerFeatureVimState*)calloc(1, sizeof(PerFeatureVimState));
        if (!st) { TimerEnd(timer, "ui_x11:SavePerFeatureState"); return; }
        HashMap_Put(g_perFeatureVimMap, feature, st);
    }
    st->mode = mode;
    st->last_index = last_index;
    TimerEnd(timer, "ui_x11:SavePerFeatureState");
}

void CleanupPerFeatureVimMap(void) {
    WinTimer timer = TimerStart();
    if (!g_perFeatureVimMap) { TimerEnd(timer, "ui_x11:CleanupPerFeatureVimMap"); return; }
    for (int i = 0; i < g_perFeatureVimMap->size; i++) {
        HashMapEntry* entry = g_perFeatureVimMap->buckets[i];
        while (entry) {
            free(entry->value);
            entry = entry->next;
        }
    }
    HashMap_Destroy(g_perFeatureVimMap);
    g_perFeatureVimMap = NULL;
    TimerEnd(timer, "ui_x11:CleanupPerFeatureVimMap");
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
    WinTimer timer = TimerStart();
    if (m->win != None && m->dpy) {
        XDestroyWindow(m->dpy, m->win);
        XFlush(m->dpy);
        m->win = None;
    }
    if (m->xd) { XftDrawDestroy(m->xd); m->xd = NULL; }
    Platform_DestroyFont(m->font);
    m->font = (FontHandle){0};
    TimerEnd(timer, "ui_x11:destroy_msg_window");
}

void Platform_DismissMessage(void) {
    WinTimer timer = TimerStart();
    pthread_mutex_lock(&g_msg_mtx);
    MsgWindow* old = g_msg_window;
    if (old) {
        g_msg_window = NULL;
        old->dead = 1;
        destroy_msg_window(old);
    }
    pthread_mutex_unlock(&g_msg_mtx);
    TimerEnd(timer, "ui_x11:Platform_DismissMessage");
}

static void* msg_destroy_thread(void* arg) {
    WinTimer timer = TimerStart();
    MsgWindow* m = (MsgWindow*)arg;
    usleep((useconds_t)m->timeout * 1000000);
    pthread_mutex_lock(&g_msg_mtx);
    if (!m->dead) {
        if (g_msg_window == m) g_msg_window = NULL;
        destroy_msg_window(m);
    }
    pthread_mutex_unlock(&g_msg_mtx);
    free(m);
    TimerEnd(timer, "ui_x11:msg_destroy_thread");
    return NULL;
}

static void replace_msg_window(MsgWindow* m) {
    WinTimer timer = TimerStart();
    pthread_mutex_lock(&g_msg_mtx);
    MsgWindow* old = g_msg_window;
    g_msg_window = m;
    if (old) {
        old->dead = 1;
        destroy_msg_window(old);
    }
    pthread_mutex_unlock(&g_msg_mtx);
    TimerEnd(timer, "ui_x11:replace_msg_window");
}

bool Linux_GetMainMonitorGeometry(Display* dpy, int* ox, int* oy, int* ow, int* oh) {
    WinTimer timer = TimerStart();
    Window root = DefaultRootWindow(dpy);
    XRRScreenResources* res = XRRGetScreenResourcesCurrent(dpy, root);
    if (!res) { TimerEnd(timer, "ui_x11:Linux_GetMainMonitorGeometry"); return false; }
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
    TimerEnd(timer, "ui_x11:Linux_GetMainMonitorGeometry");
    return found;
}

void Platform_DisplayMessage(const char* message, int centerX, int centerY, int maxWidth, int maxHeight, int timeout, ColorRef color) {
    WinTimer timer = TimerStart();
    Display* dpy = get_display();
    if (!dpy) {
        fprintf(stderr, "[PLATFORM] Platform_DisplayMessage: no X11 display\n");
        TimerEnd(timer, "ui_x11:Platform_DisplayMessage");
        return;
    }

    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    if (!FH_TO_XFT(font)) {
        fprintf(stderr, "[PLATFORM] Platform_DisplayMessage: font load failed\n");
        TimerEnd(timer, "ui_x11:Platform_DisplayMessage");
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

    // save focus to restore after showing notification
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
    // restore focus - don't steal
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
    // fix: for short messages "CENTER" with hardcoded 870 draws outside small winWidth (textW+40)
    // use winWidth for centering when message has no spaces or fits in one line
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
    TimerEnd(timer, "ui_x11:Platform_DisplayMessage");
}

void Platform_SettingsPopUp(void) {
    WinTimer timer = TimerStart();
    fprintf(stderr, "[PLATFORM] Platform_SettingsPopUp not implemented yet\n");
    TimerEnd(timer, "ui_x11:Platform_SettingsPopUp");
}

char* Platform_SearchPopup(void) {
    WinTimer timer = TimerStart();
    fprintf(stderr, "[PLATFORM] Platform_SearchPopup not implemented yet\n");
    TimerEnd(timer, "ui_x11:Platform_SearchPopup");
    return NULL;
}

int Platform_InitializeTrayIcon(void) {
    WinTimer timer = TimerStart();
    TimerEnd(timer, "ui_x11:Platform_InitializeTrayIcon");
    return -1;
}

#define MAX_KEYBOARDS 16

typedef struct {
    int fd;
    char path[256];
} SystemKeyboard;

static SystemKeyboard active_kbs[MAX_KEYBOARDS];
static int active_kb_count = 0;



static int g_uinput_fd = -1;
static bool g_suppress_release[KEY_MAX + 1] = {0};
static bool g_device_grabbed[MAX_KEYBOARDS] = {0};

static int init_uinput(void) {
    WinTimer timer = TimerStart();
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) fd = open("/dev/input/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "[UINPUT] open failed: %s (swallow disabled)\n", strerror(errno));
        TimerEnd(timer, "ui_x11:init_uinput");
        return -1;
    }
    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) perror("[UINPUT] UI_SET_EVBIT EV_KEY");
    if (ioctl(fd, UI_SET_EVBIT, EV_SYN) < 0) perror("[UINPUT] UI_SET_EVBIT EV_SYN");
    
    
    
    ioctl(fd, UI_SET_EVBIT, EV_MSC);
    ioctl(fd, UI_SET_MSCBIT, MSC_SCAN);
    for (int i = 0; i <= KEY_MAX; i++) {
        ioctl(fd, UI_SET_KEYBIT, i);
    }
    
    struct uinput_setup setup;
    memset(&setup, 0, sizeof(setup));
    setup.id.bustype = BUS_USB;
    setup.id.vendor = 0x1234;
    setup.id.product = 0x5678;
    snprintf(setup.name, sizeof(setup.name), "flint-virtual-keyboard");
    if (ioctl(fd, UI_DEV_SETUP, &setup) == 0) {
        if (ioctl(fd, UI_DEV_CREATE) < 0) {
            perror("[UINPUT] UI_DEV_CREATE");
            close(fd);
            TimerEnd(timer, "ui_x11:init_uinput");
            return -1;
        }
    } else {
        
        struct uinput_user_dev uud;
        memset(&uud, 0, sizeof(uud));
        snprintf(uud.name, sizeof(uud.name), "flint-virtual-keyboard");
        uud.id.bustype = BUS_USB;
        uud.id.vendor = 0x1234;
        uud.id.product = 0x5678;
        if (write(fd, &uud, sizeof(uud)) != sizeof(uud)) {
            perror("[UINPUT] write uud");
        }
        if (ioctl(fd, UI_DEV_CREATE) < 0) {
            perror("[UINPUT] UI_DEV_CREATE legacy");
            close(fd);
            TimerEnd(timer, "ui_x11:init_uinput");
            return -1;
        }
    }
    
    usleep(150 * 1000);
    fprintf(stderr, "[UINPUT] virtual keyboard created (fd=%d)\n", fd);
    TimerEnd(timer, "ui_x11:init_uinput");
    return fd;
}

static void destroy_uinput(void) {
    WinTimer timer = TimerStart();
    if (g_uinput_fd >= 0) {
        ioctl(g_uinput_fd, UI_DEV_DESTROY);
        close(g_uinput_fd);
        g_uinput_fd = -1;
    }
    TimerEnd(timer, "ui_x11:destroy_uinput");
}

static void uinput_emit(const struct input_event *ev) {
    WinTimer timer = TimerStart();
    if (g_uinput_fd < 0) { TimerEnd(timer, "ui_x11:uinput_emit"); return; }
    if (write(g_uinput_fd, ev, sizeof(*ev)) < 0) {
        
        if (errno != EAGAIN) perror("[UINPUT] write");
        TimerEnd(timer, "ui_x11:uinput_emit");
        return;
    }
    struct input_event syn;
    memset(&syn, 0, sizeof(syn));
    gettimeofday(&syn.time, NULL);
    syn.type = EV_SYN;
    syn.code = SYN_REPORT;
    syn.value = 0;
    if (write(g_uinput_fd, &syn, sizeof(syn)) < 0 && errno != EAGAIN) {
        perror("[UINPUT] write syn");
    }
    TimerEnd(timer, "ui_x11:uinput_emit");
}

void ScanAndHookKeyboards(void) {
    WinTimer timer = TimerStart();
    DIR *dir = opendir("/dev/input");
    if (!dir) { TimerEnd(timer, "ui_x11:ScanAndHookKeyboards"); return; }

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
                    char dev_name[256] = {0};
                    unsigned long key_bits[(KEY_MAX/8)+1] = {0};
                    bool has_alpha = false;
                    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) >= 0) {
                        if (key_bits[30/ (8*sizeof(unsigned long))] & (1UL << (30 % (8*sizeof(unsigned long))))) has_alpha = true;
                        if (!has_alpha && (key_bits[KEY_ESC/ (8*sizeof(unsigned long))] & (1UL << (KEY_ESC % (8*sizeof(unsigned long)))))) {
                            if (key_bits[KEY_SPACE/ (8*sizeof(unsigned long))] & (1UL << (KEY_SPACE % (8*sizeof(unsigned long))))) has_alpha = true;
                        }
                    }
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
    TimerEnd(timer, "ui_x11:ScanAndHookKeyboards");
}
//TEMPORARY
void HandleRunNobAndReload(void) {
  WinTimer timer = TimerStart();
  Platform_RunTerminalCommand("./nob",false);
  FeatureLoader_ReloadAll();
  TimerEnd(timer, "ui_x11:HandleRunNobAndReload");
}


bool g_Linux_CtrlHeld  = false;
bool g_Linux_AltHeld   = false;
bool g_Linux_ShiftHeld = false;

bool HandleGlobalKeyEvent(int key_code, int value) {
    WinTimer timer = TimerStart();
    if (key_code == KEY_LEFTCTRL || key_code == KEY_RIGHTCTRL) {
        if (value == 0) g_Linux_CtrlHeld = false;
        else g_Linux_CtrlHeld = true;
    }
    if (key_code == KEY_LEFTALT || key_code == KEY_RIGHTALT) {
        if (value == 0) g_Linux_AltHeld = false;
        else g_Linux_AltHeld = true;
    }
    if (key_code == KEY_LEFTSHIFT || key_code == KEY_RIGHTSHIFT) {
        if (value == 0) g_Linux_ShiftHeld = false;
        else g_Linux_ShiftHeld = true;
    }
    double seconds = Platform_GetUniversalTimeSeconds();
    if (seconds >= 300) {
        Platform_ResetUniversalTimer();
        printf("SUPPOSED TO CLEAR FILE\n");
    }

    if (value != 1) {
        TimerEnd(timer, "ui_x11:HandleGlobalKeyEvent");
        return 0;
    }

    int pressed_vk = ConvertLinuxKeyToWindowsVK(key_code);
    
    int modifiers = HOTKEY_MOD_NONE;
    if (g_Linux_AltHeld)   modifiers |= HOTKEY_MOD_ALT;
    if (g_Linux_ShiftHeld) modifiers |= HOTKEY_MOD_SHIFT;
    if (g_Linux_CtrlHeld)  modifiers |= HOTKEY_MOD_CTRL;

    if (hotkey_process(pressed_vk, modifiers)) {
        if (value == 1) { pending_hotkey_vk = pressed_vk; pending_hotkey_mod = modifiers; struct timeval tv; gettimeofday(&tv,NULL); pending_hotkey_timeMs = tv.tv_sec*1000 + tv.tv_usec/1000; }
        else if (value == 2) { pending_hotkey_vk = pressed_vk; pending_hotkey_mod = modifiers; struct timeval tv2; gettimeofday(&tv2,NULL); pending_hotkey_timeMs = tv2.tv_sec*1000 + tv2.tv_usec/1000; }
        TimerEnd(timer, "ui_x11:HandleGlobalKeyEvent");
        return 1;
    }

    //temporary
    HotkeyBinding binding = {
        .vk_code = 'N',
        .modifiers = HOTKEY_MOD_ALT_CTRL,
        .blacklist = NULL,
        .blacklist_count = 0,
        .whitelist = 0,
        .hotkey_callback = HandleRunNobAndReload,
        .hotkey_condition = NULL
    };
    hotkey_register_internal(binding);

    TimerEnd(timer, "ui_x11:HandleGlobalKeyEvent");
    return 0;
}

void Platform_RunHotKeyMessageLoop(void) {
    WinTimer timer = TimerStart();
    bool use_grab = should_use_grab();
    if (!use_grab) {
        g_uinput_fd = -1;
    } else {
        g_uinput_fd = init_uinput();
        if (g_uinput_fd < 0) {
            fprintf(stderr, "[SYSTEM] continuing without uinput (no swallow)\n");
        }
    }

    ScanAndHookKeyboards();
    if (active_kb_count == 0) {
        fprintf(stderr, "[ERROR] No structural keyboard nodes located under /dev/input/\n");
        destroy_uinput();
        TimerEnd(timer, "ui_x11:Platform_RunHotKeyMessageLoop");
        return;
    }

    struct pollfd *fds = calloc(active_kb_count, sizeof(struct pollfd));
    for (int i = 0; i < active_kb_count; i++) {
        fds[i].fd = active_kbs[i].fd;
        fds[i].events = POLLIN;
    }

    struct input_event ev;
    const long HOTKEY_STALE_EVENT_MS = 200;

    while (true) {
        int poll_res = poll(fds, active_kb_count, -1);
        if (poll_res < 0) break;

        for (int i = 0; i < active_kb_count; i++) {
            if (fds[i].revents & POLLIN) {
                bool grabbed = g_device_grabbed[i];
                while (read(fds[i].fd, &ev, sizeof(struct input_event)) > 0) {
                    
                    if (ev.type != EV_KEY) {
                        if (grabbed && g_uinput_fd >= 0) uinput_emit(&ev);
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
                            if (grabbed && g_uinput_fd >= 0) uinput_emit(&ev);
                            continue;
                        }
                        if (ev.value == 1) {
                            if (grabbed && g_uinput_fd >= 0) uinput_emit(&ev);
                            continue;
                        }
                    }

                    
                    
                    if (ev.value == 0 && ev.code <= KEY_MAX && g_suppress_release[ev.code]) {
                        g_suppress_release[ev.code] = false;
                        
                        continue;
                    }
                    if (ev.value == 2 && ev.code <= KEY_MAX && g_suppress_release[ev.code]) {
                        
                        continue;
                    }

                    if (HandleGlobalKeyEvent(ev.code, ev.value)) {
                        
                        if (ev.value == 1 && ev.code <= KEY_MAX) {
                            g_suppress_release[ev.code] = true;
                        } else if (ev.value == 2 && ev.code <= KEY_MAX) {
                            
                            g_suppress_release[ev.code] = true;
                        }
                        continue;
                    }
                    
                    if (grabbed) {
                        if (g_uinput_fd >= 0) {
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

    
    for (int i = 0; i < active_kb_count; i++) {
        ioctl(active_kbs[i].fd, EVIOCGRAB, 0);
        close(active_kbs[i].fd);
    }
    free(fds);
    destroy_uinput();
    TimerEnd(timer, "ui_x11:Platform_RunHotKeyMessageLoop");
}


void Platform_InitUI(void) {
    WinTimer timer = TimerStart();
    TimerEnd(timer, "ui_x11:Platform_InitUI");
}

void Platform_ShutdownUI(void) {
    WinTimer timer = TimerStart();
    TimerEnd(timer, "ui_x11:Platform_ShutdownUI");
}
