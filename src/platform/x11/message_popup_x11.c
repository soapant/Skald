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

void Platform_DisplayMessage(DisplayMessage spec) {
  MSG_UNPACK(spec);
  if (!message)
    message = "";

  Display *dpy = x11_get_display();
  if (!dpy) {
    return;
  }

  int win_w = 0;
  int win_h = 0;

  if (w > 0 && h > 0) {
    win_w = w;
    win_h = h;
  } else {
    if (max_width <= 0)
      max_width = MSG_AUTO_WRAP_MAX_W;
    if (max_height <= 0)
      max_height = MSG_AUTO_WRAP_MAX_H;
    x11_msg_measure(&(X11MsgMeasure){.dpy = dpy,
                                     .message = message,
                                     .max_width = max_width,
                                     .max_height = max_height,
                                     .out_w = &win_w,
                                     .out_h = &win_h});
  }

  int x = abs(center_x - win_w / 2);
  int y = abs(center_y - win_h / 2);
  x11_msg_present(&(X11MsgPresent){.dpy = dpy,
                                   .message = message,
                                   .x = x,
                                   .y = y,
                                   .win_w = win_w,
                                   .win_h = win_h,
                                   .timeout = timeout,
                                   .color = color,
                                   .no_bg = no_bg,
                                   .render = render,
                                   .render_data = render_data,
                                   .name = name,
                                   .rounded = rounded,
                                   .radius = radius,
                                   .fps = fps});
}

void Platform_HoverMessage(HoverMessage spec) {
  HOVER_MSG_UNPACK(spec);

  Display *dpy = x11_get_display();
  if (!dpy) {
    return;
  }

  if (max_width <= 0)
    max_width = MSG_HOVER_DEFAULT_W;
  if (max_height <= 0)
    max_height = MSG_HOVER_DEFAULT_H;
  int win_w = 0, win_h = 0;
  if (w > 0 && h > 0) {
    win_w = w;
    win_h = h;
  } else {
    x11_msg_measure(&(X11MsgMeasure){.dpy = dpy,
                                     .message = message,
                                     .max_width = max_width,
                                     .max_height = max_height,
                                     .out_w = &win_w,
                                     .out_h = &win_h});
  }
  x11_msg_hover_present(&(X11MsgHoverPresent){.dpy = dpy,
                                              .key = key,
                                              .message = message,
                                              .center_x = center_x,
                                              .center_y = center_y,
                                              .win_w = win_w,
                                              .win_h = win_h,
                                              .color = color,
                                              .no_bg = no_bg});
}

void Platform_DisplayMessageAnchored(AnchoredMessage spec) {
  ANCHORED_MSG_UNPACK(spec);

  if (!message && !render) {
    return;
  }
  Display *dpy = x11_get_display();
  if (!dpy) {
    return;
  }

  if (max_width <= 0)
    max_width = MSG_AUTO_WRAP_MAX_W;
  if (max_height <= 0)
    max_height = MSG_AUTO_WRAP_MAX_H;
  int win_w = 0;
  int win_h = 0;
  x11_msg_measure(&(X11MsgMeasure){.dpy = dpy,
                                   .message = message,
                                   .max_width = max_width,
                                   .max_height = max_height,
                                   .out_w = &win_w,
                                   .out_h = &win_h});
  if (render) {
    if (want_width > 0 && want_height > 0 && !outward) {
      win_w = want_width;
      win_h = want_height;
    } else if (win_w <= 0 || win_h <= 0) {
      win_w = 160;
      win_h = 48;
    }
  }
  int center_x = msg_anchor_center(&(MsgAnchorCenter){.rect = rect_x,
                                                      .rect_size = rect_w,
                                                      .slot = h_slot,
                                                      .gap = gap,
                                                      .outward = outward,
                                                      .win_size = win_w});
  int center_y = msg_anchor_center(&(MsgAnchorCenter){.rect = rect_y,
                                                      .rect_size = rect_h,
                                                      .slot = v_slot,
                                                      .gap = gap,
                                                      .outward = outward,
                                                      .win_size = win_h});
  int x = center_x - win_w / 2;
  int y = center_y - win_h / 2;
  int ax, ay, aw, ah;
  WindowManager_GetMainMonitorGeometry(&ax, &ay, &aw, &ah);
  int stack_dir = msg_stack_dir(v_slot, outward);
  y = x11_msg_stack_adjust_y(&(X11MsgStackAdjustY){
      .x = x, .y = y, .w = win_w, .h = win_h, .dir = stack_dir});
  x11_msg_present(&(X11MsgPresent){.dpy = dpy,
                                   .message = message,
                                   .x = x - ax,
                                   .y = y - ay,
                                   .win_w = win_w,
                                   .win_h = win_h,
                                   .timeout = timeout,
                                   .color = color,
                                   .no_bg = no_bg,
                                   .render = render,
                                   .render_data = render_data,
                                   .name = name,
                                   .rounded = rounded,
                                   .radius = radius,
                                   .fps = fps});
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

bool Platform_MessageSetText(const char *name, const char *text) {
  Display *dpy = x11_get_display();
  if (!dpy)
    return false;
  return x11_msg_set_text(dpy, name, text);
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
