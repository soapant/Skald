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

typedef struct {
  Window drawing_win;
  Window pending_win;
  bool pending;
  char *pending_text;
  unsigned long pending_color;
  int pending_w;
  int pending_h;
  bool pending_no_bg;
} MsgDrawState;

static __thread MsgDrawState g_msg_draw;

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

int x11_msg_wrap_text(X11MsgWrapText *argspec) {

  int count = 0;
  int space_w = x11_msg_text_width(argspec->dpy, argspec->xf, " ", 1);
  const char *p = argspec->text;
  while (*p && count < argspec->maxLines) {
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
      int ww = x11_msg_text_width(argspec->dpy, argspec->xf, ws, wn);
      if (ww > argspec->max_w) {
        if (x11_msg_push_line(argspec->out, &count, argspec->maxLines, ls, le,
                              line_w)) {
          ls = NULL;
          le = NULL;
          line_w = 0;
        } else if (count >= argspec->maxLines) {
          return count;
        }
        const char *c = ws;
        const char *we = ws + wn;
        while (c < we && count < argspec->maxLines) {
          const char *t = c;
          int best = 0;
          int best_w = 0;
          while (t < we) {
            int cl = utf8_lead_len((unsigned char)*t);
            if (t + cl > we)
              cl = (int)(we - t);
            int nw = x11_msg_text_width(argspec->dpy, argspec->xf, c,
                                        (int)(t + cl - c));
            if (nw > argspec->max_w)
              break;
            best = (int)(t + cl - c);
            best_w = nw;
            t += cl;
          }
          if (best == 0) {
            best = utf8_lead_len((unsigned char)*c);
            if (c + best > we)
              best = (int)(we - c);
            best_w = x11_msg_text_width(argspec->dpy, argspec->xf, c, best);
          }
          argspec->out[count].s = c;
          argspec->out[count].n = best;
          argspec->out[count].w = best_w;
          count++;
          c += best;
        }
        continue;
      }
      int add = ww + (line_w > 0 ? space_w : 0);
      if (line_w > 0 && line_w + add > argspec->max_w) {
        if (x11_msg_push_line(argspec->out, &count, argspec->maxLines, ls, le,
                              line_w)) {
          ls = NULL;
          le = NULL;
          line_w = 0;
        }
        if (count >= argspec->maxLines)
          return count;
      }
      if (!ls)
        ls = ws;
      le = w;
      line_w += add;
    }
    if (ls)
      x11_msg_push_line(argspec->out, &count, argspec->maxLines, ls, le,
                        line_w);
    else if (pend == p && nl && count < argspec->maxLines) {
      argspec->out[count].s = p;
      argspec->out[count].n = 0;
      argspec->out[count].w = 0;
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
  char *owned_pending = NULL;

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
    g_msg_draw.drawing_win = win;
    render(render_data);
    g_msg_draw.drawing_win = None;
    x11_set_draw_context(NULL, NULL, NULL, 0);
    if (g_msg_draw.pending && g_msg_draw.pending_win == win) {
      owned_pending = g_msg_draw.pending_text;
      g_msg_draw.pending_text = NULL;
      g_msg_draw.pending = false;
      g_msg_draw.pending_win = None;
      if (owned_pending) {
        message = owned_pending;
        color = g_msg_draw.pending_color;
        w = g_msg_draw.pending_w;
        h = g_msg_draw.pending_h;
        no_bg = g_msg_draw.pending_no_bg;
      }
    }
  }
  FontHandle font = x11_create_font_on_display(dpy, FontSize, UsedFont);
  XftFont *xf = FH_TO_XFT(font);
  if (xf && message && message[0] != '\0') {
    XftColor col;
    x11_xft_color_cached(&(X11XftColorCached){.dpy = dpy,
                                              .vis = attrs.visual,
                                              .cmap = attrs.colormap,
                                              .hex = rgb_to_hex(color),
                                              .alpha = 0xFFFF,
                                              .out = &col});
    MsgLine lines[MSG_MAX_LINES];
    int nlines =
        x11_msg_wrap_text(&(X11MsgWrapText){.dpy = dpy,
                                            .xf = xf,
                                            .text = message,
                                            .max_w = w - 30,
                                            .out = lines,
                                            .maxLines = MSG_MAX_LINES});
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
  x11_draw_window_border(&(X11DrawWindowBorder){
      .dpy = dpy, .drw = win, .vis = NULL, .w = w, .h = h, .no_bg = no_bg});
  free(owned_pending);
  XFlush(dpy);
}

int x11_msg_stack_adjust_y(X11MsgStackAdjustY *argspec) {

  pthread_mutex_lock(&g_msg_lock);
  for (;;) {
    bool hit = false;
    for (int i = 0; i < g_msg_slot_count; i++) {
      const MsgSlot *s = &g_msg_slots[i];
      if (argspec->x < s->x + s->w && argspec->x + argspec->w > s->x &&
          argspec->y < s->y + s->h && argspec->y + argspec->h > s->y) {
        if (argspec->dir > 0)
          argspec->y = s->y + s->h + MSG_COLLISION_GAP;
        else
          argspec->y = s->y - argspec->h - MSG_COLLISION_GAP;
        hit = true;
        break;
      }
    }
    if (!hit)
      break;
  }
  pthread_mutex_unlock(&g_msg_lock);
  return argspec->y;
}

void x11_msg_present(X11MsgPresent *argspec) {

  int ax, ay, aw, ah;
  WindowManager_GetMainMonitorGeometry(&ax, &ay, &aw, &ah);
  set_transparent_window(argspec->no_bg);
  Window win = x11_create_display_message_window_ex(
      &(X11CreateDisplayMessageWindowEx){.dpy = argspec->dpy,
                                         .x = argspec->x + ax,
                                         .y = argspec->y + ay,
                                         .w = argspec->win_w,
                                         .h = argspec->win_h,
                                         .clickable = false,
                                         .rounded = argspec->rounded,
                                         .radius = argspec->radius});
  set_transparent_window(false);
  if (!win) {
    return;
  }
  XMapWindow(argspec->dpy, win);
  XFlush(argspec->dpy);
  x11_display_message_draw(argspec->dpy, win, argspec->message,
                           (unsigned long)argspec->color, argspec->win_w,
                           argspec->win_h, argspec->no_bg, argspec->render,
                           argspec->render_data);
  bool indefinite = MsgTimeoutIsIndefinite(argspec->timeout);
  bool pump = false;
  pthread_mutex_lock(&g_msg_lock);
  if (g_msg_slot_count >= MSG_MAX_ACTIVE)
    x11_msg_remove_slot_locked(argspec->dpy, 0);
  MsgSlot *slot = &g_msg_slots[g_msg_slot_count];
  memset(slot, 0, sizeof(*slot));
  slot->win = win;
  slot->indefinite = indefinite;
  slot->key[0] = '\0';
  if (argspec->name && argspec->name[0])
    snprintf(slot->name, sizeof(slot->name), "%s", argspec->name);
  else
    slot->name[0] = '\0';
  slot->w = argspec->win_w;
  slot->h = argspec->win_h;
  slot->x = argspec->x + ax;
  slot->y = argspec->y + ay;
  slot->fps = argspec->fps;
  slot->color = (unsigned long)argspec->color;
  slot->no_bg = argspec->no_bg;
  slot->rounded = argspec->rounded ? argspec->rounded : RoundedCorners;
  slot->radius = argspec->radius > 0 ? argspec->radius : CornerRadius;
  slot->message = strdup(argspec->message ? argspec->message : "");
  slot->render = argspec->render;
  slot->render_data = argspec->render_data;
  pump = argspec->fps > 0 && argspec->render != NULL;
  g_msg_slot_count++;
  pthread_mutex_unlock(&g_msg_lock);
  if (pump) {
    render_thread_ensure_started(argspec->fps);
    render_thread_add_frame_callback(x11_msg_fps_frame, (void *)(uintptr_t)win);
  }
  if (!indefinite) {
    MsgDismissPayload *p = malloc(sizeof(*p));
    if (p) {
      p->dpy = argspec->dpy;
      p->win = win;
      p->timeout = argspec->timeout;
      pthread_t thr;
      if (pthread_create(&thr, NULL, x11_msg_dismiss_thread, p) == 0) {
        pthread_detach(thr);
      } else {
        free(p);
      }
    }
  }
}

bool x11_msg_set_text(Display *dpy, const char *name, const char *text) {
  if (!dpy || !name || !name[0])
    return false;
  if (g_msg_draw.pending) {
    free(g_msg_draw.pending_text);
    g_msg_draw.pending_text = NULL;
    g_msg_draw.pending = false;
    g_msg_draw.pending_win = None;
  }
  char *copy = strdup(text ? text : "");
  if (!copy)
    return false;
  pthread_mutex_lock(&g_msg_lock);
  int idx = -1;
  for (int i = 0; i < g_msg_slot_count; i++)
    if (strcmp(g_msg_slots[i].name, name) == 0) {
      idx = i;
      break;
    }
  if (idx < 0) {
    pthread_mutex_unlock(&g_msg_lock);
    free(copy);
    return false;
  }
  MsgSlot *slot = &g_msg_slots[idx];
  Window win = slot->win;
  Display *use_dpy = dpy;
  bool in_draw = g_msg_draw.drawing_win == win;
  if (in_draw) {
    Display *rdpy = (Display *)render_thread_display();
    if (rdpy)
      use_dpy = rdpy;
  }
  int w = 0, h = 0;
  x11_msg_measure(&(X11MsgMeasure){.dpy = use_dpy,
                                   .message = copy,
                                   .max_width = MSG_AUTO_WRAP_MAX_W,
                                   .max_height = MSG_AUTO_WRAP_MAX_H,
                                   .out_w = &w,
                                   .out_h = &h});
  if (w <= 0)
    w = 32;
  if (h <= 0)
    h = 24;
  int nx = slot->x + slot->w / 2 - w / 2;
  int ny = slot->y + slot->h / 2 - h / 2;
  unsigned long color = slot->color;
  bool no_bg = slot->no_bg;
  bool rounded = slot->rounded ? slot->rounded : RoundedCorners;
  int radius = slot->radius > 0 ? slot->radius : CornerRadius;
  void (*render)(void *ud) = slot->render;
  void *render_data = slot->render_data;
  free(slot->message);
  slot->message = copy;
  slot->w = w;
  slot->h = h;
  slot->x = nx;
  slot->y = ny;
  pthread_mutex_unlock(&g_msg_lock);
  XMoveResizeWindow(use_dpy, win, nx, ny, (unsigned int)w, (unsigned int)h);
  x11_apply_shape(&(X11ApplyShape){.dpy = use_dpy,
                                   .win = win,
                                   .w = w,
                                   .h = h,
                                   .rounded = rounded,
                                   .radius = radius});
  if (in_draw) {
    g_msg_draw.pending_text = strdup(copy);
    if (g_msg_draw.pending_text) {
      g_msg_draw.pending_win = win;
      g_msg_draw.pending_color = color;
      g_msg_draw.pending_w = w;
      g_msg_draw.pending_h = h;
      g_msg_draw.pending_no_bg = no_bg;
      g_msg_draw.pending = true;
    }
  } else {
    x11_display_message_draw(use_dpy, win, copy, color, w, h, no_bg, render,
                             render_data);
  }
  XFlush(use_dpy);
  return true;
}

void x11_msg_hover_present(X11MsgHoverPresent *argspec) {

  if (!argspec->dpy || !argspec->key || !argspec->key[0] || !argspec->message ||
      !argspec->message[0]) {
    return;
  }
  if (argspec->win_w <= 0 || argspec->win_h <= 0) {
    return;
  }
  pthread_mutex_lock(&g_msg_lock);
  int idx = x11_msg_find_key_locked(argspec->key);
  if (idx >= 0) {
    Window w = g_msg_slots[idx].win;
    if (g_msg_slots[idx].w != argspec->win_w ||
        g_msg_slots[idx].h != argspec->win_h) {
      XMoveResizeWindow(argspec->dpy, w, argspec->center_x - argspec->win_w / 2,
                        argspec->center_y - argspec->win_h / 2, argspec->win_w,
                        argspec->win_h);
      g_msg_slots[idx].w = argspec->win_w;
      g_msg_slots[idx].h = argspec->win_h;
    }
    g_msg_slots[idx].x = argspec->center_x - argspec->win_w / 2;
    g_msg_slots[idx].y = argspec->center_y - argspec->win_h / 2;
    x11_msg_slot_reset_extra(&g_msg_slots[idx]);
    pthread_mutex_unlock(&g_msg_lock);
    x11_display_message_draw(argspec->dpy, w, argspec->message,
                             (unsigned long)argspec->color, argspec->win_w,
                             argspec->win_h, argspec->no_bg, NULL, NULL);
    return;
  }
  pthread_mutex_unlock(&g_msg_lock);

  Window win =
      x11_create_display_message_window(&(X11CreateDisplayMessageWindow){
          .dpy = argspec->dpy,
          .x = argspec->center_x - argspec->win_w / 2,
          .y = argspec->center_y - argspec->win_h / 2,
          .w = argspec->win_w,
          .h = argspec->win_h,
          .clickable = false});
  if (win == None) {
    return;
  }
  XMapRaised(argspec->dpy, win);
  XFlush(argspec->dpy);
  x11_display_message_draw(argspec->dpy, win, argspec->message,
                           (unsigned long)argspec->color, argspec->win_w,
                           argspec->win_h, argspec->no_bg, NULL, NULL);

  pthread_mutex_lock(&g_msg_lock);
  if (g_msg_slot_count >= MSG_MAX_ACTIVE)
    x11_msg_remove_slot_locked(argspec->dpy, 0);
  int at = x11_msg_find_key_locked(argspec->key);
  if (at < 0)
    at = g_msg_slot_count < MSG_MAX_ACTIVE ? g_msg_slot_count
                                           : MSG_MAX_ACTIVE - 1;
  g_msg_slots[at].win = win;
  g_msg_slots[at].indefinite = true;
  g_msg_slots[at].key[0] = '\0';
  snprintf(g_msg_slots[at].key, sizeof(g_msg_slots[at].key), "%s",
           argspec->key);
  g_msg_slots[at].w = argspec->win_w;
  g_msg_slots[at].h = argspec->win_h;
  g_msg_slots[at].x = argspec->center_x - argspec->win_w / 2;
  g_msg_slots[at].y = argspec->center_y - argspec->win_h / 2;
  x11_msg_slot_reset_extra(&g_msg_slots[at]);
  if (at >= g_msg_slot_count)
    g_msg_slot_count = at + 1;
  pthread_mutex_unlock(&g_msg_lock);
  XFlush(argspec->dpy);
}

void x11_msg_measure(X11MsgMeasure *argspec) {

  *argspec->out_w = 0;
  *argspec->out_h = 0;
  FontHandle font =
      x11_create_font_on_display(argspec->dpy, FontSize, UsedFont);
  XftFont *xf = FH_TO_XFT(font);
  int text_w = 0, text_h = 0;
  MsgLine lines[MSG_MAX_LINES];
  int nlines = 0;
  if (xf) {
    int wrap_w = argspec->max_width - 30;
    if (wrap_w > MSG_AUTO_WRAP_MAX_W)
      wrap_w = MSG_AUTO_WRAP_MAX_W;
    nlines = x11_msg_wrap_text(&(X11MsgWrapText){.dpy = argspec->dpy,
                                                 .xf = xf,
                                                 .text = argspec->message,
                                                 .max_w = wrap_w,
                                                 .out = lines,
                                                 .maxLines = MSG_MAX_LINES});
    for (int i = 0; i < nlines; i++)
      if (lines[i].w > text_w)
        text_w = lines[i].w;
    text_h = nlines * (xf->ascent + xf->descent);
  }
  Platform_DestroyFont(font);
  int win_w = text_w + 30;
  int win_h = text_h + 30;
  if (win_w > argspec->max_width)
    win_w = argspec->max_width;
  if (win_h > argspec->max_height)
    win_h = argspec->max_height;
  *argspec->out_w = win_w;
  *argspec->out_h = win_h;
}
