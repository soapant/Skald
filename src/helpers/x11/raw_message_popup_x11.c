#include "helpers/x11/raw_message_popup_x11.h"
#include "helpers/helpers_text.h"
#include "helpers/helpers_render_thread.h"

static void x11_msg_fps_frame(void *ud);
static void x11_display_message_draw(Display *dpy, Window win,
                                     const char *message, unsigned long color,
                                     int w, int h, bool no_bg,
                                     void (*render)(void *ud),
                                     void *render_data);

pthread_mutex_t g_msg_lock = PTHREAD_MUTEX_INITIALIZER;
MsgSlot g_msg_slots[MSG_MAX_ACTIVE];
int g_msg_slot_count = 0;

static void x11_msg_fps_frame(void *ud) {
  Window win = (Window)(uintptr_t)ud;
  pthread_mutex_lock(&g_msg_lock);
  int idx = -1;
  for (int i = 0; i < g_msg_slot_count; i++) {
    if (g_msg_slots[i].win == win) {
      idx = i;
      break;
    }
  }
  if (idx < 0 || !g_msg_slots[idx].render || g_msg_slots[idx].fps <= 0) {
    pthread_mutex_unlock(&g_msg_lock);
    return;
  }
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  long long now = (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
  long long interval = 1000LL / g_msg_slots[idx].fps;
  if (interval < 16)
    interval = 16;
  if (g_msg_slots[idx].last_frame_ms != 0 &&
      now - g_msg_slots[idx].last_frame_ms < interval) {
    pthread_mutex_unlock(&g_msg_lock);
    return;
  }
  g_msg_slots[idx].last_frame_ms = now;
  char *message =
      g_msg_slots[idx].message ? strdup(g_msg_slots[idx].message) : NULL;
  unsigned long color = g_msg_slots[idx].color;
  bool no_bg = g_msg_slots[idx].no_bg;
  int w = g_msg_slots[idx].w, h = g_msg_slots[idx].h;
  void (*render)(void *ud) = g_msg_slots[idx].render;
  void *render_data = g_msg_slots[idx].render_data;
  pthread_mutex_unlock(&g_msg_lock);
  Display *rdpy = (Display *)render_thread_display();
  if (!rdpy)
    rdpy = x11_get_display();
  if (rdpy)
    x11_display_message_draw(rdpy, win, message ? message : "", color, w, h,
                             no_bg, render, render_data);
  free(message);
}

static void x11_msg_slot_reset_extra(MsgSlot *s) {
  s->fps = 0;
  s->last_frame_ms = 0;
  s->color = 0;
  s->no_bg = false;
  s->name[0] = '\0';
  free(s->message);
  s->message = NULL;
  s->render = NULL;
  s->render_data = NULL;
}

void x11_msg_remove_slot_locked(Display *dpy, int idx) {

  Window win = g_msg_slots[idx].win;
  if (g_msg_slots[idx].fps > 0 && g_msg_slots[idx].render)
    render_thread_remove_frame_callback(x11_msg_fps_frame,
                                        (void *)(uintptr_t)win);
  x11_msg_slot_reset_extra(&g_msg_slots[idx]);
  XDestroyWindow(dpy, win);
  for (int i = idx; i < g_msg_slot_count - 1; i++)
    g_msg_slots[i] = g_msg_slots[i + 1];
  g_msg_slot_count--;
  memset(&g_msg_slots[g_msg_slot_count], 0, sizeof(g_msg_slots[0]));
}

static void x11_msg_remove_window_locked(Display *dpy, Window win) {

  for (int i = 0; i < g_msg_slot_count; i++) {
    if (g_msg_slots[i].win == win) {
      x11_msg_remove_slot_locked(dpy, i);
      return;
    }
  }
}

int x11_msg_find_key_locked(const char *key) {

  if (!key || !key[0])
    return -1;
  for (int i = 0; i < g_msg_slot_count; i++) {
    if (strcmp(g_msg_slots[i].key, key) == 0)
      return i;
  }
  return -1;
}

void x11_msg_remove_indefinite_locked(Display *dpy) {

  for (int i = 0; i < g_msg_slot_count;) {
    if (g_msg_slots[i].indefinite)
      x11_msg_remove_slot_locked(dpy, i);
    else
      i++;
  }
}

typedef struct {
  Display *dpy;
  Window win;
  double timeout;
} MsgDismissPayload;

static void *x11_msg_dismiss_thread(void *arg) {

  MsgDismissPayload *p = (MsgDismissPayload *)arg;
  if (!p)
    return NULL;
  if (p->timeout > 0) {
    struct timespec ts;
    ts.tv_sec = (time_t)p->timeout;
    ts.tv_nsec = (long)((p->timeout - (double)ts.tv_sec) * 1000000000.0);
    if (ts.tv_nsec < 0)
      ts.tv_nsec = 0;
    if (ts.tv_nsec > 999999999L)
      ts.tv_nsec = 999999999L;
    nanosleep(&ts, NULL);
  }
  Display *dpy = p->dpy;
  Window win = p->win;
  free(p);
  if (dpy && win != None) {
    pthread_mutex_lock(&g_msg_lock);
    x11_msg_remove_window_locked(dpy, win);
    pthread_mutex_unlock(&g_msg_lock);
    XFlush(dpy);
  }
  return NULL;
}

static int x11_msg_text_width(Display *dpy, XftFont *xf, const char *s, int n) {

  if (!s || n <= 0)
    return 0;
  XGlyphInfo ext;
  XftTextExtentsUtf8(dpy, xf, (FcChar8 *)s, n, &ext);
  return ext.xOff;
}

static bool x11_msg_push_line(MsgLine *lines, int *count, int max_lines,
                              const char *ls, const char *le, int line_w) {
  if (!ls || *count >= max_lines)
    return false;
  lines[*count].s = ls;
  lines[*count].n = (int)(le - ls);
  lines[*count].w = line_w;
  (*count)++;
  return true;
}

int x11_msg_wrap_text(Display *dpy, XftFont *xf, const char *text, int max_w,
                      MsgLine *lines, int max_lines) {

  int count = 0;
  int space_w = x11_msg_text_width(dpy, xf, " ", 1);
  const char *p = text;
  while (*p && count < max_lines) {
    const char *nl = strchr(p, '\n');
    const char *pend = nl ? nl : p + strlen(p);
    const char *w = p;
    const char *ls = NULL;
    const char *le = NULL;
    int line_w = 0;
    while (w < pend) {
      while (w < pend && *w == ' ')
        w++;
      if (w >= pend)
        break;
      const char *ws = w;
      while (w < pend && *w != ' ')
        w++;
      int wn = (int)(w - ws);
      int ww = x11_msg_text_width(dpy, xf, ws, wn);
      if (ww > max_w) {
        if (x11_msg_push_line(lines, &count, max_lines, ls, le, line_w)) {
          ls = NULL;
          le = NULL;
          line_w = 0;
        } else if (count >= max_lines) {
          return count;
        }
        const char *c = ws;
        const char *we = ws + wn;
        while (c < we && count < max_lines) {
          const char *t = c;
          int best = 0;
          int best_w = 0;
          while (t < we) {
            int cl = utf8_lead_len((unsigned char)*t);
            if (t + cl > we)
              cl = (int)(we - t);
            int nw = x11_msg_text_width(dpy, xf, c, (int)(t + cl - c));
            if (nw > max_w)
              break;
            best = (int)(t + cl - c);
            best_w = nw;
            t += cl;
          }
          if (best == 0) {
            best = utf8_lead_len((unsigned char)*c);
            if (c + best > we)
              best = (int)(we - c);
            best_w = x11_msg_text_width(dpy, xf, c, best);
          }
          lines[count].s = c;
          lines[count].n = best;
          lines[count].w = best_w;
          count++;
          c += best;
        }
        continue;
      }
      int add = ww + (line_w > 0 ? space_w : 0);
      if (line_w > 0 && line_w + add > max_w) {
        if (x11_msg_push_line(lines, &count, max_lines, ls, le, line_w)) {
          ls = NULL;
          le = NULL;
          line_w = 0;
        }
        if (count >= max_lines)
          return count;
      }
      if (!ls)
        ls = ws;
      le = w;
      line_w += add;
    }
    if (ls)
      x11_msg_push_line(lines, &count, max_lines, ls, le, line_w);
    else if (pend == p && nl && count < max_lines) {
      lines[count].s = p;
      lines[count].n = 0;
      lines[count].w = 0;
      count++;
    }
    p = nl ? nl + 1 : pend;
  }
  return count;
}

static void x11_display_message_draw(Display *dpy, Window win,
                                     const char *message, unsigned long color,
                                     int w, int h, bool no_bg,
                                     void (*render)(void *ud),
                                     void *render_data) {

  XWindowAttributes attrs;
  if (!XGetWindowAttributes(dpy, win, &attrs)) {
    return;
  }

  if (!no_bg) {
    x11_draw_background(dpy, win, w, h);
  } else {
    XClearArea(dpy, win, 0, 0, w, h, False);
  }

  XftDraw *xd = XftDrawCreate(dpy, win, attrs.visual, attrs.colormap);
  if (!xd) {
    return;
  }

  if (render != NULL) {
    x11_set_draw_context(dpy, xd, attrs.visual, attrs.colormap);
    render(render_data);
  }
  FontHandle font = x11_create_font_on_display(dpy, FontSize, UsedFont);
  XftFont *xf = FH_TO_XFT(font);
  if (xf && message && message[0] != '\0') {
    XftColor col;
    x11_xft_color_cached(dpy, attrs.visual, attrs.colormap, rgb_to_hex(color),
                         0xFFFF, &col);
    MsgLine lines[MSG_MAX_LINES];
    int nlines =
        x11_msg_wrap_text(dpy, xf, message, w - 30, lines, MSG_MAX_LINES);
    int line_h = xf->ascent + xf->descent;
    int y = (h - nlines * line_h) / 2 + xf->ascent;
    if (y < xf->ascent)
      y = xf->ascent;
    for (int i = 0; i < nlines; i++) {
      if (y > h - 5)
        break;
      if (lines[i].n > 0) {
        int x = (w - lines[i].w) / 2;
        if (x < 0)
          x = 0;
        XftDrawStringUtf8(xd, &col, xf, x, y, (FcChar8 *)lines[i].s,
                          lines[i].n);
      }
      y += line_h;
    }
  }
  Platform_DestroyFont(font);

  x11_set_draw_context(NULL, NULL, NULL, 0);
  XftDrawDestroy(xd);
  x11_draw_window_border(dpy, win, NULL, w, h, no_bg);
  XFlush(dpy);
}

int x11_msg_stack_adjust_y(int x, int y, int w, int h, int dir) {

  pthread_mutex_lock(&g_msg_lock);
  for (;;) {
    bool hit = false;
    for (int i = 0; i < g_msg_slot_count; i++) {
      const MsgSlot *s = &g_msg_slots[i];
      if (x < s->x + s->w && x + w > s->x && y < s->y + s->h && y + h > s->y) {
        if (dir > 0)
          y = s->y + s->h + MSG_COLLISION_GAP;
        else
          y = s->y - h - MSG_COLLISION_GAP;
        hit = true;
        break;
      }
    }
    if (!hit)
      break;
  }
  pthread_mutex_unlock(&g_msg_lock);
  return y;
}

void x11_msg_present(Display *dpy, const char *message, int x, int y, int win_w,
                     int win_h, double timeout, ColorRef color, bool no_bg,
                     void (*render)(void *ud), void *render_data,
                     const char *name, bool rounded, int radius, int fps) {

  int ax, ay, aw, ah;
  WindowManager_GetMainMonitorGeometry(&ax, &ay, &aw, &ah);
  set_transparent_window(no_bg);
  Window win = x11_create_display_message_window_ex(
      dpy, x + ax, y + ay, win_w, win_h, false, rounded, radius);
  set_transparent_window(false);
  if (!win) {
    return;
  }
  XMapWindow(dpy, win);
  XFlush(dpy);
  x11_display_message_draw(dpy, win, message, (unsigned long)color, win_w,
                           win_h, no_bg, render, render_data);
  bool indefinite = MsgTimeoutIsIndefinite(timeout);
  bool pump = false;
  pthread_mutex_lock(&g_msg_lock);
  if (g_msg_slot_count >= MSG_MAX_ACTIVE)
    x11_msg_remove_slot_locked(dpy, 0);
  MsgSlot *slot = &g_msg_slots[g_msg_slot_count];
  memset(slot, 0, sizeof(*slot));
  slot->win = win;
  slot->indefinite = indefinite;
  slot->key[0] = '\0';
  if (name && name[0])
    snprintf(slot->name, sizeof(slot->name), "%s", name);
  else
    slot->name[0] = '\0';
  slot->w = win_w;
  slot->h = win_h;
  slot->x = x + ax;
  slot->y = y + ay;
  slot->fps = fps;
  slot->color = (unsigned long)color;
  slot->no_bg = no_bg;
  slot->message = strdup(message ? message : "");
  slot->render = render;
  slot->render_data = render_data;
  pump = fps > 0 && render != NULL;
  g_msg_slot_count++;
  pthread_mutex_unlock(&g_msg_lock);
  if (pump) {
    render_thread_ensure_started(fps);
    render_thread_add_frame_callback(x11_msg_fps_frame, (void *)(uintptr_t)win);
  }
  if (!indefinite) {
    MsgDismissPayload *p = malloc(sizeof(*p));
    if (p) {
      p->dpy = dpy;
      p->win = win;
      p->timeout = timeout;
      pthread_t thr;
      if (pthread_create(&thr, NULL, x11_msg_dismiss_thread, p) == 0) {
        pthread_detach(thr);
      } else {
        free(p);
      }
    }
  }
}

void x11_msg_hover_present(Display *dpy, const char *key, const char *message,
                           int center_x, int center_y, int win_w, int win_h,
                           ColorRef color, bool no_bg) {

  if (!dpy || !key || !key[0] || !message || !message[0]) {
    return;
  }
  if (win_w <= 0 || win_h <= 0) {
    return;
  }
  pthread_mutex_lock(&g_msg_lock);
  int idx = x11_msg_find_key_locked(key);
  if (idx >= 0) {
    Window w = g_msg_slots[idx].win;
    if (g_msg_slots[idx].w != win_w || g_msg_slots[idx].h != win_h) {
      XMoveResizeWindow(dpy, w, center_x - win_w / 2, center_y - win_h / 2,
                        win_w, win_h);
      g_msg_slots[idx].w = win_w;
      g_msg_slots[idx].h = win_h;
    }
    g_msg_slots[idx].x = center_x - win_w / 2;
    g_msg_slots[idx].y = center_y - win_h / 2;
    x11_msg_slot_reset_extra(&g_msg_slots[idx]);
    pthread_mutex_unlock(&g_msg_lock);
    x11_display_message_draw(dpy, w, message, (unsigned long)color, win_w,
                             win_h, no_bg, NULL, NULL);
    return;
  }
  pthread_mutex_unlock(&g_msg_lock);

  Window win = x11_create_display_message_window(
      dpy, center_x - win_w / 2, center_y - win_h / 2, win_w, win_h, false);
  if (win == None) {
    return;
  }
  XMapRaised(dpy, win);
  XFlush(dpy);
  x11_display_message_draw(dpy, win, message, (unsigned long)color, win_w,
                           win_h, no_bg, NULL, NULL);

  pthread_mutex_lock(&g_msg_lock);
  if (g_msg_slot_count >= MSG_MAX_ACTIVE)
    x11_msg_remove_slot_locked(dpy, 0);
  int at = x11_msg_find_key_locked(key);
  if (at < 0)
    at = g_msg_slot_count < MSG_MAX_ACTIVE ? g_msg_slot_count
                                           : MSG_MAX_ACTIVE - 1;
  g_msg_slots[at].win = win;
  g_msg_slots[at].indefinite = true;
  g_msg_slots[at].key[0] = '\0';
  snprintf(g_msg_slots[at].key, sizeof(g_msg_slots[at].key), "%s", key);
  g_msg_slots[at].w = win_w;
  g_msg_slots[at].h = win_h;
  g_msg_slots[at].x = center_x - win_w / 2;
  g_msg_slots[at].y = center_y - win_h / 2;
  x11_msg_slot_reset_extra(&g_msg_slots[at]);
  if (at >= g_msg_slot_count)
    g_msg_slot_count = at + 1;
  pthread_mutex_unlock(&g_msg_lock);
  XFlush(dpy);
}

void x11_msg_measure(Display *dpy, const char *message, int max_width,
                     int max_height, int *out_w, int *out_h) {

  *out_w = 0;
  *out_h = 0;
  FontHandle font = x11_create_font_on_display(dpy, FontSize, UsedFont);
  XftFont *xf = FH_TO_XFT(font);
  int text_w = 0, text_h = 0;
  MsgLine lines[MSG_MAX_LINES];
  int nlines = 0;
  if (xf) {
    int wrap_w = max_width - 30;
    if (wrap_w > MSG_AUTO_WRAP_MAX_W)
      wrap_w = MSG_AUTO_WRAP_MAX_W;
    nlines = x11_msg_wrap_text(dpy, xf, message, wrap_w, lines, MSG_MAX_LINES);
    for (int i = 0; i < nlines; i++)
      if (lines[i].w > text_w)
        text_w = lines[i].w;
    text_h = nlines * (xf->ascent + xf->descent);
  }
  Platform_DestroyFont(font);
  int win_w = text_w + 30;
  int win_h = text_h + 30;
  if (win_w > max_width)
    win_w = max_width;
  if (win_h > max_height)
    win_h = max_height;
  *out_w = win_w;
  *out_h = win_h;
}
