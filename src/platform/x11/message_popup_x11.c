#include "helpers/x11/raw_message_popup_x11.h"

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
  MSG_UNPACK(spec);
  if (!message)
    message = "";

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
    x11_msg_measure(dpy, message, maxWidth, maxHeight, &winW, &winH);
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
  ANCHORED_MSG_UNPACK(spec);

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
  int winW = 0;
  int winH = 0;
  x11_msg_measure(dpy, message, maxWidth, maxHeight, &winW, &winH);
  int centerX = msg_anchor_center(rect_x, rect_w, h_slot, gap, outward, winW);
  int centerY = msg_anchor_center(rect_y, rect_h, v_slot, gap, outward, winH);
  int x = centerX - winW / 2;
  int y = centerY - winH / 2;
  int ax, ay, aw, ah;
  WindowManager_GetMainMonitorGeometry(&ax, &ay, &aw, &ah);
  int stack_dir = msg_stack_dir(v_slot, outward);
  y = x11_msg_stack_adjust_y(x, y, winW, winH, stack_dir);
  x11_msg_present(dpy, message, x - ax, y - ay, winW, winH, timeout, color,
                  no_bg, render, name);
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
