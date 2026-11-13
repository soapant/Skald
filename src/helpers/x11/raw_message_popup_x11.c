#include "helpers/x11/raw_message_popup_x11.h"
#include "helpers/helpers_text.h"

pthread_mutex_t g_msg_lock = PTHREAD_MUTEX_INITIALIZER;
MsgSlot g_msg_slots[MSG_MAX_ACTIVE];
int g_msg_slot_count = 0;

void x11_msg_remove_slot_locked(Display *dpy, int idx) {

  XDestroyWindow(dpy, g_msg_slots[idx].win);
  for (int i = idx; i < g_msg_slot_count - 1; i++)
    g_msg_slots[i] = g_msg_slots[i + 1];
  g_msg_slot_count--;
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
                              const char *ls, const char *le, int lineW) {
  if (!ls || *count >= max_lines)
    return false;
  lines[*count].s = ls;
  lines[*count].n = (int)(le - ls);
  lines[*count].w = lineW;
  (*count)++;
  return true;
}

int x11_msg_wrap_text(Display *dpy, XftFont *xf, const char *text, int maxW,
                      MsgLine *lines, int max_lines) {

  int count = 0;
  int spaceW = x11_msg_text_width(dpy, xf, " ", 1);
  const char *p = text;
  while (*p && count < max_lines) {
    const char *nl = strchr(p, '\n');
    const char *pend = nl ? nl : p + strlen(p);
    const char *w = p;
    const char *ls = NULL;
    const char *le = NULL;
    int lineW = 0;
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
      if (ww > maxW) {
        if (x11_msg_push_line(lines, &count, max_lines, ls, le, lineW)) {
          ls = NULL;
          le = NULL;
          lineW = 0;
        } else if (count >= max_lines) {
          return count;
        }
        const char *c = ws;
        const char *we = ws + wn;
        while (c < we && count < max_lines) {
          const char *t = c;
          int best = 0;
          int bestW = 0;
          while (t < we) {
            int cl = Utf8_LeadLen((unsigned char)*t);
            if (t + cl > we)
              cl = (int)(we - t);
            int nw = x11_msg_text_width(dpy, xf, c, (int)(t + cl - c));
            if (nw > maxW)
              break;
            best = (int)(t + cl - c);
            bestW = nw;
            t += cl;
          }
          if (best == 0) {
            best = Utf8_LeadLen((unsigned char)*c);
            if (c + best > we)
              best = (int)(we - c);
            bestW = x11_msg_text_width(dpy, xf, c, best);
          }
          lines[count].s = c;
          lines[count].n = best;
          lines[count].w = bestW;
          count++;
          c += best;
        }
        continue;
      }
      int add = ww + (lineW > 0 ? spaceW : 0);
      if (lineW > 0 && lineW + add > maxW) {
        if (x11_msg_push_line(lines, &count, max_lines, ls, le, lineW)) {
          ls = NULL;
          le = NULL;
          lineW = 0;
        }
        if (count >= max_lines)
          return count;
      }
      if (!ls)
        ls = ws;
      le = w;
      lineW += add;
    }
    if (ls)
      x11_msg_push_line(lines, &count, max_lines, ls, le, lineW);
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

void x11_display_message_draw(Display *dpy, Window win, const char *message,
                              unsigned long color, int w, int h, bool no_bg,
                              void (*render)(void)) {

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
    render();
  }
  FontHandle font = x11_create_font_on_display(dpy, FontSize, UsedFont);
  XftFont *xf = FH_TO_XFT(font);
  if (xf && message && message[0] != '\0') {
    XftColor col;
    x11_xft_alloc_color(dpy, attrs.visual, attrs.colormap, rgb_to_hex(color),
                        0xFFFF, &col);
    MsgLine lines[MSG_MAX_LINES];
    int nlines =
        x11_msg_wrap_text(dpy, xf, message, w - 30, lines, MSG_MAX_LINES);
    int lineH = xf->ascent + xf->descent;
    int y = (h - nlines * lineH) / 2 + xf->ascent;
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
      y += lineH;
    }
    XftColorFree(dpy, attrs.visual, attrs.colormap, &col);
  }
  Platform_DestroyFont(font);

  x11_set_draw_context(NULL, NULL, NULL, 0);
  XftDrawDestroy(xd);
  x11_draw_window_border(dpy, win, w, h, no_bg);
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

void x11_msg_present(Display *dpy, const char *message, int x, int y, int winW,
                     int winH, double timeout, ColorRef color, bool no_bg,
                     void (*render)(void), const char *name) {

  int ax, ay, aw, ah;
  WindowManager_GetMainMonitorGeometry(&ax, &ay, &aw, &ah);
  SetTransparentWindow(no_bg);
  Window win =
      x11_create_display_message_window(dpy, x + ax, y + ay, winW, winH, false);
  SetTransparentWindow(false);
  if (!win) {
    return;
  }
  XMapWindow(dpy, win);
  XFlush(dpy);
  x11_display_message_draw(dpy, win, message, (unsigned long)color, winW, winH,
                           no_bg, render);
  bool indefinite = MsgTimeoutIsIndefinite(timeout);
  pthread_mutex_lock(&g_msg_lock);
  if (g_msg_slot_count >= MSG_MAX_ACTIVE)
    x11_msg_remove_slot_locked(dpy, 0);
  g_msg_slots[g_msg_slot_count].win = win;
  g_msg_slots[g_msg_slot_count].indefinite = indefinite;
  g_msg_slots[g_msg_slot_count].key[0] = '\0';
  if (name && name[0])
    snprintf(g_msg_slots[g_msg_slot_count].name,
             sizeof(g_msg_slots[g_msg_slot_count].name), "%s", name);
  else
    g_msg_slots[g_msg_slot_count].name[0] = '\0';
  g_msg_slots[g_msg_slot_count].w = winW;
  g_msg_slots[g_msg_slot_count].h = winH;
  g_msg_slots[g_msg_slot_count].x = x + ax;
  g_msg_slots[g_msg_slot_count].y = y + ay;
  g_msg_slot_count++;
  pthread_mutex_unlock(&g_msg_lock);
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
                           int centerX, int centerY, int winW, int winH,
                           ColorRef color, bool no_bg) {

  if (!dpy || !key || !key[0] || !message || !message[0]) {
    return;
  }
  if (winW <= 0 || winH <= 0) {
    return;
  }
  pthread_mutex_lock(&g_msg_lock);
  int idx = x11_msg_find_key_locked(key);
  if (idx >= 0) {
    Window w = g_msg_slots[idx].win;
    if (g_msg_slots[idx].w != winW || g_msg_slots[idx].h != winH) {
      XMoveResizeWindow(dpy, w, centerX - winW / 2, centerY - winH / 2, winW,
                        winH);
      g_msg_slots[idx].w = winW;
      g_msg_slots[idx].h = winH;
    }
    g_msg_slots[idx].x = centerX - winW / 2;
    g_msg_slots[idx].y = centerY - winH / 2;
    pthread_mutex_unlock(&g_msg_lock);
    x11_display_message_draw(dpy, w, message, (unsigned long)color, winW, winH,
                             no_bg, NULL);
    return;
  }
  pthread_mutex_unlock(&g_msg_lock);

  Window win = x11_create_display_message_window(
      dpy, centerX - winW / 2, centerY - winH / 2, winW, winH, false);
  if (win == None) {
    return;
  }
  XMapRaised(dpy, win);
  XFlush(dpy);
  x11_display_message_draw(dpy, win, message, (unsigned long)color, winW, winH,
                           no_bg, NULL);

  pthread_mutex_lock(&g_msg_lock);
  if (g_msg_slot_count >= MSG_MAX_ACTIVE)
    x11_msg_remove_slot_locked(dpy, 0);
  int at = x11_msg_find_key_locked(key);
  if (at < 0)
    at = g_msg_slot_count < MSG_MAX_ACTIVE ? g_msg_slot_count
                                           : MSG_MAX_ACTIVE - 1;
  g_msg_slots[at].win = win;
  g_msg_slots[at].indefinite = true;
  snprintf(g_msg_slots[at].key, sizeof(g_msg_slots[at].key), "%s", key);
  g_msg_slots[at].w = winW;
  g_msg_slots[at].h = winH;
  g_msg_slots[at].x = centerX - winW / 2;
  g_msg_slots[at].y = centerY - winH / 2;
  if (at >= g_msg_slot_count)
    g_msg_slot_count = at + 1;
  pthread_mutex_unlock(&g_msg_lock);
  XFlush(dpy);
}

void x11_msg_measure(Display *dpy, const char *message, int maxWidth,
                     int maxHeight, int *outW, int *outH) {

  *outW = 0;
  *outH = 0;
  FontHandle font = x11_create_font_on_display(dpy, FontSize, UsedFont);
  XftFont *xf = FH_TO_XFT(font);
  int textW = 0, textH = 0;
  MsgLine lines[MSG_MAX_LINES];
  int nlines = 0;
  if (xf) {
    int wrapW = maxWidth - 30;
    if (wrapW > MSG_AUTO_WRAP_MAX_W)
      wrapW = MSG_AUTO_WRAP_MAX_W;
    nlines = x11_msg_wrap_text(dpy, xf, message, wrapW, lines, MSG_MAX_LINES);
    for (int i = 0; i < nlines; i++)
      if (lines[i].w > textW)
        textW = lines[i].w;
    textH = nlines * (xf->ascent + xf->descent);
  }
  Platform_DestroyFont(font);
  int winW = textW + 30;
  int winH = textH + 30;
  if (winW > maxWidth)
    winW = maxWidth;
  if (winH > maxHeight)
    winH = maxHeight;
  *outW = winW;
  *outH = winH;
}
