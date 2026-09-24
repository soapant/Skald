#include "platform/x11/clipboard_wayland_x11.h"
#include "deps/log.h"
#include <wayland-client-core.h>
#include <dlfcn.h>
#include <pthread.h>
#include <poll.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

struct WlFns {
    struct wl_display *(*display_connect)(const char *);
    void (*display_disconnect)(struct wl_display *);
    int (*display_flush)(struct wl_display *);
    int (*display_roundtrip)(struct wl_display *);
    int (*display_dispatch_pending)(struct wl_display *);
    int (*display_prepare_read)(struct wl_display *);
    int (*display_read_events)(struct wl_display *);
    void (*display_cancel_read)(struct wl_display *);
    int (*display_get_fd)(struct wl_display *);
    void (*proxy_add_listener)(struct wl_proxy *, void (**)(void), void *);
    struct wl_proxy *(*proxy_marshal_flags)(struct wl_proxy *, uint32_t, const struct wl_interface *, uint32_t, uint32_t, ...);
    struct wl_proxy *(*proxy_marshal_constructor)(struct wl_proxy *, uint32_t, const struct wl_interface *, ...);
    void (*proxy_destroy)(struct wl_proxy *);
    int (*display_get_error)(struct wl_display *);
};

static const struct wl_interface *g_wl_registry_iface = NULL;

static struct WlFns wl;
static void *g_wl_handle = NULL;

static const struct wl_interface g_iface_source;
static const struct wl_interface g_iface_offer;
static const struct wl_interface g_iface_device;
static const struct wl_interface g_iface_manager;
static const struct wl_interface g_iface_seat;

static const struct wl_interface *g_src_offer_types[] = { NULL };
static const struct wl_interface *g_mgr_create_types[] = { &g_iface_source };
static const struct wl_interface *g_mgr_device_types[] = { &g_iface_device, NULL };
static const struct wl_interface *g_dev_sel_types[] = { &g_iface_source };
static const struct wl_interface *g_dev_offer_types[] = { &g_iface_offer };
static const struct wl_interface *g_dev_psel_types[] = { &g_iface_offer };

static const struct wl_message g_source_requests[] = {
    { "offer", "s", g_src_offer_types },
    { "destroy", "", NULL },
};
static const struct wl_message g_source_events[] = {
    { "send", "sh", NULL },
    { "cancelled", "", NULL },
};
static const struct wl_interface g_iface_source = {
    "zwlr_data_control_source_v1", 1, 2, g_source_requests, 2, g_source_events
};

static const struct wl_message g_offer_requests[] = {
    { "receive", "sh", NULL },
    { "destroy", "", NULL },
};
static const struct wl_message g_offer_events[] = {
    { "offer", "s", NULL },
};
static const struct wl_interface g_iface_offer = {
    "zwlr_data_control_offer_v1", 1, 2, g_offer_requests, 1, g_offer_events
};

static const struct wl_message g_device_requests[] = {
    { "set_selection", "?o", g_dev_sel_types },
    { "destroy", "", NULL },
    { "set_primary_selection", "?o", g_dev_sel_types },
};
static const struct wl_message g_device_events[] = {
    { "data_offer", "n", g_dev_offer_types },
    { "selection", "?o", g_dev_psel_types },
    { "finished", "", NULL },
    { "primary_selection", "?o", g_dev_psel_types },
};
static const struct wl_interface g_iface_device = {
    "zwlr_data_control_device_v1", 2, 3, g_device_requests, 4, g_device_events
};

static const struct wl_message g_manager_requests[] = {
    { "create_data_source", "n", g_mgr_create_types },
    { "get_data_device", "no", g_mgr_device_types },
    { "destroy", "", NULL },
};
static const struct wl_interface g_iface_manager = {
    "zwlr_data_control_manager_v1", 2, 3, g_manager_requests, 0, NULL
};

static const struct wl_message g_seat_events[] = {
    { "capabilities", "u", NULL },
    { "name", "s", NULL },
};
static const struct wl_interface g_iface_seat = {
    "wl_seat", 1, 0, NULL, 2, g_seat_events
};

static struct wl_display *g_wl_dpy = NULL;
static struct wl_proxy *g_wl_manager = NULL;
static struct wl_proxy *g_wl_seat = NULL;
static struct wl_proxy *g_wl_device = NULL;
static struct wl_proxy *g_wl_source = NULL;
static struct wl_proxy *g_wl_psource = NULL;
static uint32_t g_mgr_name = 0;
static uint32_t g_mgr_ver = 0;
static uint32_t g_seat_name = 0;
static int g_mgr_bind_ver = 0;
static char *g_wl_text = NULL;
static unsigned char *g_wl_png = NULL;
static size_t g_wl_png_len = 0;
static struct wl_proxy *g_wl_offer = NULL;
static bool g_wl_has_selection = false;
static char g_wl_offer_mimes[8][64];
static int g_wl_offer_mime_count = 0;
static bool g_wl_running = false;
static bool g_wl_ready = false;
static bool g_wl_dead = false;
static bool g_wl_thread_started = false;
static pthread_t g_wl_thread = 0;
static pthread_mutex_t g_wl_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_wl_text_lock = PTHREAD_MUTEX_INITIALIZER;

static void reg_global(void *data, struct wl_proxy *reg, uint32_t name, const char *iface, uint32_t version) {
    (void)data;
    (void)reg;
    if (strcmp(iface, "zwlr_data_control_manager_v1") == 0) {
        g_mgr_name = name;
        g_mgr_ver = version;
    } else if (strcmp(iface, "wl_seat") == 0 && g_seat_name == 0) {
        g_seat_name = name;
    }
}

static void reg_global_remove(void *data, struct wl_proxy *reg, uint32_t name) {
    (void)data;
    (void)reg;
    (void)name;
}

static void (*g_reg_listener[])(void) = {
    (void (*)(void))reg_global,
    (void (*)(void))reg_global_remove,
};

static void offer_mime(void *data, struct wl_proxy *offer, const char *mime) {
    (void)data;
    (void)offer;
    if (!mime) return;
    pthread_mutex_lock(&g_wl_text_lock);
    if (g_wl_offer_mime_count < 8) {
        strncpy(g_wl_offer_mimes[g_wl_offer_mime_count], mime, sizeof(g_wl_offer_mimes[0]) - 1);
        g_wl_offer_mimes[g_wl_offer_mime_count][sizeof(g_wl_offer_mimes[0]) - 1] = '\0';
        g_wl_offer_mime_count++;
    }
    pthread_mutex_unlock(&g_wl_text_lock);
}

static void (*g_offer_listener[])(void) = {
    (void (*)(void))offer_mime,
};

static void dev_data_offer(void *data, struct wl_proxy *dev, struct wl_proxy *offer) {
    (void)data;
    (void)dev;
    if (g_wl_offer && g_wl_offer != offer) {
        wl.proxy_marshal_flags(g_wl_offer, 1, NULL, 0, 0);
        wl.proxy_destroy(g_wl_offer);
    }
    g_wl_offer = offer;
    g_wl_has_selection = false;
    pthread_mutex_lock(&g_wl_text_lock);
    g_wl_offer_mime_count = 0;
    pthread_mutex_unlock(&g_wl_text_lock);
    wl.proxy_add_listener(offer, (void (**)(void))g_offer_listener, NULL);
}

static void dev_selection(void *data, struct wl_proxy *dev, struct wl_proxy *offer) {
    (void)data;
    (void)dev;
    g_wl_has_selection = offer != NULL;
}

static void dev_finished(void *data, struct wl_proxy *dev) {
    (void)data;
    (void)dev;
}

static void dev_primary(void *data, struct wl_proxy *dev, struct wl_proxy *offer) {
    (void)data;
    (void)dev;
    (void)offer;
}

static void (*g_dev_listener[])(void) = {
    (void (*)(void))dev_data_offer,
    (void (*)(void))dev_selection,
    (void (*)(void))dev_finished,
    (void (*)(void))dev_primary,
};

static void src_send(void *data, struct wl_proxy *src, const char *mime, int fd) {
    (void)data;
    (void)src;
    pthread_mutex_lock(&g_wl_text_lock);
    char *text = g_wl_text ? strdup(g_wl_text) : NULL;
    unsigned char *png = NULL;
    size_t png_len = 0;
    if (g_wl_png && g_wl_png_len > 0) {
        png = malloc(g_wl_png_len);
        if (png) {
            memcpy(png, g_wl_png, g_wl_png_len);
            png_len = g_wl_png_len;
        }
    }
    pthread_mutex_unlock(&g_wl_text_lock);
    const unsigned char *buf = NULL;
    size_t n = 0;
    if (mime && strcmp(mime, "image/png") == 0) {
        buf = png;
        n = png_len;
    } else {
        buf = (const unsigned char *)text;
        n = text ? strlen(text) : 0;
    }
    size_t off = 0;
    while (off < n) {
        ssize_t w = write(fd, buf + off, n - off);
        if (w <= 0) break;
        off += (size_t)w;
    }
    free(text);
    free(png);
    close(fd);
    log_info("wayland clipboard served mime=%s bytes=%d", mime ? mime : "?", (int)off);
}

static void src_cancelled(void *data, struct wl_proxy *src) {
    (void)data;
    if (src == g_wl_source) g_wl_source = NULL;
    if (src == g_wl_psource) g_wl_psource = NULL;
    wl.proxy_marshal_flags(src, 1, NULL, 0, 0);
    wl.proxy_destroy(src);
    log_info("wayland clipboard source cancelled");
}

static void (*g_src_listener[])(void) = {
    (void (*)(void))src_send,
    (void (*)(void))src_cancelled,
};

static bool wl_load(void) {
    if (g_wl_handle) return true;
    dlerror();
    void *h = dlopen("libwayland-client.so.0", RTLD_NOW);
    if (!h) {
        log_info("wayland dlopen so.0 failed: %s", dlerror());
        dlerror();
        h = dlopen("/usr/lib/libwayland-client.so.0", RTLD_NOW);
    }
    if (!h) {
        log_info("wayland dlopen abspath failed: %s", dlerror());
        dlerror();
        h = dlopen("libwayland-client.so", RTLD_NOW);
    }
    if (!h) {
        log_warn("wayland clipboard dlopen failed: %s", dlerror());
        return false;
    }
    struct WlFns t = {0};
    t.display_connect = dlsym(h, "wl_display_connect");
    t.display_disconnect = dlsym(h, "wl_display_disconnect");
    t.display_flush = dlsym(h, "wl_display_flush");
    t.display_roundtrip = dlsym(h, "wl_display_roundtrip");
    t.display_dispatch_pending = dlsym(h, "wl_display_dispatch_pending");
    t.display_prepare_read = dlsym(h, "wl_display_prepare_read");
    t.display_read_events = dlsym(h, "wl_display_read_events");
    t.display_cancel_read = dlsym(h, "wl_display_cancel_read");
    t.display_get_fd = dlsym(h, "wl_display_get_fd");
    t.proxy_add_listener = dlsym(h, "wl_proxy_add_listener");
    t.proxy_marshal_flags = dlsym(h, "wl_proxy_marshal_flags");
    t.proxy_marshal_constructor = dlsym(h, "wl_proxy_marshal_constructor");
    t.proxy_destroy = dlsym(h, "wl_proxy_destroy");
    t.display_get_error = dlsym(h, "wl_display_get_error");
    g_wl_registry_iface = dlsym(h, "wl_registry_interface");
    if (!t.display_connect || !t.display_disconnect || !t.display_flush ||
        !t.display_roundtrip || !t.display_dispatch_pending || !t.display_prepare_read ||
        !t.display_read_events || !t.display_cancel_read ||
        !t.display_get_fd || !t.proxy_add_listener || !t.proxy_marshal_flags ||
        !t.proxy_marshal_constructor || !t.proxy_destroy || !t.display_get_error || !g_wl_registry_iface) {
        log_warn("wayland clipboard dlsym missing");
        dlclose(h);
        return false;
    }
    wl = t;
    g_wl_handle = h;
    return true;
}

static void *wl_thread_func(void *arg) {
    (void)arg;
    log_info("wayland clipboard thread enter");
    while (g_wl_running) {
        pthread_mutex_lock(&g_wl_lock);
        while (g_wl_running && wl.display_prepare_read(g_wl_dpy) != 0)
            wl.display_dispatch_pending(g_wl_dpy);
        pthread_mutex_unlock(&g_wl_lock);
        if (!g_wl_running) break;
        struct pollfd pfd = { wl.display_get_fd(g_wl_dpy), POLLIN, 0 };
        int r = poll(&pfd, 1, 100);
        if (r < 0) break;
        pthread_mutex_lock(&g_wl_lock);
        if (r == 0) {
            wl.display_cancel_read(g_wl_dpy);
        } else {
            if (wl.display_read_events(g_wl_dpy) < 0) {
                pthread_mutex_unlock(&g_wl_lock);
                break;
            }
            if (wl.display_dispatch_pending(g_wl_dpy) < 0) {
                pthread_mutex_unlock(&g_wl_lock);
                break;
            }
        }
        pthread_mutex_unlock(&g_wl_lock);
    }
    log_info("wayland clipboard thread exit");
    return NULL;
}

static bool wl_setup(void) {
    if (g_wl_ready) return true;
    if (g_wl_dead) return false;
    if (!wl_load()) {
        log_warn("wayland clipboard needs libwayland-client");
        g_wl_dead = true;
        return false;
    }
    struct wl_display *dpy = wl.display_connect(NULL);
    if (!dpy) {
        log_warn("wayland clipboard connect failed");
        g_wl_dead = true;
        return false;
    }
    struct wl_proxy *reg = wl.proxy_marshal_constructor((struct wl_proxy *)dpy, 1, g_wl_registry_iface);
    wl.proxy_add_listener(reg, (void (**)(void))g_reg_listener, NULL);
    pthread_mutex_lock(&g_wl_lock);
    wl.display_roundtrip(dpy);
    pthread_mutex_unlock(&g_wl_lock);
    if (!g_mgr_name || !g_seat_name) {
        wl.proxy_destroy(reg);
        wl.display_disconnect(dpy);
        log_warn("wayland compositor lacks zwlr_data_control_manager_v1");
        g_wl_dead = true;
        return false;
    }
    g_mgr_bind_ver = g_mgr_ver > 2 ? 2 : (int)g_mgr_ver;
    pthread_mutex_lock(&g_wl_lock);
    g_wl_manager = wl.proxy_marshal_flags(reg, 0, &g_iface_manager, (uint32_t)g_mgr_bind_ver, 0,
        g_mgr_name, "zwlr_data_control_manager_v1", (uint32_t)g_mgr_bind_ver, NULL);
    struct wl_proxy *seat = wl.proxy_marshal_flags(reg, 0, &g_iface_seat, 1, 0,
        g_seat_name, "wl_seat", 1, NULL);
    log_info("wayland setup mgr_name=%u mgr_ver=%u seat_name=%u manager=%p seat=%p",
        g_mgr_name, g_mgr_ver, g_seat_name, (void*)g_wl_manager, (void*)seat);
    if (!g_wl_manager || !seat) {
        pthread_mutex_unlock(&g_wl_lock);
        if (seat) wl.proxy_destroy(seat);
        if (g_wl_manager) wl.proxy_destroy(g_wl_manager);
        g_wl_manager = NULL;
        wl.proxy_destroy(reg);
        wl.display_disconnect(dpy);
        log_warn("wayland clipboard bind failed");
        g_wl_dead = true;
        return false;
    }
    g_wl_device = wl.proxy_marshal_constructor(g_wl_manager, 1, &g_iface_device, NULL, seat);
    if (!g_wl_device) {
        pthread_mutex_unlock(&g_wl_lock);
        wl.proxy_destroy(seat);
        wl.proxy_destroy(g_wl_manager);
        g_wl_manager = NULL;
        wl.proxy_destroy(reg);
        wl.display_disconnect(dpy);
        log_warn("wayland clipboard device failed");
        g_wl_dead = true;
        return false;
    }
    wl.proxy_add_listener(g_wl_device, (void (**)(void))g_dev_listener, NULL);
    g_wl_seat = seat;
    wl.display_roundtrip(dpy);
    int werr = wl.display_get_error(dpy);
    log_info("wayland setup proto_err=%d device=%p", werr, (void*)g_wl_device);
    pthread_mutex_unlock(&g_wl_lock);
    wl.proxy_destroy(reg);
    g_wl_dpy = dpy;
    g_wl_running = true;
    if (pthread_create(&g_wl_thread, NULL, wl_thread_func, NULL) != 0) {
        log_error("wayland clipboard thread start failed");
        wl.display_disconnect(dpy);
        g_wl_dpy = NULL;
        g_wl_running = false;
        g_wl_dead = true;
        return false;
    }
    g_wl_thread_started = true;
    g_wl_ready = true;
    log_info("wayland clipboard ready bind_ver=%d", g_mgr_bind_ver);
    return true;
}

static struct wl_proxy *wl_make_source(void) {
    struct wl_proxy *src = wl.proxy_marshal_constructor(g_wl_manager, 0, &g_iface_source, NULL);
    pthread_mutex_lock(&g_wl_text_lock);
    bool has_text = g_wl_text != NULL;
    bool has_png = g_wl_png != NULL && g_wl_png_len > 0;
    pthread_mutex_unlock(&g_wl_text_lock);
    if (has_text) {
        const char *mimes[] = { "text/plain;charset=utf-8", "text/plain", "TEXT", "STRING", "UTF8_STRING" };
        for (int i = 0; i < 5; i++) wl.proxy_marshal_flags(src, 0, NULL, 0, 0, mimes[i]);
    }
    if (has_png) wl.proxy_marshal_flags(src, 0, NULL, 0, 0, "image/png");
    wl.proxy_add_listener(src, (void (**)(void))g_src_listener, NULL);
    return src;
}

static void wl_drop_source(struct wl_proxy **slot) {
    if (slot && *slot) {
        wl.proxy_marshal_flags(*slot, 1, NULL, 0, 0);
        wl.proxy_destroy(*slot);
        *slot = NULL;
    }
}

bool Wayland_ClipboardAvailable(void) {
    return getenv("WAYLAND_DISPLAY") != NULL;
}

static void wl_publish_locked(void) {
    wl_drop_source(&g_wl_source);
    wl_drop_source(&g_wl_psource);
    pthread_mutex_lock(&g_wl_text_lock);
    bool has_any = g_wl_text != NULL || (g_wl_png != NULL && g_wl_png_len > 0);
    pthread_mutex_unlock(&g_wl_text_lock);
    if (!has_any) return;
    g_wl_source = wl_make_source();
    wl.proxy_marshal_flags(g_wl_device, 0, NULL, 0, 0, g_wl_source);
    if (g_mgr_bind_ver >= 2) {
        g_wl_psource = wl_make_source();
        wl.proxy_marshal_flags(g_wl_device, 2, NULL, 0, 0, g_wl_psource);
    }
    wl.display_flush(g_wl_dpy);
}

int Wayland_ClipboardSetText(const char *text) {
    if (!text) return 0;
    log_info("wayland clipboard set req len=%d", (int)strlen(text));
    if (!wl_setup()) return 0;
    pthread_mutex_lock(&g_wl_text_lock);
    char* nt = strdup(text);
    if (nt) {
        free(g_wl_text);
        g_wl_text = nt;
        free(g_wl_png);
        g_wl_png = NULL;
        g_wl_png_len = 0;
    }
    pthread_mutex_unlock(&g_wl_text_lock);
    if (!nt) return 0;
    pthread_mutex_lock(&g_wl_lock);
    wl_publish_locked();
    pthread_mutex_unlock(&g_wl_lock);
    log_info("wayland clipboard set done");
    return 1;
}

int Wayland_ClipboardSetImage(const unsigned char *png, size_t len) {
    if (!png || len == 0) return 0;
    log_info("wayland clipboard set req png len=%d", (int)len);
    if (!wl_setup()) return 0;
    unsigned char* np = malloc(len);
    if (np) memcpy(np, png, len);
    if (!np) return 0;
    pthread_mutex_lock(&g_wl_text_lock);
    free(g_wl_png);
    g_wl_png = np;
    g_wl_png_len = len;
    free(g_wl_text);
    g_wl_text = NULL;
    pthread_mutex_unlock(&g_wl_text_lock);
    pthread_mutex_lock(&g_wl_lock);
    wl_publish_locked();
    pthread_mutex_unlock(&g_wl_lock);
    log_info("wayland clipboard set image done");
    return 1;
}

static const char *wl_pick_mime_locked(void) {
    static const char *prefs[] = { "text/plain;charset=utf-8", "text/plain", "UTF8_STRING", "STRING", "TEXT" };
    for (size_t p = 0; p < sizeof(prefs) / sizeof(prefs[0]); p++) {
        for (int i = 0; i < g_wl_offer_mime_count; i++) {
            if (strcmp(g_wl_offer_mimes[i], prefs[p]) == 0) return prefs[p];
        }
    }
    return NULL;
}

char *Wayland_ClipboardGetText(void) {
    if (!wl_setup()) return NULL;
    pthread_mutex_lock(&g_wl_lock);
    struct wl_proxy *offer = (g_wl_offer && g_wl_has_selection) ? g_wl_offer : NULL;
    pthread_mutex_lock(&g_wl_text_lock);
    const char *pick = offer ? wl_pick_mime_locked() : NULL;
    char mime[64] = {0};
    if (pick) strncpy(mime, pick, sizeof(mime) - 1);
    pthread_mutex_unlock(&g_wl_text_lock);
    if (!offer || !mime[0]) {
        pthread_mutex_unlock(&g_wl_lock);
        return NULL;
    }
    int fds[2] = {-1, -1};
    if (pipe(fds) != 0) {
        pthread_mutex_unlock(&g_wl_lock);
        return NULL;
    }
    wl.proxy_marshal_flags(offer, 0, NULL, 0, 0, mime, fds[1]);
    wl.display_flush(g_wl_dpy);
    pthread_mutex_unlock(&g_wl_lock);
    close(fds[1]);
    size_t cap = 8192;
    size_t len = 0;
    char *out = malloc(cap);
    if (!out) {
        close(fds[0]);
        return NULL;
    }
    for (;;) {
        struct pollfd pfd = { fds[0], POLLIN, 0 };
        int r = poll(&pfd, 1, 500);
        if (r <= 0) break;
        if (len + 4096 + 1 > cap) {
            size_t ncap = (len + 4096 + 1) * 2;
            char *n = realloc(out, ncap);
            if (!n) {
                free(out);
                close(fds[0]);
                return NULL;
            }
            out = n;
            cap = ncap;
        }
        ssize_t got = read(fds[0], out + len, 4096);
        if (got <= 0) break;
        len += (size_t)got;
    }
    close(fds[0]);
    if (len == 0) {
        free(out);
        return NULL;
    }
    out[len] = '\0';
    log_info("wayland clipboard read len=%d", (int)len);
    return out;
}

void Wayland_ClipboardShutdown(void) {
    if (!g_wl_ready && !g_wl_handle) return;
    g_wl_running = false;
    if (g_wl_thread_started) {
        pthread_join(g_wl_thread, NULL);
        g_wl_thread_started = false;
    }
    if (g_wl_dpy) {
        pthread_mutex_lock(&g_wl_lock);
        wl_drop_source(&g_wl_source);
        wl_drop_source(&g_wl_psource);
        if (g_wl_offer) {
            wl.proxy_marshal_flags(g_wl_offer, 1, NULL, 0, 0);
            wl.proxy_destroy(g_wl_offer);
            g_wl_offer = NULL;
        }
        g_wl_has_selection = false;
        if (g_wl_device) {
            wl.proxy_marshal_flags(g_wl_device, 1, NULL, 0, 0);
            wl.proxy_destroy(g_wl_device);
            g_wl_device = NULL;
        }
        if (g_wl_manager) {
            wl.proxy_marshal_flags(g_wl_manager, 2, NULL, 0, 0);
            wl.proxy_destroy(g_wl_manager);
            g_wl_manager = NULL;
        }
        if (g_wl_seat) {
            wl.proxy_destroy(g_wl_seat);
            g_wl_seat = NULL;
        }
        pthread_mutex_unlock(&g_wl_lock);
        wl.display_disconnect(g_wl_dpy);
        g_wl_dpy = NULL;
    }
    pthread_mutex_lock(&g_wl_text_lock);
    free(g_wl_text);
    g_wl_text = NULL;
    free(g_wl_png);
    g_wl_png = NULL;
    g_wl_png_len = 0;
    pthread_mutex_unlock(&g_wl_text_lock);
    g_wl_ready = false;
    log_info("wayland clipboard stopped");
}
