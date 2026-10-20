#include "core/timer.h"
#if defined(FLINT_WAYLAND_SESSION)
#include "platform/x11/clipboard_backend_x11.h"
#include "deps/log.h"
#include "core/clipboard_history.h"
#include <wayland-client-core.h>
#include <dlfcn.h>
#include <pthread.h>
#include <poll.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <stdint.h>
#include <errno.h>

static volatile long g_wl_seq = 0;
static long g_wl_ev_count = 0;
static long g_wl_req_count = 0;
static long g_wl_send_count = 0;
static long g_wl_recv_count = 0;
static long g_wl_prepare_fail = 0;
static long g_wl_receive_calls = 0;
static long g_wl_receive_blocked = 0;
static const void *g_wl_dead_offer = NULL;
static long g_wl_hypr_pid = -1;
static long g_wl_hypr_fd = -1;
static bool g_wl_read_disabled = false;
static char g_wl_read_block_reason[128] = {0};
#define WL_MAX_RECEIVE_CALLS 3
#define WL_HYPR_FD_ALARM 20000
extern long g_cl_selreq;

static void wl_trace(const char *what, const char *fmt, ...) {
        PROFILE_START(_pt);
va_list ap;
    long n = ++g_wl_seq;
    char buf[512];
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
}

static int wl_dir_fd_count(const char *pid, const char *want) {
        PROFILE_START(_pt);
char p[64];
    snprintf(p, sizeof(p), "/proc/%s/fd", pid);
    DIR *d = opendir(p);
    if (!d) { PROFILE_END_VAR(_pt, "wl_dir_fd_count"); return -1; }
    int total = 0, pipes = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        total++;
        char lp[128];
        snprintf(lp, sizeof(lp), "/proc/%s/fd/%s", pid, e->d_name);
        char link[256];
        ssize_t r = readlink(lp, link, sizeof(link) - 1);
        if (r > 0) {
            link[r] = '\0';
            if (strncmp(link, "pipe:", 5) == 0) pipes++;
        }
    }
    closedir(d);
    if (want && strcmp(want, "pipes") == 0) { PROFILE_END_VAR(_pt, "wl_dir_fd_count"); return pipes; }
    { PROFILE_END_VAR(_pt, "wl_dir_fd_count"); return total; }
}

static long wl_dir_rss_mb(const char *pid) {
        PROFILE_START(_pt);
char p[64];
    snprintf(p, sizeof(p), "/proc/%s/statm", pid);
    FILE *f = fopen(p, "r");
    if (!f) { PROFILE_END_VAR(_pt, "wl_dir_rss_mb"); return -1; }
    long total = 0, res = 0;
    if (fscanf(f, "%ld %ld", &total, &res) != 2) { fclose(f); return -1; }
    fclose(f);
    { PROFILE_END_VAR(_pt, "wl_dir_rss_mb"); return (res * 4) / 1024; }
}

static int wl_find_pid(const char *name) {
        PROFILE_START(_pt);
DIR *d = opendir("/proc");
    if (!d) { PROFILE_END_VAR(_pt, "wl_find_pid"); return -1; }
    int found = -1;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
        char p[96];
        snprintf(p, sizeof(p), "/proc/%s/comm", e->d_name);
        FILE *f = fopen(p, "r");
        if (!f) continue;
        char buf[64] = {0};
        if (fgets(buf, sizeof(buf), f)) {
            size_t l = strlen(buf);
            while (l && (buf[l-1] == '\n' || buf[l-1] == '\r')) buf[--l] = '\0';
            if (strcmp(buf, name) == 0) { found = atoi(e->d_name); fclose(f); break; }
        }
        fclose(f);
    }
    closedir(d);
    { PROFILE_END_VAR(_pt, "wl_find_pid"); return found; }
}

static void *wl_sampler_func(void *arg) {
        PROFILE_START(_pt);
(void)arg;
    int hypr = -1, xway = -1, self = (int)getpid();
    int base_self = -1, base_hypr = -1;
    int prev_hypr_fd = -1;
    bool fired = false;
    for (int i = 0; i < 1200; i++) {
        struct timespec ts = { 1, 0 };
        nanosleep(&ts, NULL);
        int prev = hypr;
        hypr = wl_find_pid("Hyprland");
        xway = wl_find_pid("Xwayland");
        if (hypr != prev)  base_hypr = -1;
        char spid[32];
        snprintf(spid, sizeof(spid), "%d", self);
        int self_fd = wl_dir_fd_count(spid, NULL);
        int self_pipe = wl_dir_fd_count(spid, "pipes");
        int h_fd = -1, h_pipe = -1, h_rss = -1;
        if (hypr > 0) {
            char hp[32];
            snprintf(hp, sizeof(hp), "%d", hypr);
            h_fd = wl_dir_fd_count(hp, NULL);
            h_pipe = h_fd;
            if (h_fd < 20000) h_pipe = wl_dir_fd_count(hp, "pipes");
            else h_pipe = -2;
            h_rss = (int)wl_dir_rss_mb(hp);
            if (base_hypr < 0) base_hypr = h_fd;
        }
        if (base_self < 0) base_self = self_fd;
        int x_fd = -1;
        if (xway > 0) { char xp[32]; snprintf(xp, sizeof(xp), "%d", xway); x_fd = wl_dir_fd_count(xp, NULL); }
        if (h_fd > 0) { g_wl_hypr_pid = hypr; g_wl_hypr_fd = h_fd; }
        if (h_fd > 20000 && !fired) fired = true; 
        prev_hypr_fd = h_fd > 0 ? h_fd : prev_hypr_fd;
        wl_trace("SAMPLE",
            "self_fd=%d(+%d) self_pipe=%d | hypr_pid=%d fds=%d(+%d) pipes=%d rss=%dmb | xwayland_fd=%d | ev=%ld req=%ld send=%ld recv=%ld prepfail=%ld selreq=%ld rcalls=%ld rblocked=%ld readoff=%d",
            self_fd, self_fd - base_self, self_pipe, hypr, h_fd, h_fd - base_hypr, h_pipe, h_rss, x_fd,
            g_wl_ev_count, g_wl_req_count, g_wl_send_count, g_wl_recv_count, g_wl_prepare_fail, g_cl_selreq,
            g_wl_receive_calls, g_wl_receive_blocked, g_wl_read_disabled ? 1 : 0);
    }
    { PROFILE_END_VAR(_pt, "wl_sampler_func"); return NULL; }
}

static void wl_sampler_start(void) {
        PROFILE_START(_pt);
static bool started = false;
    if (started) { PROFILE_END_VAR(_pt, "wl_sampler_start"); return ; }
    started = true;
    pthread_t t;
    if (pthread_create(&t, NULL, wl_sampler_func, NULL) != 0);
    else wl_trace("SAMPLER", "started");
}

static void wl_hypr_refresh(void) {
        PROFILE_START(_pt);
g_wl_hypr_pid = wl_find_pid("Hyprland");
    if (g_wl_hypr_pid <= 0) { g_wl_hypr_fd = -1; return; }
    char hp[32];
    snprintf(hp, sizeof(hp), "%ld", g_wl_hypr_pid);
    g_wl_hypr_fd = wl_dir_fd_count(hp, NULL);
}

static const char *wl_read_guard(const void *offer) {
        PROFILE_START(_pt);
if (g_wl_read_disabled) { PROFILE_END_VAR(_pt, "wl_read_guard"); return g_wl_read_block_reason[0] ? g_wl_read_block_reason : "read path disabled"; }
    if (!offer) { PROFILE_END_VAR(_pt, "wl_read_guard"); return "no offer"; }
    if (g_wl_hypr_pid <= 0) wl_hypr_refresh();
    if (g_wl_hypr_pid > 0) {
        snprintf(g_wl_read_block_reason, sizeof(g_wl_read_block_reason),
            "hyprland pid %ld present (data-control receive crashes it)", g_wl_hypr_pid);
        g_wl_read_disabled = true;
        return g_wl_read_block_reason;
    }
    if (offer == g_wl_dead_offer) { PROFILE_END_VAR(_pt, "wl_read_guard"); return "offer already produced no data"; }
    if (g_wl_receive_calls >= WL_MAX_RECEIVE_CALLS) { PROFILE_END_VAR(_pt, "wl_read_guard"); return "session receive cap reached"; }
    if (g_wl_hypr_fd > WL_HYPR_FD_ALARM) { PROFILE_END_VAR(_pt, "wl_read_guard"); return "compositor fd count already elevated"; }
    { PROFILE_END_VAR(_pt, "wl_read_guard"); return NULL; }
}

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

static const struct wl_interface g_iface_ext_source;
static const struct wl_interface g_iface_ext_offer;
static const struct wl_interface g_iface_ext_device;
static const struct wl_interface g_iface_ext_manager;

static const struct wl_interface *g_ext_src_offer_types[] = { NULL };
static const struct wl_interface *g_ext_mgr_create_types[] = { &g_iface_ext_source };
static const struct wl_interface *g_ext_mgr_device_types[] = { &g_iface_ext_device, NULL };
static const struct wl_interface *g_ext_dev_sel_types[] = { &g_iface_ext_source };
static const struct wl_interface *g_ext_dev_offer_types[] = { &g_iface_ext_offer };
static const struct wl_interface *g_ext_dev_psel_types[] = { &g_iface_ext_offer };

static const struct wl_message g_ext_source_requests[] = {
    { "offer", "s", g_ext_src_offer_types },
    { "destroy", "", NULL },
};
static const struct wl_message g_ext_source_events[] = {
    { "send", "sh", NULL },
    { "cancelled", "", NULL },
};
static const struct wl_interface g_iface_ext_source = {
    "ext_data_control_source_v1", 1, 2, g_ext_source_requests, 2, g_ext_source_events
};

static const struct wl_message g_ext_offer_requests[] = {
    { "receive", "sh", NULL },
    { "destroy", "", NULL },
};
static const struct wl_message g_ext_offer_events[] = {
    { "offer", "s", NULL },
};
static const struct wl_interface g_iface_ext_offer = {
    "ext_data_control_offer_v1", 1, 2, g_ext_offer_requests, 1, g_ext_offer_events
};

static const struct wl_message g_ext_device_requests[] = {
    { "set_selection", "?o", g_ext_dev_sel_types },
    { "destroy", "", NULL },
    { "set_primary_selection", "?o", g_ext_dev_sel_types },
};
static const struct wl_message g_ext_device_events[] = {
    { "data_offer", "n", g_ext_dev_offer_types },
    { "selection", "?o", g_ext_dev_psel_types },
    { "finished", "", NULL },
    { "primary_selection", "?o", g_ext_dev_psel_types },
};
static const struct wl_interface g_iface_ext_device = {
    "ext_data_control_device_v1", 1, 4, g_ext_device_requests, 4, g_ext_device_events
};

static const struct wl_message g_ext_manager_requests[] = {
    { "create_data_source", "n", g_ext_mgr_create_types },
    { "get_data_device", "no", g_ext_mgr_device_types },
    { "destroy", "", NULL },
};
static const struct wl_interface g_iface_ext_manager = {
    "ext_data_control_manager_v1", 1, 3, g_ext_manager_requests, 0, NULL
};

static const struct wl_interface *g_use_manager = &g_iface_manager;
static const struct wl_interface *g_use_device = &g_iface_device;
static const struct wl_interface *g_use_source = &g_iface_source;
static const char *g_use_mgr_name = "zwlr_data_control_manager_v1";

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
static const char *g_mgr_proto = "none";
static bool g_mgr_ext = false;
static int g_mgr_bind_ver = 0;
static char *g_wl_text = NULL;
static unsigned char *g_wl_png = NULL;
static size_t g_wl_png_len = 0;
static char g_wl_png_mime[32] = "image/png";
static char *g_wl_uri_list = NULL;
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
        PROFILE_START(_pt);
(void)data;
    (void)reg;
    g_wl_ev_count++;
    wl_trace("EV registry.global", "name=%u iface='%s' version=%u (mgr=%u proto=%s ext=%d)", name, iface ? iface : "?", version, g_mgr_name, g_mgr_proto, g_mgr_ext ? 1 : 0);
    if (strcmp(iface, "zwlr_data_control_manager_v1") == 0 || strcmp(iface, "ext_data_control_manager_v1") == 0) {
        bool is_ext = strcmp(iface, "ext_data_control_manager_v1") == 0;
        bool take = g_mgr_name == 0 || version > g_mgr_ver || (version == g_mgr_ver && !is_ext && g_mgr_ext);
        if (take) {
            g_mgr_name = name;
            g_mgr_ver = version;
            g_mgr_proto = is_ext ? "ext" : "zwlr";
            g_mgr_ext = is_ext;
        } else {
            wl_trace("EV registry.global", "SKIP data-control manager iface='%s' version=%u (have '%s' v%u)", iface, version, g_mgr_proto, g_mgr_ver);
        }
    } else if (strcmp(iface, "wl_seat") == 0 && g_seat_name == 0) {
        g_seat_name = name;
    }
}

static void reg_global_remove(void *data, struct wl_proxy *reg, uint32_t name) {
        PROFILE_START(_pt);
(void)data;
    (void)reg;
    (void)name;
    g_wl_ev_count++;
    wl_trace("EV registry.global_remove", "name=%u mgr=%u", name, g_mgr_name);
}

static void (*g_reg_listener[])(void) = {
    (void (*)(void))reg_global,
    (void (*)(void))reg_global_remove,
};

static void offer_mime(void *data, struct wl_proxy *offer, const char *mime) {
        PROFILE_START(_pt);
(void)data;
    (void)offer;
    if (!mime) { PROFILE_END_VAR(_pt, "offer_mime"); return ; }
    g_wl_ev_count++;
    wl_trace("EV offer.mime", "offer=%p mime='%s' idx_before=%d", (void*)offer, mime, g_wl_offer_mime_count);
    pthread_mutex_lock(&g_wl_text_lock);
    if (g_wl_offer_mime_count < 8) {
        strncpy(g_wl_offer_mimes[g_wl_offer_mime_count], mime, sizeof(g_wl_offer_mimes[0]) - 1);
        g_wl_offer_mimes[g_wl_offer_mime_count][sizeof(g_wl_offer_mimes[0]) - 1] = '\0';
        g_wl_offer_mime_count++;
    }
    pthread_mutex_unlock(&g_wl_text_lock);
    wl_trace("EV offer.mime", "offer=%p stored idx_after=%d", (void*)offer, g_wl_offer_mime_count);
}

static void (*g_offer_listener[])(void) = {
    (void (*)(void))offer_mime,
};

static void dev_data_offer(void *data, struct wl_proxy *dev, struct wl_proxy *offer) {
        PROFILE_START(_pt);
(void)data;
    (void)dev;
    g_wl_ev_count++;
    g_wl_recv_count++;
    wl_trace("EV device.data_offer", "dev=%p offer=%p prev_offer=%p prev_source=%p prev_psource=%p",
        (void*)dev, (void*)offer, (void*)g_wl_offer, (void*)g_wl_source, (void*)g_wl_psource);
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
    wl_trace("EV device.data_offer", "installed listener on offer=%p", (void*)offer);
}

static void dev_selection(void *data, struct wl_proxy *dev, struct wl_proxy *offer) {
        PROFILE_START(_pt);
(void)data;
    (void)dev;
    g_wl_ev_count++;
    g_wl_recv_count++;
    wl_trace("EV device.selection", "dev=%p offer=%p cur_offer=%p -> has_selection=%d",
        (void*)dev, (void*)offer, (void*)g_wl_offer, offer != NULL ? 1 : 0);
    g_wl_has_selection = offer != NULL;
}

static void dev_finished(void *data, struct wl_proxy *dev) {
        PROFILE_START(_pt);
(void)data;
    g_wl_ev_count++;
    wl_trace("EV device.finished", "dev=%p display_err=%d", (void*)dev,
        (g_wl_dpy && wl.display_get_error) ? wl.display_get_error(g_wl_dpy) : -1);
}

static void dev_primary(void *data, struct wl_proxy *dev, struct wl_proxy *offer) {
        PROFILE_START(_pt);
(void)data;
    (void)dev;
    (void)offer;
    g_wl_ev_count++;
    g_wl_recv_count++;
    wl_trace("EV device.primary_selection", "dev=%p offer=%p", (void*)dev, (void*)offer);
}

static void (*g_dev_listener[])(void) = {
    (void (*)(void))dev_data_offer,
    (void (*)(void))dev_selection,
    (void (*)(void))dev_finished,
    (void (*)(void))dev_primary,
};

static void src_send(void *data, struct wl_proxy *src, const char *mime, int fd) {
        PROFILE_START(_pt);
(void)data;
    g_wl_ev_count++;
    g_wl_send_count++;
    wl_trace("EV source.send", "src=%p is_source=%d is_psource=%d mime='%s' fd=%d text=%d png_len=%d",
        (void*)src, src == g_wl_source ? 1 : 0, src == g_wl_psource ? 1 : 0, mime ? mime : "?",
        fd, g_wl_text ? (int)strlen(g_wl_text) : -1, (int)g_wl_png_len);
    pthread_mutex_lock(&g_wl_text_lock);
    char *text = g_wl_text ? strdup(g_wl_text) : NULL;
    char *uri = g_wl_uri_list ? strdup(g_wl_uri_list) : NULL;
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
    if (mime && strcmp(mime, "text/uri-list") == 0) {
        buf = (const unsigned char *)uri;
        n = uri ? strlen(uri) : 0;
    } else if (mime && strcmp(mime, g_wl_png_mime) == 0) {
        buf = png;
        n = png_len;
    } else {
        buf = (const unsigned char *)text;
        n = text ? strlen(text) : 0;
    }
    size_t off = 0;
    int writes = 0;
    int gaveup = 0;
    while (off < n) {
        struct pollfd wp = { fd, POLLOUT, 0 };
        int pr = poll(&wp, 1, 200);
        if (pr == 0) { gaveup = 1; wl_trace("EV source.send", "write would block after %d bytes, abandoning", (int)off); break; }
        if (pr < 0) { wl_trace("EV source.send", "poll ERR %d errno=%d", pr, errno); break; }
        ssize_t w = write(fd, buf + off, n - off);
        writes++;
        if (w <= 0) {
            if (w < 0 && (errno == EAGAIN || errno == EINTR)) { if (errno == EINTR) continue; }
            wl_trace("EV source.send", "write FAILED off=%d of %d errno=%d (%s)", (int)off, (int)n, errno, strerror(errno));
            break;
        }
        off += (size_t)w;
    }
    int cerr = close(fd);
    wl_trace("EV source.send", "done mime=%s bytes=%d writes=%d close_ret=%d abandoned=%d", mime ? mime : "?", (int)off, writes, cerr, gaveup);
    free(text);
    free(png);
    free(uri);
}

static void src_cancelled(void *data, struct wl_proxy *src) {
        PROFILE_START(_pt);
(void)data;
    g_wl_ev_count++;
    wl_trace("EV source.cancelled", "src=%p is_source=%d is_psource=%d", (void*)src,
        src == g_wl_source ? 1 : 0, src == g_wl_psource ? 1 : 0);
    if (src == g_wl_source) g_wl_source = NULL;
    if (src == g_wl_psource) g_wl_psource = NULL;
    wl.proxy_marshal_flags(src, 1, NULL, 0, 0);
    wl.proxy_destroy(src);
}

static void (*g_src_listener[])(void) = {
    (void (*)(void))src_send,
    (void (*)(void))src_cancelled,
};

static bool wl_load(void) {
        PROFILE_START(_pt);
if (g_wl_handle) { PROFILE_END_VAR(_pt, "wl_load"); return true; }
    dlerror();
    void *h = dlopen("libwayland-client.so.0", RTLD_NOW);
    if (!h) {
        dlerror();
        h = dlopen("/usr/lib/libwayland-client.so.0", RTLD_NOW);
    }
    if (!h) {
        dlerror();
        h = dlopen("libwayland-client.so", RTLD_NOW);
    }
    if (!h) {
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
        dlclose(h);
        return false;
    }
    wl = t;
    g_wl_handle = h;
    { PROFILE_END_VAR(_pt, "wl_load"); return true; }
}

static void *wl_thread_func(void *arg) {
        PROFILE_START(_pt);
(void)arg;
    int iter = 0;
    while (g_wl_running) {
        iter++;
        int dpyfd = wl.display_get_fd(g_wl_dpy);
        pthread_mutex_lock(&g_wl_lock);
        int spin = 0;
        int pr = 0;
        while (g_wl_running && (pr = wl.display_prepare_read(g_wl_dpy)) != 0) {
            spin++;
            g_wl_prepare_fail++;
            int dp = wl.display_dispatch_pending(g_wl_dpy);
            if (spin == 1 || spin % 2000 == 0) {
                wl_trace("THREAD", "SPINNING prepare_read=%d errno=%d dperr=%d spin=%d iter=%d",
                    pr, errno, wl.display_get_error(g_wl_dpy), spin, iter);
            }
            if (dp < 0) { wl_trace("THREAD", "dispatch_pending FAILED %d during prepare_read spin", dp); break; }
        }
        pthread_mutex_unlock(&g_wl_lock);
        if (!g_wl_running) break;
        struct pollfd pfd = { dpyfd, POLLIN, 0 };
        int r = poll(&pfd, 1, 100);
        int re = 0, dp = 0;
        pthread_mutex_lock(&g_wl_lock);
        if (r == 0) {
            wl.display_cancel_read(g_wl_dpy);
        } else {
            re = wl.display_read_events(g_wl_dpy);
            if (re < 0) {
                wl_trace("THREAD", "read_events FAILED %d errno=%d dperr=%d iter=%d", re, errno, wl.display_get_error(g_wl_dpy), iter);
                pthread_mutex_unlock(&g_wl_lock);
                break;
            }
            dp = wl.display_dispatch_pending(g_wl_dpy);
            if (dp < 0) {
                wl_trace("THREAD", "dispatch_pending FAILED %d dperr=%d iter=%d", dp, wl.display_get_error(g_wl_dpy), iter);
                pthread_mutex_unlock(&g_wl_lock);
                break;
            }
        }
        pthread_mutex_unlock(&g_wl_lock);
        if (iter % 50 == 0 || r > 0) {
            wl_trace("THREAD", "iter=%d dpyfd=%d poll=%d revents=%d re=%d dp=%d spin=%d dperr=%d ev=%ld",
                iter, dpyfd, r, pfd.revents, re, dp, spin, wl.display_get_error(g_wl_dpy), g_wl_ev_count);
        }
    }
    { PROFILE_END_VAR(_pt, "wl_thread_func"); return NULL; }
}

static bool wl_setup(void) {
        PROFILE_START(_pt);
if (g_wl_ready) { PROFILE_END_VAR(_pt, "wl_setup"); return true; }
    if (g_wl_dead) { wl_trace("SETUP", "already dead, refusing"); return false; }
    if (!wl_load()) {
        g_wl_dead = true;
        return false;
    }
    struct wl_display *dpy = wl.display_connect(NULL);
    if (!dpy) {
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
        g_wl_dead = true;
        return false;
    }
    g_mgr_bind_ver = g_mgr_ver > 2 ? 2 : (int)g_mgr_ver;
    if (g_mgr_ext) {
        g_use_manager = &g_iface_ext_manager;
        g_use_device = &g_iface_ext_device;
        g_use_source = &g_iface_ext_source;
        g_use_mgr_name = "ext_data_control_manager_v1";
    }
    pthread_mutex_lock(&g_wl_lock);
    g_wl_manager = wl.proxy_marshal_flags(reg, 0, g_use_manager, (uint32_t)g_mgr_bind_ver, 0,
        g_mgr_name, g_use_mgr_name, (uint32_t)g_mgr_bind_ver, NULL);
    struct wl_proxy *seat = wl.proxy_marshal_flags(reg, 0, &g_iface_seat, 1, 0,
        g_seat_name, "wl_seat", 1, NULL);
    if (!g_wl_manager || !seat) {
        pthread_mutex_unlock(&g_wl_lock);
        if (seat) wl.proxy_destroy(seat);
        if (g_wl_manager) wl.proxy_destroy(g_wl_manager);
        g_wl_manager = NULL;
        wl.proxy_destroy(reg);
        wl.display_disconnect(dpy);
        g_wl_dead = true;
        return false;
    }
    g_wl_device = wl.proxy_marshal_constructor(g_wl_manager, 1, g_use_device, NULL, seat);
    if (!g_wl_device) {
        pthread_mutex_unlock(&g_wl_lock);
        wl.proxy_destroy(seat);
        wl.proxy_destroy(g_wl_manager);
        g_wl_manager = NULL;
        wl.proxy_destroy(reg);
        wl.display_disconnect(dpy);
        g_wl_dead = true;
        return false;
    }
    wl.proxy_add_listener(g_wl_device, (void (**)(void))g_dev_listener, NULL);
    g_wl_seat = seat;
    wl.display_roundtrip(dpy);
    int werr = wl.display_get_error(dpy);
    if (werr != 0) {
        pthread_mutex_unlock(&g_wl_lock);
        wl.proxy_destroy(seat);
        wl.proxy_destroy(g_wl_manager);
        g_wl_manager = NULL;
        g_wl_device = NULL;
        g_wl_seat = NULL;
        wl.proxy_destroy(reg);
        wl.display_disconnect(dpy);
        g_wl_dead = true;
        return false;
    }
    pthread_mutex_unlock(&g_wl_lock);
    wl.proxy_destroy(reg);
    g_wl_dpy = dpy;
    g_wl_running = true;
    if (pthread_create(&g_wl_thread, NULL, wl_thread_func, NULL) != 0) {
        wl.display_disconnect(dpy);
        g_wl_dpy = NULL;
        g_wl_running = false;
        g_wl_dead = true;
        return false;
    }
    g_wl_thread_started = true;
    g_wl_ready = true;
    wl_sampler_start();
    wl_trace("SETUP", "READY dpy=%p dpyfd=%d mgr=%p dev=%p seat=%p bind_ver=%d proto=%s",
        (void*)g_wl_dpy, wl.display_get_fd(g_wl_dpy), (void*)g_wl_manager, (void*)g_wl_device,
        (void*)g_wl_seat, g_mgr_bind_ver, g_mgr_proto);
    wl_hypr_refresh();
    wl_trace("SETUP", "read path state: hyprland_pid=%ld hyprland_fd=%ld read_disabled=%d receive_cap=%d",
        g_wl_hypr_pid, g_wl_hypr_fd, g_wl_read_disabled ? 1 : 0, WL_MAX_RECEIVE_CALLS);
    if (g_wl_hypr_pid > 0) {
        g_wl_read_disabled = true;
        snprintf(g_wl_read_block_reason, sizeof(g_wl_read_block_reason),
            "hyprland pid %ld present (data-control receive crashes it)", g_wl_hypr_pid);
    }
    { PROFILE_END_VAR(_pt, "wl_setup"); return true; }
}

static struct wl_proxy *wl_make_source(void) {
        PROFILE_START(_pt);
struct wl_proxy *src = wl.proxy_marshal_constructor(g_wl_manager, 0, g_use_source, NULL);
    g_wl_req_count++;
    pthread_mutex_lock(&g_wl_text_lock);
    bool has_text = g_wl_text != NULL;
    bool has_png = g_wl_png != NULL && g_wl_png_len > 0;
    bool has_uri = g_wl_uri_list != NULL;
    pthread_mutex_unlock(&g_wl_text_lock);
    wl_trace("REQ manager.create_data_source", "src=%p has_text=%d has_png=%d", (void*)src, has_text ? 1 : 0, has_png ? 1 : 0);
    if (has_text) {
        const char *mimes[] = { "text/plain;charset=utf-8", "text/plain", "TEXT", "STRING", "UTF8_STRING" };
        for (int i = 0; i < 5; i++) {
            wl.proxy_marshal_flags(src, 0, NULL, 0, 0, mimes[i]);
            g_wl_req_count++;
        }
    }
    if (has_uri) { wl.proxy_marshal_flags(src, 0, NULL, 0, 0, "text/uri-list"); g_wl_req_count++; }
    if (has_png) { wl.proxy_marshal_flags(src, 0, NULL, 0, 0, g_wl_png_mime); g_wl_req_count++; }
    wl.proxy_add_listener(src, (void (**)(void))g_src_listener, NULL);
    { PROFILE_END_VAR(_pt, "wl_make_source"); return src; }
}

static void wl_drop_source(struct wl_proxy **slot) {
        PROFILE_START(_pt);
if (slot && *slot) {
        g_wl_req_count++;
        wl_trace("REQ source.destroy", "proxy=%p", (void*)*slot);
        wl.proxy_marshal_flags(*slot, 1, NULL, 0, 0);
        wl.proxy_destroy(*slot);
        *slot = NULL;
    }
}

bool Platform_ClipboardAvailable(void) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "Platform_ClipboardAvailable"); return getenv("WAYLAND_DISPLAY") != NULL; }
}

static void wl_publish_locked(void) {
        PROFILE_START(_pt);
g_wl_req_count += 2;
    wl_trace("REQ publish.begin", "old_source=%p old_psource=%p", (void*)g_wl_source, (void*)g_wl_psource);
    wl_drop_source(&g_wl_source);
    wl_drop_source(&g_wl_psource);
    pthread_mutex_lock(&g_wl_text_lock);
    bool has_any = g_wl_text != NULL || (g_wl_png != NULL && g_wl_png_len > 0);
    pthread_mutex_unlock(&g_wl_text_lock);
    if (!has_any) { wl_trace("REQ publish.abort", "no text and no png"); return; }
    g_wl_source = wl_make_source();
    wl_trace("REQ device.set_selection", "src=%p", (void*)g_wl_source);
    wl.proxy_marshal_flags(g_wl_device, 0, NULL, 0, 0, g_wl_source);
    if (g_mgr_bind_ver >= 2) {
        g_wl_psource = wl_make_source();
        wl_trace("REQ device.set_primary_selection", "src=%p bind_ver=%d", (void*)g_wl_psource, g_mgr_bind_ver);
        wl.proxy_marshal_flags(g_wl_device, 2, NULL, 0, 0, g_wl_psource);
    } else {
        wl_trace("REQ device.set_primary_selection SKIPPED", "bind_ver=%d < 2", g_mgr_bind_ver);
    }
    int fr = wl.display_flush(g_wl_dpy);
    wl_trace("REQ publish.end", "flush=%d dperr=%d", fr, wl.display_get_error(g_wl_dpy));
}

int Platform_ClipboardSetText(const char *text) {
        PROFILE_START(_pt);
if (!text) { PROFILE_END_VAR(_pt, "Platform_ClipboardSetText"); return 0; }
    wl_trace("API SetText", "len=%d text='%s'", (int)strlen(text), text);
    pthread_mutex_lock(&g_wl_text_lock);
    bool same = g_wl_text != NULL && strcmp(g_wl_text, text) == 0 && g_wl_png == NULL;
    pthread_mutex_unlock(&g_wl_text_lock);
    if (same && g_wl_source != NULL) {
        wl_trace("API SetText", "SKIPPED identical text already published (source=%p)", (void*)g_wl_source);
        return 1;
    }
    if (!wl_setup()) { wl_trace("API SetText", "setup failed, returning 0"); return 0; }
    pthread_mutex_lock(&g_wl_text_lock);
    char* nt = strdup(text);
    if (nt) {
        free(g_wl_text);
        g_wl_text = nt;
        free(g_wl_png);
        g_wl_png = NULL;
        g_wl_png_len = 0;
        free(g_wl_uri_list);
        g_wl_uri_list = NULL;
    }
    pthread_mutex_unlock(&g_wl_text_lock);
    if (!nt) { PROFILE_END_VAR(_pt, "Platform_ClipboardSetText"); return 0; }
    long long t0 = 0;
    struct timespec ts0, ts1;
    clock_gettime(CLOCK_MONOTONIC, &ts0);
    pthread_mutex_lock(&g_wl_lock);
    wl_publish_locked();
    pthread_mutex_unlock(&g_wl_lock);
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    t0 = (ts1.tv_sec - ts0.tv_sec) * 1000 + (ts1.tv_nsec - ts0.tv_nsec) / 1000000;
    wl_trace("API SetText", "DONE lock+publish took %lldms", t0);
    { PROFILE_END_VAR(_pt, "Platform_ClipboardSetText"); return 1; }
}

int Platform_ClipboardSetImage(const unsigned char *png, size_t len) {
        PROFILE_START(_pt);
if (!png || len == 0) { PROFILE_END_VAR(_pt, "Platform_ClipboardSetImage"); return 0; }
    if (!wl_setup()) { PROFILE_END_VAR(_pt, "Platform_ClipboardSetImage"); return 0; }
    unsigned char* np = malloc(len);
    if (np) memcpy(np, png, len);
    if (!np) { PROFILE_END_VAR(_pt, "Platform_ClipboardSetImage"); return 0; }
    const char* mime = clipboard_image_mime(clipboard_image_format(png, len));
    pthread_mutex_lock(&g_wl_text_lock);
    free(g_wl_png);
    g_wl_png = np;
    g_wl_png_len = len;
    snprintf(g_wl_png_mime, sizeof(g_wl_png_mime), "%s", mime ? mime : "image/png");
    free(g_wl_text);
    g_wl_text = NULL;
    free(g_wl_uri_list);
    g_wl_uri_list = NULL;
    pthread_mutex_unlock(&g_wl_text_lock);
    pthread_mutex_lock(&g_wl_lock);
    wl_publish_locked();
    pthread_mutex_unlock(&g_wl_lock);
    { PROFILE_END_VAR(_pt, "Platform_ClipboardSetImage"); return 1; }
}

int Platform_ClipboardSetFile(const char *path) {
        PROFILE_START(_pt);
if (!path || path[0] == 0) { PROFILE_END_VAR(_pt, "Platform_ClipboardSetFile"); return 0; }
    char *uri = clipboard_path_to_uri_list(path);
    if (!uri) { PROFILE_END_VAR(_pt, "Platform_ClipboardSetFile"); return 0; }
    if (!wl_setup()) { free(uri); return 0; }
    pthread_mutex_lock(&g_wl_text_lock);
    free(g_wl_text);
    g_wl_text = NULL;
    free(g_wl_png);
    g_wl_png = NULL;
    g_wl_png_len = 0;
    free(g_wl_uri_list);
    g_wl_uri_list = uri;
    pthread_mutex_unlock(&g_wl_text_lock);
    pthread_mutex_lock(&g_wl_lock);
    wl_publish_locked();
    pthread_mutex_unlock(&g_wl_lock);
    { PROFILE_END_VAR(_pt, "Platform_ClipboardSetFile"); return 1; }
}

static const char *wl_pick_mime_locked(void) {
        PROFILE_START(_pt);
static const char *prefs[] = { "text/plain;charset=utf-8", "text/plain", "UTF8_STRING", "STRING", "TEXT" };
    for (size_t p = 0; p < sizeof(prefs) / sizeof(prefs[0]); p++) {
        for (int i = 0; i < g_wl_offer_mime_count; i++) {
            if (strcmp(g_wl_offer_mimes[i], prefs[p]) == 0) return prefs[p];
        }
    }
    { PROFILE_END_VAR(_pt, "wl_pick_mime_locked"); return NULL; }
}

char *Platform_ClipboardGetText(void) {
        PROFILE_START(_pt);
wl_trace("API GetText", "enter offer=%p has_selection=%d mime_count=%d",
        (void*)g_wl_offer, g_wl_has_selection ? 1 : 0, g_wl_offer_mime_count);
    if (!wl_setup()) { wl_trace("API GetText", "setup failed"); return NULL; }
    pthread_mutex_lock(&g_wl_lock);
    struct wl_proxy *offer = (g_wl_offer && g_wl_has_selection) ? g_wl_offer : NULL;
    pthread_mutex_lock(&g_wl_text_lock);
    const char *pick = offer ? wl_pick_mime_locked() : NULL;
    char mime[64] = {0};
    if (pick) strncpy(mime, pick, sizeof(mime) - 1);
    pthread_mutex_unlock(&g_wl_text_lock);
    if (!offer || !mime[0]) {
        pthread_mutex_unlock(&g_wl_lock);
        wl_trace("API GetText", "NO OFFER offer=%p has_selection=%d mime_count=%d picked='%s' dperr=%d",
            (void*)g_wl_offer, g_wl_has_selection ? 1 : 0, g_wl_offer_mime_count, mime, wl.display_get_error(g_wl_dpy));
        return NULL;
    }
    int fds[2] = {-1, -1};
    const char *blocked = wl_read_guard(offer);
    if (blocked) {
        g_wl_receive_blocked++;
        g_wl_dead_offer = offer;
        pthread_mutex_unlock(&g_wl_lock);
        wl_trace("API GetText", "RECEIVE BLOCKED reason='%s' offer=%p blocked_total=%ld",
            blocked, (void*)offer, g_wl_receive_blocked);
        return NULL;
    }
    if (pipe(fds) != 0) {
        pthread_mutex_unlock(&g_wl_lock);
        wl_trace("API GetText", "pipe FAILED errno=%d", errno);
        return NULL;
    }
    g_wl_receive_calls++;
    g_wl_req_count++;
    wl_trace("REQ offer.receive", "offer=%p mime='%s' rfd=%d wfd=%d call=%ld", (void*)offer, mime, fds[0], fds[1], g_wl_receive_calls);
    wl.proxy_marshal_flags(offer, 0, NULL, 0, 0, mime, fds[1]);
    int fr = wl.display_flush(g_wl_dpy);
    wl_trace("REQ offer.receive", "flush=%d dperr=%d", fr, wl.display_get_error(g_wl_dpy));
    pthread_mutex_unlock(&g_wl_lock);
    close(fds[1]);
    size_t cap = 8192;
    size_t len = 0;
    char *out = malloc(cap);
    if (!out) {
        close(fds[0]);
        return NULL;
    }
    int timeouts = 0, reads = 0, eofs = 0;
    for (;;) {
        struct pollfd pfd = { fds[0], POLLIN, 0 };
        int r = poll(&pfd, 1, 500);
        if (r == 0) { timeouts++; wl_trace("API GetText", "poll TIMEOUT len=%d timeouts=%d", (int)len, timeouts); break; }
        if (r < 0) { wl_trace("API GetText", "poll ERR %d errno=%d", r, errno); break; }
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
        reads++;
        if (got <= 0) { eofs = 1; wl_trace("API GetText", "read EOF got=%d errno=%d after %d bytes", (int)got, errno, (int)len); break; }
        len += (size_t)got;
    }
    close(fds[0]);
    wl_trace("API GetText", "result len=%d timeouts=%d reads=%d eof=%d", (int)len, timeouts, reads, eofs);
    if (len == 0) {
        free(out);
        g_wl_dead_offer = offer;
        wl_trace("API GetText", "marking offer=%p dead, no further receive from it", (void*)offer);
        return NULL;
    }
    out[len] = '\0';
    { PROFILE_END_VAR(_pt, "Platform_ClipboardGetText"); return out; }
}

void Platform_ClipboardShutdown(void) {
        PROFILE_START(_pt);
if (!g_wl_ready && !g_wl_handle) { PROFILE_END_VAR(_pt, "Platform_ClipboardShutdown"); return ; }
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
}
#endif
