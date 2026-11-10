#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <X11/X.h>
#include <X11/extensions/Xrender.h>
#include <fontconfig/fontconfig.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "helpers/x11/display_x11.h"
#include "platform/x11/drawing_x11.h"
#include "helpers/x11/message_popup_x11.h"
#include "helpers/x11/window_factory_x11.h"
#include "platform/platform.h"
#include "helpers/helpers_ui.h"
#include "platform/wm_platform.h"
#include "platform/draw.h"
#include "config_values.h"
#include "core/context.h"
#include "core/types/color.h"
#include "core/types/window.h"
#include "core/window_manager.h"



void Platform_HoverMessageRelease(const char *key) {
  Display *dpy = x11_get_display();
  if (!dpy || !key || !key[0]) {
    return;
  }
  pthread_mutex_lock(&g_msg_lock);
  int idx = x11_msg_find_key_locked(key);
  if (idx >= 0)
    x11_msg_remove_slot_locked(dpy, idx);
  pthread_mutex_unlock(&g_msg_lock);
  XFlush(dpy);
}

void Platform_DisplayMessage(PlDisplayMessageSpec spec) {
  const char *message = spec.message;
  if (!message)
    message = "";
  int centerX = spec.x;
  int centerY = spec.y;
  bool no_bg = spec.no_bg;
  int w = spec.w;
  int h = spec.h;
  int maxWidth = spec.max_width;
  int maxHeight = spec.max_height;
  double timeout = spec.timeout;
  ColorRef color = spec.color;
  void (*render)(void) = spec.render;
  const char *name = spec.name;

  Display *dpy = x11_get_display();
  if (!dpy) {
    return;
  }

  int winW = 0;
  int winH = 0;

  if (w > 0 && h > 0) {
    winW = w;
    winH = h;
  } else {
    if (maxWidth <= 0)
      maxWidth = MSG_AUTO_WRAP_MAX_W;
    if (maxHeight <= 0)
      maxHeight = MSG_AUTO_WRAP_MAX_H;

    FontHandle font = x11_create_font_on_display(dpy, FontSize, UsedFont);
    XftFont *xf = FH_TO_XFT(font);
    int textW = 0;
    int textH = 0;
    MsgLine lines[MSG_MAX_LINES];
    int nlines = 0;

    if (xf) {
      int wrapW = maxWidth - 30;
      if (wrapW > MSG_AUTO_WRAP_MAX_W)
        wrapW = MSG_AUTO_WRAP_MAX_W;
      nlines = x11_msg_wrap_text(dpy, xf, message, wrapW, lines, MSG_MAX_LINES);
      for (int i = 0; i < nlines; i++) {
        if (lines[i].w > textW)
          textW = lines[i].w;
      }
      textH = nlines * (xf->ascent + xf->descent);
    }
    Platform_DestroyFont(font);

    winW = textW + 30;
    winH = textH + 30;
    if (winW > maxWidth)
      winW = maxWidth;
    if (winH > maxHeight)
      winH = maxHeight;
  }

  int x = abs(centerX - winW / 2);
  int y = abs(centerY - winH / 2);
  x11_msg_present(dpy, message, x, y, winW, winH, timeout, color, no_bg, render,
             name);
}

void Platform_HoverMessage(PlHoverMessageSpec spec) {
  const char *key = spec.key;
  const char *message = spec.message;
  int centerX = spec.x;
  int centerY = spec.y;
  int w = spec.w;
  int h = spec.h;
  int maxWidth = spec.max_width;
  int maxHeight = spec.max_height;
  ColorRef color = spec.color;
  bool no_bg = spec.no_bg;

  Display *dpy = x11_get_display();
  if (!dpy) {
    return;
  }

  if (maxWidth <= 0)
    maxWidth = MSG_HOVER_DEFAULT_W;
  if (maxHeight <= 0)
    maxHeight = MSG_HOVER_DEFAULT_H;
  int winW = 0, winH = 0;
  if (w > 0 && h > 0) {
    winW = w;
    winH = h;
  } else {
    x11_msg_measure(dpy, message, maxWidth, maxHeight, &winW, &winH);
  }
  x11_msg_hover_present(dpy, key, message, centerX, centerY, winW, winH, color,
                  no_bg);
}

void Platform_DisplayMessageAnchored(PlAnchoredMessageSpec spec) {
  const char *message = spec.message;
  int rect_x = spec.x;
  int rect_y = spec.y;
  int rect_w = spec.w;
  int rect_h = spec.h;
  int h_slot = spec.h_slot;
  int v_slot = spec.v_slot;
  int gap = spec.gap;
  int outward = spec.outward;
  int maxWidth = spec.max_width;
  int maxHeight = spec.max_height;
  double timeout = spec.timeout;
  ColorRef color = spec.color;
  bool no_bg = spec.no_bg;
  void (*render)(void) = spec.render;
  const char *name = spec.name;

  if (!message && !render) {
    return;
  }
  Display *dpy = x11_get_display();
  if (!dpy) {
    return;
  }

  if (maxWidth <= 0)
    maxWidth = MSG_AUTO_WRAP_MAX_W;
  if (maxHeight <= 0)
    maxHeight = MSG_AUTO_WRAP_MAX_H;
  FontHandle font = x11_create_font_on_display(dpy, FontSize, UsedFont);
  XftFont *xf = FH_TO_XFT(font);
  int textW = 0;
  int textH = 0;
  MsgLine lines[MSG_MAX_LINES];
  int nlines = 0;
  if (xf) {
    int wrapW = maxWidth - 30;
    if (wrapW > MSG_AUTO_WRAP_MAX_W)
      wrapW = MSG_AUTO_WRAP_MAX_W;
    nlines = x11_msg_wrap_text(dpy, xf, message, wrapW, lines, MSG_MAX_LINES);
    for (int i = 0; i < nlines; i++) {
      if (lines[i].w > textW)
        textW = lines[i].w;
    }
    textH = nlines * (xf->ascent + xf->descent);
  }
  Platform_DestroyFont(font);
  int winW = textW + 30;
  int winH = textH + 30;
  if (winW > maxWidth)
    winW = maxWidth;
  if (winH > maxHeight)
    winH = maxHeight;
  int x;
  if (h_slot == 0)
    x = outward ? rect_x - gap - winW : rect_x + gap;
  else if (h_slot == 2)
    x = outward ? rect_x + rect_w + gap : rect_x + rect_w - gap - winW;
  else
    x = rect_x + (rect_w - winW) / 2;
  int y;
  if (v_slot == 0)
    y = outward ? rect_y - gap - winH : rect_y + gap;
  else if (v_slot == 2)
    y = outward ? rect_y + rect_h + gap : rect_y + rect_h - gap - winH;
  else
    y = rect_y + (rect_h - winH) / 2;
  int ax, ay, aw, ah;
  WindowManager_GetMainMonitorGeometry(&ax, &ay, &aw, &ah);
  x11_msg_present(dpy, message, x - ax, y - ay, winW, winH, timeout, color, no_bg,
             render, name);
}

bool Platform_DismissMessage(const char *name) {
  Display *dpy = x11_get_display();
  if (!dpy)
    return false;
  bool removed = false;
  pthread_mutex_lock(&g_msg_lock);
  if (name && name[0]) {
    for (int i = 0; i < g_msg_slot_count;) {
      if (strcmp(g_msg_slots[i].name, name) == 0) {
        x11_msg_remove_slot_locked(dpy, i);
        removed = true;
      } else
        i++;
    }
  } else {
    while (g_msg_slot_count > 0) {
      x11_msg_remove_slot_locked(dpy, 0);
      removed = true;
    }
  }
  pthread_mutex_unlock(&g_msg_lock);
  XFlush(dpy);
  return removed;
}

bool Platform_MessageActive(const char *name) {
  if (!name || !name[0])
    return false;
  Display *dpy = x11_get_display();
  if (!dpy)
    return false;
  bool found = false;
  pthread_mutex_lock(&g_msg_lock);
  for (int i = 0; i < g_msg_slot_count; i++) {
    if (strcmp(g_msg_slots[i].name, name) == 0) {
      found = true;
      break;
    }
  }
  pthread_mutex_unlock(&g_msg_lock);
  return found;
}

void Platform_DismissIndefiniteMessage(void) {
  Display *dpy = x11_get_display();
  if (!dpy) {
    return;
  }
  pthread_mutex_lock(&g_msg_lock);
  x11_msg_remove_indefinite_locked(dpy);
  pthread_mutex_unlock(&g_msg_lock);
  XFlush(dpy);
}

void Platform_DismissDisplayMessage(void) { Platform_DismissMessage(NULL); }

void Platform_MessagePopup_PurgeOwner(const char *path_a, const char *path_b) {
  (void)path_a;
  (void)path_b;
}
