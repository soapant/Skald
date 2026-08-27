#include "common.h"
#include "config.h"
#include "core/modal.h"
#include "core/common_ui.h"
#include "platform/platform.h"
#include "core/types.h"
#include "core/timer.h"
#include "helpers.h"
#include "bindings/bindings.h"
#include "bindings/hotkey_definitions.h"
#include "window/subclasses.h"
#include "features/focus_switcher.h"
#include "core/feature_data.h"
#include "features/music_player.h"
#include "features/video_viewer.h"
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
#include <sys/time.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xft/Xft.h>
#include "x11_display.h"


PromptState* g_currentPromptState = NULL;
SimplePromptState* g_currentSimplePromptState = NULL;
SimplePromptState* g_currentEditPopupState = NULL;
bool g_EditPopupCloseRequested = false;
int pending_hotkey_vk = 0;
int pending_hotkey_mod = 0;
long pending_hotkey_timeMs = 0;
Window g_PromptWin = None;
bool g_PromptJustClosed = false;
long g_PromptCloseTimeMs = 0;
extern bool g_Linux_CtrlHeld, g_Linux_AltHeld, g_Linux_ShiftHeld;
extern char output_buffer[255];

static bool should_use_grab(void) {
    if (getenv("WAYLAND_DISPLAY")) return false;
    if (getenv("HYPRLAND_INSTANCE_SIGNATURE")) return false;
    const char* t = getenv("XDG_SESSION_TYPE");
    if (t && strcmp(t, "wayland") == 0) return false;
    return true;
}

typedef struct {
    EditorMode mode;
    SequenceState seq;
    int last_index;
} PerFeatureVimState;

static HashMap* g_perFeatureVimMap = NULL;
static PerFeatureVimState* GetPerFeatureState(const char* feature) {
    if (!feature) return NULL;
    if (!g_perFeatureVimMap) g_perFeatureVimMap = HashMap_Create(32);
    void* val = HashMap_Get(g_perFeatureVimMap, feature);
    return (PerFeatureVimState*)val;
}
static void SavePerFeatureState(const char* feature, EditorMode mode,int last_index) {
    if (!feature) return;
    if (!g_perFeatureVimMap) g_perFeatureVimMap = HashMap_Create(32);
    PerFeatureVimState* st = (PerFeatureVimState*)HashMap_Get(g_perFeatureVimMap, feature);
    if (!st) {
        st = (PerFeatureVimState*)calloc(1, sizeof(PerFeatureVimState));
        if (!st) return;
        HashMap_Put(g_perFeatureVimMap, feature, st);
    }
    st->mode = mode;
    st->last_index = last_index;
}

void CleanupPerFeatureVimMap(void) {
    if (!g_perFeatureVimMap) return;
    for (int i = 0; i < g_perFeatureVimMap->size; i++) {
        HashMapEntry* entry = g_perFeatureVimMap->buckets[i];
        while (entry) {
            free(entry->value);
            entry = entry->next;
        }
    }
    HashMap_Destroy(g_perFeatureVimMap);
    g_perFeatureVimMap = NULL;
}

char* Platform_PromptUser(const PromptSpec* spec) {
    const char** window_list = spec->items;
    const char* window_title = spec->title;
    int count = spec->count;
    int height = spec->height;
    int width = spec->width;
    int item_height = spec->item_height;
    RefreshCallback refresh_callback = spec->refresh;
    EnterCallback enter_callback = spec->enter;
    CtrlEnterCallback ctrl_enter_callback = spec->ctrl_enter;
    TabCallback tab_callback = spec->tab;
    CopyCallback copy_callback = spec->copy;
    RenderCallback render_callback = spec->render;
    EditCallback edit_callback = spec->edit;
    FilterCallback filter_callback = spec->filter;
    HelpCallback help_callback = spec->help;

    selection_complete = false;
    output_buffer[0] = '\0';

    Display* dpy = get_display();
    if (!dpy) return NULL;

    int screen = DefaultScreen(dpy);
    int screen_width = 1920;
    int screen_height = 1080;

    int layout_w = 900;
    int layout_h = 95 + (count > 0 ? (count * item_height) : 0);

    if (layout_h < height) layout_h = height;
    if (layout_h > width) layout_h = width;

    PromptState* state = (PromptState*)calloc(1, sizeof(PromptState));
    if (!state) return NULL;
    state->items = (char**)window_list;
    state->item_count = count;
    state->cursor = 0;
    state->width = layout_w;
    state->height = layout_h;
    {
        const char* p = strchr(window_title, ' ');
        state->window_title = (char*)(p ? p+1 : window_title);
    }
    state->filter[0] = '\0';
    state->has_listbox = (count >= 0);
    ListBoxInfo_Init(&state->listbox);
    state->listbox.selected_index = -1;
    g_currentPromptState = state;
    g_IsMainWindowActive = true;
    g_Linux_AltHeld = false;
    g_Linux_CtrlHeld = false;
    g_Linux_ShiftHeld = false;
    g_PromptJustClosed = false;
    {
        const char* feat = window_title ? (strchr(window_title,' ') ? strchr(window_title,' ')+1 : window_title) : "unknown";
        PerFeatureVimState* pfs = GetPerFeatureState(feat);
        if (pfs && ModePersistsThroughFeatures) {
            g_current_mode = pfs->mode;
            g_vim_mode = (pfs->mode==MODE_NORMAL?"NORMAL":pfs->mode==MODE_INSERT?"INSERT":"VISUAL");
            last_index = pfs->last_index;
            SetModeWithListBox(pfs->mode, &state->listbox);
        } else if (ModePersistsThroughFeatures) {
            extern EditorMode g_last_mode;
            SetModeWithListBox(g_last_mode, &state->listbox);
        } else {
            SetModeWithListBox(MODE_INSERT, &state->listbox);
        }
    }

    int x = (screen_width - layout_w) / 2;
    int y = (screen_height - layout_h) / 2;

    Window win = Linux_CreatePromptWindow(dpy, x, y, layout_w, layout_h);

    XSelectInput(dpy, win, KeyPressMask | ExposureMask | FocusChangeMask);

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
    {
        const char* p = strchr(window_title, ' ');
        g_current_feature = (char*)(p ? p+1 : window_title);
    }
    SetCurrentFeatureName(g_current_feature);
    if (g_refresh_callback) g_refresh_callback(state->filter, hListBox, state->has_listbox ? &state->listbox : NULL);

    XEvent ev;
    int running = 1;
    bool got_focus = false; 
                            
    while (running) {
        XNextEvent(dpy, &ev);
        if (ev.type == FocusIn) {
            got_focus = true;
        }
        if (CloseOnLossOfFocus && ev.type == FocusOut && got_focus) {
            if (ev.xfocus.mode == NotifyNormal) {
                output_buffer[0] = '\0';
                selection_complete = true;
                running = 0;
                break;
            }
        }
        if (ev.type == KeyPress) {
            bool handled = Linux_HandlePromptKey(&ev,state);
            if (!handled) {
              if (IsInsertMode()) {
                  char buf[32];
                  KeySym ks;
                  int len = XLookupString(&ev.xkey, buf, sizeof(buf), &ks, NULL);
                  if (len > 0) {
                      
                      bool swallow_pending = false;
                      if (pending_hotkey_vk != 0 && len==1) {
                          struct timeval tv; gettimeofday(&tv,NULL); long now=tv.tv_sec*1000+tv.tv_usec/1000;
                          if (now - pending_hotkey_timeMs < 800) {
                              unsigned char c = (unsigned char)buf[0];
                              int vk_from_char = (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : (c >= 'A' && c <= 'Z') ? c : (c==' ' ? VK_SPACE : 0);
                              if (vk_from_char !=0 && vk_from_char == pending_hotkey_vk) {
                                  swallow_pending = true;
                                  pending_hotkey_vk=0; pending_hotkey_mod=0;
                              } else if (vk_from_char==0 && pending_hotkey_vk==VK_SPACE && c==' ') {
                                  swallow_pending = true;
                                  pending_hotkey_vk=0; pending_hotkey_mod=0;
                              }
                          } else {
                              pending_hotkey_vk=0; pending_hotkey_mod=0;
                          }
                      }
                      if (swallow_pending) {
                          
                      } else {
                          bool is_printable = true;
                          for (int i=0;i<len;i++) {
                            unsigned char c = (unsigned char)buf[i];
                            if (c < 0x20 || c == 0x7F) { is_printable = false; break; }
                          }
                          if (!is_printable) {
                          } else {
                            size_t n = strlen(state->filter);
                            if (n + len < sizeof(state->filter) - 1) {
                              if (state->cursor < 0) state->cursor = 0;
                              if ((size_t)state->cursor > n) state->cursor = (int)n;
                              memmove(state->filter + state->cursor + len,
                                      state->filter + state->cursor,
                                      n - state->cursor + 1);
                              memcpy(state->filter + state->cursor, buf, len);
                              state->cursor += len;
                              if (g_refresh_callback) g_refresh_callback(state->filter, hListBox, state->has_listbox ? &state->listbox : NULL);
                            }
                          }
                      }
                  }
              }
            }
        }
        Linux_RenderPromptUser(dpy,&win, state);
        if (selection_complete) {
            running = 0;
        }
    }

    if (!KeepSelectionAfterClose) {
        last_index = 0;
        if (state->has_listbox) state->listbox.selected_index = 0;
    }

    {
        const char* feat = g_current_feature ? g_current_feature : (window_title ? (strchr(window_title,' ') ? strchr(window_title,' ')+1 : window_title) : "unknown");
        SavePerFeatureState(feat, g_current_mode,last_index);
    }
    g_last_mode = g_current_mode;
    g_currentPromptState = NULL;
    g_IsMainWindowActive = false;
    {
        struct timeval tv; gettimeofday(&tv, NULL);
        g_PromptJustClosed = true;
        g_PromptCloseTimeMs = tv.tv_sec*1000 + tv.tv_usec/1000;
    }
    g_Linux_AltHeld = false;
    g_Linux_CtrlHeld = false;
    g_Linux_ShiftHeld = false;
    XSync(dpy, False);
    
    Platform_DismissMessage();
    XDestroyWindow(dpy,win);
    XSync(dpy, False);
    free(state);
    return output_buffer;
}

char* Platform_SimplePromptUser(const SimplePromptSpec* spec) {
    const char* prompt = spec->prompt;
    const char* window_title = spec->title;
    int height = spec->height;
    int width = spec->width;
    int item_height = spec->item_height;

    Display* dpy = get_display();
    if (!dpy) return NULL;

    int screen = DefaultScreen(dpy);
    int screen_width = 1920;
    int screen_height = 1080;

    int layout_w = 900;
    int layout_h = 95 + (1 > 0 ? (1 * item_height) : 0);


    selection_complete = false;
    output_buffer[0] = '\0';
    SimplePromptState* state = (SimplePromptState*)calloc(1, sizeof(SimplePromptState));
    state->display_message = (char*)prompt;
    state->user_input[0] = '\0';
    state->cursor = 0;
    state->width = layout_w;
    state->height = layout_h;
    {
        const char* p = strchr(window_title, ' ');
        state->window_title = (char*)(p ? p+1 : window_title);
        if (!state->window_title) state->window_title = (char*)window_title;
    }

    if (layout_h < height) layout_h = height;
    if (layout_h > width) layout_h = width;

    int x = (screen_width - layout_w) / 2;
    int y = (screen_height - layout_h) / 2;

    Window win = Linux_CreateSimplePromptWindow(dpy, x, y, layout_w, layout_h);

    XMapWindow(dpy, win);
    XFlush(dpy);
    g_currentSimplePromptState = state;
    g_IsMainWindowActive = true;
    g_PromptJustClosed = false;

    XEvent ev;
    int running = 1;

    while (running) {
        XNextEvent(dpy, &ev);
        if (ev.type == KeyPress) {
            bool handled = Linux_HandleSimplePromptKey(&ev,state);
            if (!handled) {
              if (IsInsertMode()) {
                  char buf[32];
                  KeySym ks;
                  int len = XLookupString(&ev.xkey, buf, sizeof(buf), &ks, NULL);
                  if (len > 0) {
                      bool swallow_pending = false;
                      if (pending_hotkey_vk !=0 && len==1) {
                          struct timeval tv; gettimeofday(&tv,NULL); long now=tv.tv_sec*1000+tv.tv_usec/1000;
                          if (now - pending_hotkey_timeMs < 800) {
                              unsigned char c=(unsigned char)buf[0];
                              int vkc=(c>='a'&&c<='z')?(c-'a'+'A'):(c>='A'&&c<='Z')?c:(c==' '?VK_SPACE:0);
                              if (vkc!=0 && vkc==pending_hotkey_vk) { swallow_pending=true; pending_hotkey_vk=0; pending_hotkey_mod=0; }
                          } else pending_hotkey_vk=0;
                      }
                      if (swallow_pending) {}
                      else {
                          bool is_printable = true;
                          for (int i=0;i<len;i++) {
                            unsigned char c = (unsigned char)buf[i];
                            if (c < 0x20 || c == 0x7F) { is_printable = false; break; }
                          }
                          if (!is_printable) {
                          } else {
                            size_t n = strlen(state->user_input);
                            if (n + len < sizeof(state->user_input) - 1) {
                              if (state->cursor < 0) state->cursor = 0;
                              if ((size_t)state->cursor > n) state->cursor = (int)n;
                              memmove(state->user_input + state->cursor + len,
                                      state->user_input + state->cursor,
                                      n - state->cursor + 1);
                              memcpy(state->user_input + state->cursor, buf, len);
                              state->cursor += len;
                            }
                          }
                      }
                  }
              }
            }
        }
        Linux_RenderSimplePromptUser(dpy,&win, state);
        if (selection_complete) { running = 0;selection_complete = false;}
    }

    if (state->user_input[0] != '\0') {
        strncpy(output_buffer, state->user_input, sizeof(output_buffer)-1);
        output_buffer[sizeof(output_buffer)-1] = '\0';
    } else {
        output_buffer[0] = '\0';
    }
    g_IsMainWindowActive = false;
    g_currentSimplePromptState = NULL;
    g_currentPromptState = NULL;
    {
        struct timeval tv; gettimeofday(&tv, NULL);
        g_PromptJustClosed = true;
        g_PromptCloseTimeMs = tv.tv_sec*1000 + tv.tv_usec/1000;
    }
    g_Linux_AltHeld = false;
    g_Linux_CtrlHeld = false;
    g_Linux_ShiftHeld = false;
    
    Platform_DismissMessage();
    XDestroyWindow(dpy, win);
    free(state);
    return output_buffer[0] ? output_buffer : NULL;
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
    if (m->win != None && m->dpy) {
        XDestroyWindow(m->dpy, m->win);
        XFlush(m->dpy);
        m->win = None;
    }
    if (m->xd) { XftDrawDestroy(m->xd); m->xd = NULL; }
    Platform_DestroyFont(m->font);
    m->font = (FontHandle){0};
}

void Platform_DismissMessage(void) {
    pthread_mutex_lock(&g_msg_mtx);
    MsgWindow* old = g_msg_window;
    if (old) {
        g_msg_window = NULL;
        old->dead = 1;
        destroy_msg_window(old);
    }
    pthread_mutex_unlock(&g_msg_mtx);
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

static bool Linux_GetMainMonitorGeometry(Display* dpy, int* ox, int* oy, int* ow, int* oh) {
    Window root = DefaultRootWindow(dpy);
    XRRScreenResources* res = XRRGetScreenResourcesCurrent(dpy, root);
    if (!res) return false;
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
    return found;
}

void Platform_DisplayMessage(const char* message, int centerX, int centerY, int maxWidth, int maxHeight, int timeout, ColorRef color) {
    Display* dpy = get_display();
    if (!dpy) {
        fprintf(stderr, "[PLATFORM] Platform_DisplayMessage: no X11 display\n");
        return;
    }

    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    if (!FH_TO_XFT(font)) {
        fprintf(stderr, "[PLATFORM] Platform_DisplayMessage: font load failed\n");
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

    struct pollfd pfd = { .fd = ConnectionNumber(dpy), .events = POLLIN };
    while (poll(&pfd, 1, 300) > 0) {
        XEvent ev;
        if (XCheckWindowEvent(dpy, win, ExposureMask, &ev)) {
            break;
        }
    }

    int startY = (winHeight - textH) / 2;

    draw_wrapped_text(dpy, xd,&text_xft_color, font, message,startY,"CENTER",0);

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
}

void Platform_SettingsPopUp(void) {
    fprintf(stderr, "[PLATFORM] Platform_SettingsPopUp not implemented yet\n");
}


char* Platform_EditTextPopup(const char* initial_text, int centerX, int centerY, int maxWidth, int maxHeight) {
    Display* dpy = get_display();

    int screen = DefaultScreen(dpy);

    if (maxWidth > 200) maxWidth = 200;
    if (maxHeight > 100) maxHeight = 100;

    int x = centerX - (maxWidth / 2);
    int y = centerY - (maxHeight / 2);

    Window focus_win = None;
    int focus_revert = RevertToNone;
    XGetInputFocus(dpy, &focus_win, &focus_revert);

    ListBoxInfo* active_lb = NULL;
    int saved_index = -1;

    SimplePromptState* state = (SimplePromptState*)calloc(1, sizeof(SimplePromptState));

    if (!state) return NULL;

    state->display_message = "";
    strncpy(state->user_input, initial_text, sizeof(state->user_input) - 1);
    state->user_input[sizeof(state->user_input) - 1] = '\0';
    state->cursor = strlen(state->user_input);
    state->width = maxWidth;
    state->height = maxHeight;
    state->window_title = "EditPopup";

    Window win = Linux_CreateEditPopupWindow(dpy, x, y, maxWidth, maxHeight);

    g_currentEditPopupState = state;
    g_EditPopupCloseRequested = false;
    g_hEditPopup.xid = win;
    SetMode(MODE_INSERT);

    XMapWindow(dpy, win);
    XSetInputFocus(dpy, win, RevertToParent, CurrentTime);

    char* result = NULL;
    bool running = true;
    XEvent ev;
    while (running) {
        if (g_EditPopupCloseRequested) {
            g_EditPopupCloseRequested = false;
            break;
        }
        XNextEvent(dpy, &ev);
        if (ev.type == KeyPress) {
            bool confirmed = false;
            bool cancelled = false;
            bool handled = Linux_HandleEditPopupKey(&ev, state, &confirmed, &cancelled);
            if (confirmed) {
                result = strdup(state->user_input);
                running = false;
            } else if (cancelled || g_EditPopupCloseRequested) {
                g_EditPopupCloseRequested = false;
                running = false;
            } else if (!handled && IsInsertMode()) {
                char buf[32];
                KeySym ks;
                int len = XLookupString(&ev.xkey, buf, sizeof(buf), &ks, NULL);
                if (len > 0) {
                    bool swallow=false;
                    if (pending_hotkey_vk!=0 && len==1) {
                        struct timeval tv; gettimeofday(&tv,NULL); long now=tv.tv_sec*1000+tv.tv_usec/1000;
                        if (now - pending_hotkey_timeMs < 800) {
                            unsigned char c=(unsigned char)buf[0]; int vkc=(c>='a'&&c<='z')?(c-'a'+'A'):(c>='A'&&c<='Z')?c:(c==' '?VK_SPACE:0);
                            if (vkc!=0 && vkc==pending_hotkey_vk) swallow=true;
                        }
                        if (swallow) pending_hotkey_vk=0;
                        else if (now - pending_hotkey_timeMs >=800) pending_hotkey_vk=0;
                    }
                    if (swallow) {}
                    else {
                        bool is_printable = true;
                        for (int i=0;i<len;i++) {
                            unsigned char c = (unsigned char)buf[i];
                            if (c < 0x20 || c == 0x7F) { is_printable = false; break; }
                        }
                        if (is_printable) {
                            size_t n = strlen(state->user_input);
                            if (n + len < sizeof(state->user_input) - 1) {
                                size_t cursor = state->cursor;
                                if (cursor > n) cursor = n;
                                memmove(state->user_input + cursor + len,
                                        state->user_input + cursor,
                                        n - cursor + 1);
                                memcpy(state->user_input + cursor, buf, len);
                                state->cursor = cursor + len;
                            }
                        }
                    }
                }
            }
        }
        // temporary just so it works  for some reason it thinks its not getting focus no idea why
        // } else if (ev.type == FocusOut) {
        //     if (CloseOnLossOfFocus) {
        //         focus_lost = true;
        //         running = false;
        //     }

        if (!running) {
          break;
        }

        Linux_RenderSimplePromptUser(dpy, &win, state);
    }

    g_currentEditPopupState = NULL;
    g_EditPopupCloseRequested = false;
    g_hEditPopup = WH_NULL;

    XDestroyWindow(dpy,win);

    SetModeWithListBox(MODE_NORMAL, active_lb);

    XSync(dpy, False);

    free(state);

    return result;
}

char* Platform_SearchPopup(void) {
    fprintf(stderr, "[PLATFORM] Platform_SearchPopup not implemented yet\n");
    return NULL;
}

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



static int g_uinput_fd = -1;
static bool g_suppress_release[KEY_MAX + 1] = {0};
static bool g_device_grabbed[MAX_KEYBOARDS] = {0};

static int init_uinput(void) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) fd = open("/dev/input/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "[UINPUT] open failed: %s (swallow disabled)\n", strerror(errno));
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
    snprintf(setup.name, sizeof(setup.name), "myprogram-virtual-keyboard");
    if (ioctl(fd, UI_DEV_SETUP, &setup) == 0) {
        if (ioctl(fd, UI_DEV_CREATE) < 0) {
            perror("[UINPUT] UI_DEV_CREATE");
            close(fd);
            return -1;
        }
    } else {
        
        struct uinput_user_dev uud;
        memset(&uud, 0, sizeof(uud));
        snprintf(uud.name, sizeof(uud.name), "myprogram-virtual-keyboard");
        uud.id.bustype = BUS_USB;
        uud.id.vendor = 0x1234;
        uud.id.product = 0x5678;
        if (write(fd, &uud, sizeof(uud)) != sizeof(uud)) {
            perror("[UINPUT] write uud");
        }
        if (ioctl(fd, UI_DEV_CREATE) < 0) {
            perror("[UINPUT] UI_DEV_CREATE legacy");
            close(fd);
            return -1;
        }
    }
    
    usleep(150 * 1000);
    fprintf(stderr, "[UINPUT] virtual keyboard created (fd=%d)\n", fd);
    return fd;
}

static void destroy_uinput(void) {
    if (g_uinput_fd >= 0) {
        ioctl(g_uinput_fd, UI_DEV_DESTROY);
        close(g_uinput_fd);
        g_uinput_fd = -1;
    }
}

static void uinput_emit(const struct input_event *ev) {
    if (g_uinput_fd < 0) return;
    if (write(g_uinput_fd, ev, sizeof(*ev)) < 0) {
        
        if (errno != EAGAIN) perror("[UINPUT] write");
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
}

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
}


bool g_Linux_CtrlHeld  = false;
bool g_Linux_AltHeld   = false;
bool g_Linux_ShiftHeld = false;

bool HandleGlobalKeyEvent(int key_code, int value) {
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
        return 0;
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
        if (value == 1) { pending_hotkey_vk = pressed_vk; pending_hotkey_mod = modifiers; struct timeval tv; gettimeofday(&tv,NULL); pending_hotkey_timeMs = tv.tv_sec*1000 + tv.tv_usec/1000; }
        else if (value == 2) { pending_hotkey_vk = pressed_vk; pending_hotkey_mod = modifiers; struct timeval tv2; gettimeofday(&tv2,NULL); pending_hotkey_timeMs = tv2.tv_sec*1000 + tv2.tv_usec/1000; }
        return 1;
    }
    TimerEnd(timer, "ProcessHotkey");

    WinTimer timer5 = TimerStart();

    if (CheckTextSnippetBindings(pressed_vk, modifiers)) {
        if (value==1){ pending_hotkey_vk=pressed_vk; pending_hotkey_mod=modifiers; struct timeval tv; gettimeofday(&tv,NULL); pending_hotkey_timeMs=tv.tv_sec*1000+tv.tv_usec/1000; }
        return 1;
    }
    TimerEnd(timer5, "CheckTextSnippetBindings");

    if (CheckMacroBindings(pressed_vk, modifiers)) {
        if (value==1){ pending_hotkey_vk=pressed_vk; pending_hotkey_mod=modifiers; struct timeval tv; gettimeofday(&tv,NULL); pending_hotkey_timeMs=tv.tv_sec*1000+tv.tv_usec/1000; }
        return 1;
    }

    WinTimer timer7 = TimerStart();
    if (CheckAppHotkeyBindings(pressed_vk, modifiers)) {
        if (value==1){ pending_hotkey_vk=pressed_vk; pending_hotkey_mod=modifiers; struct timeval tv; gettimeofday(&tv,NULL); pending_hotkey_timeMs=tv.tv_sec*1000+tv.tv_usec/1000; }
        return 1;
    }
    TimerEnd(timer7, "CheckAppHotkeyBindings");

    WinTimer timer8 = TimerStart();
    if (CheckMiscHotkeyBindings(pressed_vk, modifiers)) {
        if (value==1){ pending_hotkey_vk=pressed_vk; pending_hotkey_mod=modifiers; struct timeval tv; gettimeofday(&tv,NULL); pending_hotkey_timeMs=tv.tv_sec*1000+tv.tv_usec/1000; }
        return 1;
    }
    TimerEnd(timer8, "CheckMiscHotkeyBindings");

    WinTimer timer9 = TimerStart();
    if (CheckQuickLinkHotkeyBindings(pressed_vk, modifiers)) {
        if (value==1){ pending_hotkey_vk=pressed_vk; pending_hotkey_mod=modifiers; struct timeval tv; gettimeofday(&tv,NULL); pending_hotkey_timeMs=tv.tv_sec*1000+tv.tv_usec/1000; }
        return 1;
    }
    TimerEnd(timer9, "CheckQuickLinkHotkeyBindings");

    WinTimer timer10 = TimerStart();
    if (CheckTerminalCommandHotkeyBindings(pressed_vk, modifiers)) {
        if (value==1){ pending_hotkey_vk=pressed_vk; pending_hotkey_mod=modifiers; struct timeval tv; gettimeofday(&tv,NULL); pending_hotkey_timeMs=tv.tv_sec*1000+tv.tv_usec/1000; }
      return 1;
    }
    TimerEnd(timer10, "HandleTerminalCommand");

    WinTimer timer11 = TimerStart();
    if (HandleFocusSwitcher(pressed_vk, modifiers, fgTitle)) {
        if (value==1){ pending_hotkey_vk=pressed_vk; pending_hotkey_mod=modifiers; struct timeval tv; gettimeofday(&tv,NULL); pending_hotkey_timeMs=tv.tv_sec*1000+tv.tv_usec/1000; }
        return 1;
    }
    TimerEnd(timer11, "HandleFocusSwitcher");

    WinTimer timer12 = TimerStart();
    if (HandleMusicPlayer(pressed_vk, modifiers, fgTitle)) {
        if (value==1){ pending_hotkey_vk=pressed_vk; pending_hotkey_mod=modifiers; struct timeval tv; gettimeofday(&tv,NULL); pending_hotkey_timeMs=tv.tv_sec*1000+tv.tv_usec/1000; }
        return 1;
    }
    TimerEnd(timer12, "HandleMusicPlayer");

    WinTimer timer13 = TimerStart();
    if (HandleVideoViewer(pressed_vk, modifiers, fgTitle)) {
        if (value==1){ pending_hotkey_vk=pressed_vk; pending_hotkey_mod=modifiers; struct timeval tv; gettimeofday(&tv,NULL); pending_hotkey_timeMs=tv.tv_sec*1000+tv.tv_usec/1000; }
        return 1;
    }
    TimerEnd(timer13, "HandleVideoPlayer");

    WinTimer timer14 = TimerStart();
    if (HandleBulkKillProcess(pressed_vk, modifiers, fgTitle)) {
        if (value==1){ pending_hotkey_vk=pressed_vk; pending_hotkey_mod=modifiers; struct timeval tv; gettimeofday(&tv,NULL); pending_hotkey_timeMs=tv.tv_sec*1000+tv.tv_usec/1000; }
        return 1;
    }
    TimerEnd(timer14, "HandleBulkKillProcess");

    return 0;
}

void Platform_RunHotKeyMessageLoop(void) {
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
}


void Platform_InitUI(void) {
}

void Platform_ShutdownUI(void) {
}
