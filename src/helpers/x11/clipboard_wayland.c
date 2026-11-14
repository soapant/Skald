#include "core/timer.h"
#include "helpers/helpers_text.h"
#include <time.h>
#if defined(SKALD_WAYLAND_SESSION)
#include "platform/x11/clipboard_backend_x11.h"
#include "helpers/x11/env_helpers_x11.h"
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

static long wl_ev_count = 0;
static long wl_req_count = 0;
static long wl_send_count = 0;
static long wl_recv_count = 0;
static long wl_prepare_fail = 0;
static long wl_receive_calls = 0;
static long wl_receive_blocked = 0;
static const void *wl_dead_offer = NULL;
static long wl_hypr_pid = -1;
static long wl_hypr_fd = -1;
static bool wl_read_disabled = false;
static char wl_read_block_reason[128] = {0};
#define WL_MAX_RECEIVE_CALLS 3
#define WL_HYPR_FD_ALARM 20000

static void wl_trace(const char *what, const char *fmt, ...) {

  (void)what;
  (void)fmt;
}

static int wl_dir_fd_count(const char *pid, const char *want) {

  char p[64];
  snprintf(p, sizeof(p), "/proc/%s/fd", pid);
  DIR *d = opendir(p);
  if (!d)
    return -1;
  int total = 0, pipes = 0;
  const struct dirent *e;
  while ((e = readdir(d))) {
    if (e->d_name[0] == '.')
      continue;
    total++;
    char lp[128];
    snprintf(lp, sizeof(lp), "/proc/%s/fd/%s", pid, e->d_name);
    char link[256];
    ssize_t r = readlink(lp, link, sizeof(link) - 1);
    if (r > 0) {
      link[r] = '\0';
      if (strncmp(link, "pipe:", 5) == 0)
        pipes++;
    }
  }
  closedir(d);
  if (want && strcmp(want, "pipes") == 0)
    return pipes;
  return total;
}

static int wl_find_pid(const char *name) {

  DIR *d = opendir("/proc");
  if (!d)
    return -1;
  int found = -1;
  const struct dirent *e;
  while ((e = readdir(d))) {
    if (e->d_name[0] < '0' || e->d_name[0] > '9')
      continue;
    char p[96];
    snprintf(p, sizeof(p), "/proc/%s/comm", e->d_name);
    FILE *f = fopen(p, "r");
    if (!f)
      continue;
    char buf[64] = {0};
    if (fgets(buf, sizeof(buf), f)) {
      size_t l = strlen(buf);
      while (l && (buf[l - 1] == '\n' || buf[l - 1] == '\r'))
        buf[--l] = '\0';
      if (strcmp(buf, name) == 0) {
        found = atoi(e->d_name);
        fclose(f);
        break;
      }
    }
    fclose(f);
  }
  closedir(d);
  return found;
}

static void wl_hypr_refresh(void) {

  wl_hypr_pid = wl_find_pid("Hyprland");
  if (wl_hypr_pid <= 0) {
    wl_hypr_fd = -1;
    return;
  }
  char hp[32];
  snprintf(hp, sizeof(hp), "%ld", wl_hypr_pid);
  wl_hypr_fd = wl_dir_fd_count(hp, NULL);
}

static const char *wl_read_guard(const void *offer) {

  if (wl_read_disabled)
    return wl_read_block_reason[0] ? wl_read_block_reason
                                   : "read path disabled";
  if (!offer)
    return "no offer";
  if (wl_hypr_pid <= 0)
    wl_hypr_refresh();
  if (wl_hypr_pid > 0) {
    snprintf(wl_read_block_reason, sizeof(wl_read_block_reason),
             "hyprland pid %ld present (data-control receive crashes it)",
             wl_hypr_pid);
    wl_read_disabled = true;
    return wl_read_block_reason;
  }
  if (offer == wl_dead_offer)
    return "offer already produced no data";
  if (wl_receive_calls >= WL_MAX_RECEIVE_CALLS)
    return "session receive cap reached";
  if (wl_hypr_fd > WL_HYPR_FD_ALARM)
    return "compositor fd count already elevated";
  return NULL;
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
  struct wl_proxy *(*proxy_marshal_flags)(struct wl_proxy *, uint32_t,
                                          const struct wl_interface *, uint32_t,
                                          uint32_t, ...);
  struct wl_proxy *(*proxy_marshal_constructor)(struct wl_proxy *, uint32_t,
                                                const struct wl_interface *,
                                                ...);
  void (*proxy_destroy)(struct wl_proxy *);
  int (*display_get_error)(struct wl_display *);
};

static const struct wl_interface *wl_registry_iface = NULL;

static struct WlFns wl;
static void *wl_handle = NULL;

static const struct wl_interface iface_source;
static const struct wl_interface iface_offer;
static const struct wl_interface iface_device;
static const struct wl_interface iface_manager;
static const struct wl_interface iface_seat;

static const struct wl_interface *src_offer_types[] = {NULL};
static const struct wl_interface *mgr_create_types[] = {&iface_source};
static const struct wl_interface *mgr_device_types[] = {&iface_device, NULL};
static const struct wl_interface *dev_sel_types[] = {&iface_source};
static const struct wl_interface *dev_offer_types[] = {&iface_offer};
static const struct wl_interface *dev_psel_types[] = {&iface_offer};

static const struct wl_message source_requests[] = {
    {"offer", "s", src_offer_types},
    {"destroy", "", NULL},
};
static const struct wl_message source_events[] = {
    {"send", "sh", NULL},
    {"cancelled", "", NULL},
};
static const struct wl_interface iface_source = {
    "zwlr_data_control_source_v1", 1, 2, source_requests, 2, source_events};

static const struct wl_message offer_requests[] = {
    {"receive", "sh", NULL},
    {"destroy", "", NULL},
};
static const struct wl_message offer_events[] = {
    {"offer", "s", NULL},
};
static const struct wl_interface iface_offer = {
    "zwlr_data_control_offer_v1", 1, 2, offer_requests, 1, offer_events};

static const struct wl_message device_requests[] = {
    {"set_selection", "?o", dev_sel_types},
    {"destroy", "", NULL},
    {"set_primary_selection", "?o", dev_sel_types},
};
static const struct wl_message device_events[] = {
    {"data_offer", "n", dev_offer_types},
    {"selection", "?o", dev_psel_types},
    {"finished", "", NULL},
    {"primary_selection", "?o", dev_psel_types},
};
static const struct wl_interface iface_device = {
    "zwlr_data_control_device_v1", 2, 3, device_requests, 4, device_events};

static const struct wl_message manager_requests[] = {
    {"create_data_source", "n", mgr_create_types},
    {"get_data_device", "no", mgr_device_types},
    {"destroy", "", NULL},
};
static const struct wl_interface iface_manager = {
    "zwlr_data_control_manager_v1", 2, 3, manager_requests, 0, NULL};

static const struct wl_interface iface_ext_source;
static const struct wl_interface iface_ext_offer;
static const struct wl_interface iface_ext_device;
static const struct wl_interface iface_ext_manager;

static const struct wl_interface *ext_src_offer_types[] = {NULL};
static const struct wl_interface *ext_mgr_create_types[] = {&iface_ext_source};
static const struct wl_interface *ext_mgr_device_types[] = {&iface_ext_device,
                                                            NULL};
static const struct wl_interface *ext_dev_sel_types[] = {&iface_ext_source};
static const struct wl_interface *ext_dev_offer_types[] = {&iface_ext_offer};
static const struct wl_interface *ext_dev_psel_types[] = {&iface_ext_offer};

static const struct wl_message ext_source_requests[] = {
    {"offer", "s", ext_src_offer_types},
    {"destroy", "", NULL},
};
static const struct wl_message ext_source_events[] = {
    {"send", "sh", NULL},
    {"cancelled", "", NULL},
};
static const struct wl_interface iface_ext_source = {
    "ext_data_control_source_v1", 1, 2,
    ext_source_requests,          2, ext_source_events};

static const struct wl_message ext_offer_requests[] = {
    {"receive", "sh", NULL},
    {"destroy", "", NULL},
};
static const struct wl_message ext_offer_events[] = {
    {"offer", "s", NULL},
};
static const struct wl_interface iface_ext_offer = {
    "ext_data_control_offer_v1", 1, 2, ext_offer_requests, 1, ext_offer_events};

static const struct wl_message ext_device_requests[] = {
    {"set_selection", "?o", ext_dev_sel_types},
    {"destroy", "", NULL},
    {"set_primary_selection", "?o", ext_dev_sel_types},
};
static const struct wl_message ext_device_events[] = {
    {"data_offer", "n", ext_dev_offer_types},
    {"selection", "?o", ext_dev_psel_types},
    {"finished", "", NULL},
    {"primary_selection", "?o", ext_dev_psel_types},
};
static const struct wl_interface iface_ext_device = {
    "ext_data_control_device_v1", 1, 4,
    ext_device_requests,          4, ext_device_events};

static const struct wl_message ext_manager_requests[] = {
    {"create_data_source", "n", ext_mgr_create_types},
    {"get_data_device", "no", ext_mgr_device_types},
    {"destroy", "", NULL},
};
static const struct wl_interface iface_ext_manager = {
    "ext_data_control_manager_v1", 1, 3, ext_manager_requests, 0, NULL};

static const struct wl_interface *use_manager = &iface_manager;
static const struct wl_interface *use_device = &iface_device;
static const struct wl_interface *use_source = &iface_source;
static const char *use_mgr_name = "zwlr_data_control_manager_v1";

static const struct wl_message seat_events[] = {
    {"capabilities", "u", NULL},
    {"name", "s", NULL},
};
static const struct wl_interface iface_seat = {"wl_seat", 1, 0,
                                               NULL,      2, seat_events};

static struct wl_display *wl_dpy = NULL;
static struct wl_proxy *wl_manager = NULL;
static struct wl_proxy *wl_seat = NULL;
static struct wl_proxy *wl_device = NULL;
static struct wl_proxy *wl_source = NULL;
static struct wl_proxy *wl_psource = NULL;
static uint32_t mgr_name = 0;
static uint32_t mgr_ver = 0;
static uint32_t seat_name = 0;
static const char *mgr_proto = "none";
static bool mgr_ext = false;
static int mgr_bind_ver = 0;
static char *wl_text = NULL;
static unsigned char *wl_png = NULL;
static size_t wl_png_len = 0;
static char wl_png_mime[32] = "image/png";
static char *wl_uri_list = NULL;
static struct wl_proxy *wl_offer = NULL;
static bool wl_has_selection = false;
static char wl_offer_mimes[8][64];
static int wl_offer_mime_count = 0;
static bool wl_running = false;
static bool wl_ready = false;
static bool wl_dead = false;
static bool wl_thread_started = false;
static pthread_t wl_thread = 0;
static pthread_mutex_t wl_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t wl_text_lock = PTHREAD_MUTEX_INITIALIZER;

static void reg_global(void *data, struct wl_proxy *reg, uint32_t name,
                       const char *iface, uint32_t version) {

  (void)data;
  (void)reg;
  wl_ev_count++;
  wl_trace("EV registry.global",
           "name=%u iface='%s' version=%u (mgr=%u proto=%s ext=%d)", name,
           iface ? iface : "?", version, mgr_name, mgr_proto, mgr_ext ? 1 : 0);
  if (strcmp(iface, "zwlr_data_control_manager_v1") == 0 ||
      strcmp(iface, "ext_data_control_manager_v1") == 0) {
    bool is_ext = strcmp(iface, "ext_data_control_manager_v1") == 0;
    bool take = mgr_name == 0 || version > mgr_ver ||
                (version == mgr_ver && !is_ext && mgr_ext);
    if (take) {
      mgr_name = name;
      mgr_ver = version;
      mgr_proto = is_ext ? "ext" : "zwlr";
      mgr_ext = is_ext;
    } else {
      wl_trace(
          "EV registry.global",
          "SKIP data-control manager iface='%s' version=%u (have '%s' v%u)",
          iface, version, mgr_proto, mgr_ver);
    }
  } else if (strcmp(iface, "wl_seat") == 0 && seat_name == 0) {
    seat_name = name;
  }
}

static void reg_global_remove(void *data, struct wl_proxy *reg, uint32_t name) {

  (void)data;
  (void)reg;
  (void)name;
  wl_ev_count++;
  wl_trace("EV registry.global_remove", "name=%u mgr=%u", name, mgr_name);
}

static void (*reg_listener[])(void) = {
    (void (*)(void))reg_global,
    (void (*)(void))reg_global_remove,
};

static void offer_mime(void *data, struct wl_proxy *offer, const char *mime) {

  (void)data;
  (void)offer;
  if (!mime) {
    return;
  }
  wl_ev_count++;
  wl_trace("EV offer.mime", "offer=%p mime='%s' idx_before=%d", (void *)offer,
           mime, wl_offer_mime_count);
  pthread_mutex_lock(&wl_text_lock);
  if (wl_offer_mime_count < 8) {
    SafeCopy(wl_offer_mimes[wl_offer_mime_count], sizeof(wl_offer_mimes[0]),
             mime);
    wl_offer_mime_count++;
  }
  pthread_mutex_unlock(&wl_text_lock);
  wl_trace("EV offer.mime", "offer=%p stored idx_after=%d", (void *)offer,
           wl_offer_mime_count);
}

static void (*offer_listener[])(void) = {
    (void (*)(void))offer_mime,
};

static void dev_data_offer(void *data, struct wl_proxy *dev,
                           struct wl_proxy *offer) {

  (void)data;
  (void)dev;
  wl_ev_count++;
  wl_recv_count++;
  wl_trace("EV device.data_offer",
           "dev=%p offer=%p prev_offer=%p prev_source=%p prev_psource=%p",
           (void *)dev, (void *)offer, (void *)wl_offer, (void *)wl_source,
           (void *)wl_psource);
  if (wl_offer && wl_offer != offer) {
    wl.proxy_marshal_flags(wl_offer, 1, NULL, 0, 0);
    wl.proxy_destroy(wl_offer);
  }
  wl_offer = offer;
  wl_has_selection = false;
  pthread_mutex_lock(&wl_text_lock);
  wl_offer_mime_count = 0;
  pthread_mutex_unlock(&wl_text_lock);
  wl.proxy_add_listener(offer, (void (**)(void))offer_listener, NULL);
  wl_trace("EV device.data_offer", "installed listener on offer=%p",
           (void *)offer);
}

static void dev_selection(void *data, struct wl_proxy *dev,
                          struct wl_proxy *offer) {

  (void)data;
  (void)dev;
  wl_ev_count++;
  wl_recv_count++;
  wl_trace("EV device.selection",
           "dev=%p offer=%p cur_offer=%p -> has_selection=%d", (void *)dev,
           (void *)offer, (void *)wl_offer, offer != NULL ? 1 : 0);
  wl_has_selection = offer != NULL;
}

static void dev_finished(void *data, struct wl_proxy *dev) {

  (void)data;
  wl_ev_count++;
  wl_trace("EV device.finished", "dev=%p display_err=%d", (void *)dev,
           (wl_dpy && wl.display_get_error) ? wl.display_get_error(wl_dpy)
                                            : -1);
}

static void dev_primary(void *data, struct wl_proxy *dev,
                        struct wl_proxy *offer) {

  (void)data;
  (void)dev;
  (void)offer;
  wl_ev_count++;
  wl_recv_count++;
  wl_trace("EV device.primary_selection", "dev=%p offer=%p", (void *)dev,
           (void *)offer);
}

static void (*dev_listener[])(void) = {
    (void (*)(void))dev_data_offer,
    (void (*)(void))dev_selection,
    (void (*)(void))dev_finished,
    (void (*)(void))dev_primary,
};

static void src_send(void *data, struct wl_proxy *src, const char *mime,
                     int fd) {

  (void)data;
  wl_ev_count++;
  wl_send_count++;
  wl_trace(
      "EV source.send",
      "src=%p is_source=%d is_psource=%d mime='%s' fd=%d text=%d png_len=%d",
      (void *)src, src == wl_source ? 1 : 0, src == wl_psource ? 1 : 0,
      mime ? mime : "?", fd, wl_text ? (int)strlen(wl_text) : -1,
      (int)wl_png_len);
  pthread_mutex_lock(&wl_text_lock);
  char *text = wl_text ? strdup(wl_text) : NULL;
  char *uri = wl_uri_list ? strdup(wl_uri_list) : NULL;
  unsigned char *png = NULL;
  size_t png_len = 0;
  if (wl_png && wl_png_len > 0) {
    png = malloc(wl_png_len);
    if (png) {
      memcpy(png, wl_png, wl_png_len);
      png_len = wl_png_len;
    }
  }
  pthread_mutex_unlock(&wl_text_lock);
  const unsigned char *buf = NULL;
  size_t n = 0;
  if (mime && strcmp(mime, "text/uri-list") == 0) {
    buf = (const unsigned char *)uri;
    n = uri ? strlen(uri) : 0;
  } else if (mime && strcmp(mime, wl_png_mime) == 0) {
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
    struct pollfd wp = {fd, POLLOUT, 0};
    int pr = poll(&wp, 1, 200);
    if (pr == 0) {
      gaveup = 1;
      wl_trace("EV source.send", "write would block after %d bytes, abandoning",
               (int)off);
      break;
    }
    if (pr < 0) {
      wl_trace("EV source.send", "poll ERR %d errno=%d", pr, errno);
      break;
    }
    ssize_t w = write(fd, buf + off, n - off);
    writes++;
    if (w <= 0) {
      if (w < 0 && (errno == EAGAIN || errno == EINTR)) {
        if (errno == EINTR)
          continue;
      }
      wl_trace("EV source.send", "write FAILED off=%d of %d errno=%d (%s)",
               (int)off, (int)n, errno, strerror(errno));
      break;
    }
    off += (size_t)w;
  }
  int cerr = close(fd);
  wl_trace("EV source.send",
           "done mime=%s bytes=%d writes=%d close_ret=%d abandoned=%d",
           mime ? mime : "?", (int)off, writes, cerr, gaveup);
  free(text);
  free(png);
  free(uri);
}

static void src_cancelled(void *data, struct wl_proxy *src) {

  (void)data;
  wl_ev_count++;
  wl_trace("EV source.cancelled", "src=%p is_source=%d is_psource=%d",
           (void *)src, src == wl_source ? 1 : 0, src == wl_psource ? 1 : 0);
  if (src == wl_source)
    wl_source = NULL;
  if (src == wl_psource)
    wl_psource = NULL;
  wl.proxy_marshal_flags(src, 1, NULL, 0, 0);
  wl.proxy_destroy(src);
}

static void (*src_listener[])(void) = {
    (void (*)(void))src_send,
    (void (*)(void))src_cancelled,
};

static bool wl_load(void) {

  if (wl_handle)
    return true;
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
  wl_registry_iface = dlsym(h, "wl_registry_interface");
  if (!t.display_connect || !t.display_disconnect || !t.display_flush ||
      !t.display_roundtrip || !t.display_dispatch_pending ||
      !t.display_prepare_read || !t.display_read_events ||
      !t.display_cancel_read || !t.display_get_fd || !t.proxy_add_listener ||
      !t.proxy_marshal_flags || !t.proxy_marshal_constructor ||
      !t.proxy_destroy || !t.display_get_error || !wl_registry_iface) {
    dlclose(h);
    return false;
  }
  wl = t;
  wl_handle = h;
  return true;
}

static void *wl_thread_func(void *arg) {

  (void)arg;
  int iter = 0;
  while (wl_running) {
    iter++;
    int dpyfd = wl.display_get_fd(wl_dpy);
    pthread_mutex_lock(&wl_lock);
    int spin = 0;
    int pr = 0;
    while (wl_running && (pr = wl.display_prepare_read(wl_dpy)) != 0) {
      spin++;
      wl_prepare_fail++;
      int dp = wl.display_dispatch_pending(wl_dpy);
      if (spin == 1 || spin % 2000 == 0) {
        wl_trace("THREAD",
                 "SPINNING prepare_read=%d errno=%d dperr=%d spin=%d iter=%d",
                 pr, errno, wl.display_get_error(wl_dpy), spin, iter);
      }
      if (dp < 0) {
        wl_trace("THREAD",
                 "dispatch_pending FAILED %d during prepare_read spin", dp);
        break;
      }
    }
    pthread_mutex_unlock(&wl_lock);
    if (!wl_running)
      break;
    struct pollfd pfd = {dpyfd, POLLIN, 0};
    int r = poll(&pfd, 1, 100);
    int re = 0, dp = 0;
    pthread_mutex_lock(&wl_lock);
    if (r == 0) {
      wl.display_cancel_read(wl_dpy);
    } else {
      re = wl.display_read_events(wl_dpy);
      if (re < 0) {
        wl_trace("THREAD", "read_events FAILED %d errno=%d dperr=%d iter=%d",
                 re, errno, wl.display_get_error(wl_dpy), iter);
        pthread_mutex_unlock(&wl_lock);
        break;
      }
      dp = wl.display_dispatch_pending(wl_dpy);
      if (dp < 0) {
        wl_trace("THREAD", "dispatch_pending FAILED %d dperr=%d iter=%d", dp,
                 wl.display_get_error(wl_dpy), iter);
        pthread_mutex_unlock(&wl_lock);
        break;
      }
    }
    pthread_mutex_unlock(&wl_lock);
    if (iter % 50 == 0 || r > 0) {
      wl_trace("THREAD",
               "iter=%d dpyfd=%d poll=%d revents=%d re=%d dp=%d spin=%d "
               "dperr=%d ev=%ld",
               iter, dpyfd, r, pfd.revents, re, dp, spin,
               wl.display_get_error(wl_dpy), wl_ev_count);
    }
  }
  return NULL;
}

static bool wl_setup(void) {

  if (wl_ready)
    return true;
  if (wl_dead) {
    wl_trace("SETUP", "already dead, refusing");
    return false;
  }
  if (!wl_load()) {
    wl_dead = true;
    return false;
  }
  struct wl_display *dpy = wl.display_connect(NULL);
  if (!dpy) {
    wl_dead = true;
    return false;
  }
  struct wl_proxy *reg = wl.proxy_marshal_constructor((struct wl_proxy *)dpy, 1,
                                                      wl_registry_iface);
  wl.proxy_add_listener(reg, (void (**)(void))reg_listener, NULL);
  pthread_mutex_lock(&wl_lock);
  wl.display_roundtrip(dpy);
  pthread_mutex_unlock(&wl_lock);
  if (!mgr_name || !seat_name) {
    wl.proxy_destroy(reg);
    wl.display_disconnect(dpy);
    wl_dead = true;
    return false;
  }
  mgr_bind_ver = mgr_ver > 2 ? 2 : (int)mgr_ver;
  if (mgr_ext) {
    use_manager = &iface_ext_manager;
    use_device = &iface_ext_device;
    use_source = &iface_ext_source;
    use_mgr_name = "ext_data_control_manager_v1";
  }
  pthread_mutex_lock(&wl_lock);
  wl_manager = wl.proxy_marshal_flags(
      reg, 0, use_manager, (uint32_t)mgr_bind_ver, 0, mgr_name, use_mgr_name,
      (uint32_t)mgr_bind_ver, NULL);
  struct wl_proxy *seat = wl.proxy_marshal_flags(reg, 0, &iface_seat, 1, 0,
                                                 seat_name, "wl_seat", 1, NULL);
  if (!wl_manager || !seat) {
    pthread_mutex_unlock(&wl_lock);
    if (seat)
      wl.proxy_destroy(seat);
    if (wl_manager)
      wl.proxy_destroy(wl_manager);
    wl_manager = NULL;
    wl.proxy_destroy(reg);
    wl.display_disconnect(dpy);
    wl_dead = true;
    return false;
  }
  wl_device =
      wl.proxy_marshal_constructor(wl_manager, 1, use_device, NULL, seat);
  if (!wl_device) {
    pthread_mutex_unlock(&wl_lock);
    wl.proxy_destroy(seat);
    wl.proxy_destroy(wl_manager);
    wl_manager = NULL;
    wl.proxy_destroy(reg);
    wl.display_disconnect(dpy);
    wl_dead = true;
    return false;
  }
  wl.proxy_add_listener(wl_device, (void (**)(void))dev_listener, NULL);
  wl_seat = seat;
  wl.display_roundtrip(dpy);
  int werr = wl.display_get_error(dpy);
  if (werr != 0) {
    pthread_mutex_unlock(&wl_lock);
    wl.proxy_destroy(seat);
    wl.proxy_destroy(wl_manager);
    wl_manager = NULL;
    wl_device = NULL;
    wl_seat = NULL;
    wl.proxy_destroy(reg);
    wl.display_disconnect(dpy);
    wl_dead = true;
    return false;
  }
  pthread_mutex_unlock(&wl_lock);
  wl.proxy_destroy(reg);
  wl_dpy = dpy;
  wl_running = true;
  if (pthread_create(&wl_thread, NULL, wl_thread_func, NULL) != 0) {
    wl.display_disconnect(dpy);
    wl_dpy = NULL;
    wl_running = false;
    wl_dead = true;
    return false;
  }
  wl_thread_started = true;
  wl_ready = true;
  wl_trace("SETUP",
           "READY dpy=%p dpyfd=%d mgr=%p dev=%p seat=%p bind_ver=%d proto=%s",
           (void *)wl_dpy, wl.display_get_fd(wl_dpy), (void *)wl_manager,
           (void *)wl_device, (void *)wl_seat, mgr_bind_ver, mgr_proto);
  wl_hypr_refresh();
  wl_trace("SETUP",
           "read path state: hyprland_pid=%ld hyprland_fd=%ld read_disabled=%d "
           "receive_cap=%d",
           wl_hypr_pid, wl_hypr_fd, wl_read_disabled ? 1 : 0,
           WL_MAX_RECEIVE_CALLS);
  if (wl_hypr_pid > 0) {
    wl_read_disabled = true;
    snprintf(wl_read_block_reason, sizeof(wl_read_block_reason),
             "hyprland pid %ld present (data-control receive crashes it)",
             wl_hypr_pid);
  }
  return true;
}

static struct wl_proxy *wl_make_source(void) {

  struct wl_proxy *src =
      wl.proxy_marshal_constructor(wl_manager, 0, use_source, NULL);
  wl_req_count++;
  pthread_mutex_lock(&wl_text_lock);
  bool has_text = wl_text != NULL;
  bool has_png = wl_png != NULL && wl_png_len > 0;
  bool has_uri = wl_uri_list != NULL;
  pthread_mutex_unlock(&wl_text_lock);
  wl_trace("REQ manager.create_data_source", "src=%p has_text=%d has_png=%d",
           (void *)src, has_text ? 1 : 0, has_png ? 1 : 0);
  if (has_text) {
    const char *mimes[] = {"text/plain;charset=utf-8", "text/plain", "TEXT",
                           "STRING", "UTF8_STRING"};
    for (int i = 0; i < 5; i++) {
      wl.proxy_marshal_flags(src, 0, NULL, 0, 0, mimes[i]);
      wl_req_count++;
    }
  }
  if (has_uri) {
    wl.proxy_marshal_flags(src, 0, NULL, 0, 0, "text/uri-list");
    wl_req_count++;
  }
  if (has_png) {
    wl.proxy_marshal_flags(src, 0, NULL, 0, 0, wl_png_mime);
    wl_req_count++;
  }
  wl.proxy_add_listener(src, (void (**)(void))src_listener, NULL);
  return src;
}

static void wl_drop_source(struct wl_proxy **slot) {

  if (slot && *slot) {
    wl_req_count++;
    wl_trace("REQ source.destroy", "proxy=%p", (void *)*slot);
    wl.proxy_marshal_flags(*slot, 1, NULL, 0, 0);
    wl.proxy_destroy(*slot);
    *slot = NULL;
  }
}

bool x11_clipboard_available(void) { return x11_env_is_wayland(); }

static void wl_publish_locked(void) {

  wl_req_count += 2;
  wl_trace("REQ publish.begin", "old_source=%p old_psource=%p",
           (void *)wl_source, (void *)wl_psource);
  wl_drop_source(&wl_source);
  wl_drop_source(&wl_psource);
  pthread_mutex_lock(&wl_text_lock);
  bool has_any = wl_text != NULL || (wl_png != NULL && wl_png_len > 0);
  pthread_mutex_unlock(&wl_text_lock);
  if (!has_any) {
    wl_trace("REQ publish.abort", "no text and no png");
    return;
  }
  wl_source = wl_make_source();
  wl_trace("REQ device.set_selection", "src=%p", (void *)wl_source);
  wl.proxy_marshal_flags(wl_device, 0, NULL, 0, 0, wl_source);
  if (mgr_bind_ver >= 2) {
    wl_psource = wl_make_source();
    wl_trace("REQ device.set_primary_selection", "src=%p bind_ver=%d",
             (void *)wl_psource, mgr_bind_ver);
    wl.proxy_marshal_flags(wl_device, 2, NULL, 0, 0, wl_psource);
  } else {
    wl_trace("REQ device.set_primary_selection SKIPPED", "bind_ver=%d < 2",
             mgr_bind_ver);
  }
  int fr = wl.display_flush(wl_dpy);
  wl_trace("REQ publish.end", "flush=%d dperr=%d", fr,
           wl.display_get_error(wl_dpy));
}

int x11_clipboard_set_text(const char *text) {
  if (!text)
    return 0;
  wl_trace("API SetText", "len=%d text='%s'", (int)strlen(text), text);
  pthread_mutex_lock(&wl_text_lock);
  bool same = wl_text != NULL && strcmp(wl_text, text) == 0 && wl_png == NULL;
  pthread_mutex_unlock(&wl_text_lock);
  if (same && wl_source != NULL) {
    wl_trace("API SetText",
             "SKIPPED identical text already published (source=%p)",
             (void *)wl_source);
    return 1;
  }
  if (!wl_setup()) {
    wl_trace("API SetText", "setup failed, returning 0");
    return 0;
  }
  pthread_mutex_lock(&wl_text_lock);
  char *nt = strdup(text);
  if (nt) {
    free(wl_text);
    wl_text = nt;
    free(wl_png);
    wl_png = NULL;
    wl_png_len = 0;
    free(wl_uri_list);
    wl_uri_list = NULL;
  }
  pthread_mutex_unlock(&wl_text_lock);
  if (!nt)
    return 0;
  long long t0 = 0;
  struct timespec ts0, ts1;
  clock_gettime(CLOCK_MONOTONIC, &ts0);
  pthread_mutex_lock(&wl_lock);
  wl_publish_locked();
  pthread_mutex_unlock(&wl_lock);
  clock_gettime(CLOCK_MONOTONIC, &ts1);
  t0 = (ts1.tv_sec - ts0.tv_sec) * 1000 + (ts1.tv_nsec - ts0.tv_nsec) / 1000000;
  wl_trace("API SetText", "DONE lock+publish took %lldms", t0);
  return 1;
}

int x11_clipboard_set_image(const unsigned char *png, size_t len) {
  if (!png || len == 0)
    return 0;
  if (!wl_setup())
    return 0;
  unsigned char *np = malloc(len);
  if (np)
    memcpy(np, png, len);
  if (!np)
    return 0;
  const char *mime = clipboard_image_mime(clipboard_image_format(png, len));
  pthread_mutex_lock(&wl_text_lock);
  free(wl_png);
  wl_png = np;
  wl_png_len = len;
  snprintf(wl_png_mime, sizeof(wl_png_mime), "%s", mime ? mime : "image/png");
  free(wl_text);
  wl_text = NULL;
  free(wl_uri_list);
  wl_uri_list = NULL;
  pthread_mutex_unlock(&wl_text_lock);
  pthread_mutex_lock(&wl_lock);
  wl_publish_locked();
  pthread_mutex_unlock(&wl_lock);
  return 1;
}

int x11_clipboard_set_file(const char *path) {
  if (!path || path[0] == 0)
    return 0;
  char *uri = clipboard_path_to_uri_list(path);
  if (!uri)
    return 0;
  if (!wl_setup()) {
    free(uri);
    return 0;
  }
  pthread_mutex_lock(&wl_text_lock);
  free(wl_text);
  wl_text = NULL;
  free(wl_png);
  wl_png = NULL;
  wl_png_len = 0;
  free(wl_uri_list);
  wl_uri_list = uri;
  pthread_mutex_unlock(&wl_text_lock);
  pthread_mutex_lock(&wl_lock);
  wl_publish_locked();
  pthread_mutex_unlock(&wl_lock);
  return 1;
}

static const char *wl_pick_mime_locked(void) {

  static const char *prefs[] = {"text/plain;charset=utf-8", "text/plain",
                                "UTF8_STRING", "STRING", "TEXT"};
  for (size_t p = 0; p < sizeof(prefs) / sizeof(prefs[0]); p++) {
    for (int i = 0; i < wl_offer_mime_count; i++) {
      if (strcmp(wl_offer_mimes[i], prefs[p]) == 0)
        return prefs[p];
    }
  }
  return NULL;
}

char *x11_clipboard_get_text(void) {
  wl_trace("API GetText", "enter offer=%p has_selection=%d mime_count=%d",
           (void *)wl_offer, wl_has_selection ? 1 : 0, wl_offer_mime_count);
  if (!wl_setup()) {
    wl_trace("API GetText", "setup failed");
    return NULL;
  }
  pthread_mutex_lock(&wl_lock);
  struct wl_proxy *offer = (wl_offer && wl_has_selection) ? wl_offer : NULL;
  pthread_mutex_lock(&wl_text_lock);
  const char *pick = offer ? wl_pick_mime_locked() : NULL;
  char mime[64] = {0};
  if (pick)
    SafeCopy(mime, sizeof(mime), pick);
  pthread_mutex_unlock(&wl_text_lock);
  if (!offer || !mime[0]) {
    pthread_mutex_unlock(&wl_lock);
    wl_trace(
        "API GetText",
        "NO OFFER offer=%p has_selection=%d mime_count=%d picked='%s' dperr=%d",
        (void *)wl_offer, wl_has_selection ? 1 : 0, wl_offer_mime_count, mime,
        wl.display_get_error(wl_dpy));
    return NULL;
  }
  int fds[2] = {-1, -1};
  const char *blocked = wl_read_guard(offer);
  if (blocked) {
    wl_receive_blocked++;
    wl_dead_offer = offer;
    pthread_mutex_unlock(&wl_lock);
    wl_trace("API GetText",
             "RECEIVE BLOCKED reason='%s' offer=%p blocked_total=%ld", blocked,
             (void *)offer, wl_receive_blocked);
    return NULL;
  }
  if (pipe(fds) != 0) {
    pthread_mutex_unlock(&wl_lock);
    wl_trace("API GetText", "pipe FAILED errno=%d", errno);
    return NULL;
  }
  wl_receive_calls++;
  wl_req_count++;
  wl_trace("REQ offer.receive", "offer=%p mime='%s' rfd=%d wfd=%d call=%ld",
           (void *)offer, mime, fds[0], fds[1], wl_receive_calls);
  wl.proxy_marshal_flags(offer, 0, NULL, 0, 0, mime, fds[1]);
  int fr = wl.display_flush(wl_dpy);
  wl_trace("REQ offer.receive", "flush=%d dperr=%d", fr,
           wl.display_get_error(wl_dpy));
  pthread_mutex_unlock(&wl_lock);
  close(fds[1]);
  size_t cap = 8192;
  size_t len = 0;
  char *out = malloc(cap);
  if (!out) {
    close(fds[0]);
    return NULL;
  }
  int timeouts = 0, reads = 0, eofs = 0, complete = 0;
  for (;;) {
    struct pollfd pfd = {fds[0], POLLIN, 0};
    int r = poll(&pfd, 1, 500);
    if (r == 0) {
      timeouts++;
      wl_trace("API GetText", "poll TIMEOUT len=%d timeouts=%d", (int)len,
               timeouts);
      break;
    }
    if (r < 0) {
      if (errno == EINTR)
        continue;
      wl_trace("API GetText", "poll ERR %d errno=%d", r, errno);
      break;
    }
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
    if (got == 0) {
      eofs = 1;
      complete = 1;
      wl_trace("API GetText", "read EOF after %d bytes", (int)len);
      break;
    }
    if (got < 0) {
      if (errno == EINTR)
        continue;
      wl_trace("API GetText", "read ERR errno=%d after %d bytes", errno,
               (int)len);
      break;
    }
    len += (size_t)got;
  }
  close(fds[0]);
  wl_trace("API GetText", "result len=%d timeouts=%d reads=%d eof=%d", (int)len,
           timeouts, reads, eofs);
  if (!complete) {
    wl_trace("API GetText", "incomplete transfer, discarding %d bytes",
             (int)len);
    free(out);
    return NULL;
  }
  if (len == 0) {
    free(out);
    wl_dead_offer = offer;
    wl_trace("API GetText", "marking offer=%p dead, no further receive from it",
             (void *)offer);
    return NULL;
  }
  out[len] = '\0';
  return out;
}

void x11_clipboard_shutdown(void) {
  if (!wl_ready && !wl_handle) {
    return;
  }
  wl_running = false;
  if (wl_thread_started) {
    pthread_join(wl_thread, NULL);
    wl_thread_started = false;
  }
  if (wl_dpy) {
    pthread_mutex_lock(&wl_lock);
    wl_drop_source(&wl_source);
    wl_drop_source(&wl_psource);
    if (wl_offer) {
      wl.proxy_marshal_flags(wl_offer, 1, NULL, 0, 0);
      wl.proxy_destroy(wl_offer);
      wl_offer = NULL;
    }
    wl_has_selection = false;
    if (wl_device) {
      wl.proxy_marshal_flags(wl_device, 1, NULL, 0, 0);
      wl.proxy_destroy(wl_device);
      wl_device = NULL;
    }
    if (wl_manager) {
      wl.proxy_marshal_flags(wl_manager, 2, NULL, 0, 0);
      wl.proxy_destroy(wl_manager);
      wl_manager = NULL;
    }
    if (wl_seat) {
      wl.proxy_destroy(wl_seat);
      wl_seat = NULL;
    }
    pthread_mutex_unlock(&wl_lock);
    wl.display_disconnect(wl_dpy);
    wl_dpy = NULL;
  }
  pthread_mutex_lock(&wl_text_lock);
  free(wl_text);
  wl_text = NULL;
  free(wl_png);
  wl_png = NULL;
  wl_png_len = 0;
  free(wl_uri_list);
  wl_uri_list = NULL;
  pthread_mutex_unlock(&wl_text_lock);
  wl_ready = false;
}
#endif
