#include "helpers/win32/raw_message_popup_win32.h"

void Platform_MessagePopup_PurgeOwner(const char *path_a, const char *path_b) {
  if (owner_purge_inactive(path_a, path_b))
    return;
  for (int i = g_msg_slot_count - 1; i >= 0; i--) {
    HWND h = g_msg_slots[i];
    FadeWindowData *fd = (FadeWindowData *)GetWindowLongPtrA(h, GWLP_USERDATA);
    if (!fd || fd->render_owner[0] == '\0')
      continue;
    if (!owner_module_matches(fd->render_owner, path_a, path_b))
      continue;
    fd->render = NULL;
    fd->render_owner[0] = '\0';
    KillTimer(h, 999);
    DestroyWindow(h);
  }
}

void Platform_DisplayMessage(DisplayMessageSpec spec) {
  MSG_UNPACK(spec);

  int useMaxW = max_width;
  int useMaxH = max_height;
  if (w > 0 && h > 0) {
    if (useMaxW <= 0 || w > useMaxW)
      useMaxW = w;
    if (useMaxH <= 0 || h > useMaxH)
      useMaxH = h;
  }
  if (WH_TO_HWND(win32_tray_window()) != NULL && !win32_is_main_thread()) {
    DisplayMessageParams params;
    params.message = message;
    params.x = center_x;
    params.y = center_y;
    params.max_width = useMaxW;
    params.max_height = useMaxH;
    params.timeout = timeout;
    params.color = color;
    params.no_bg = no_bg;
    params.render = render;
    params.render_data = render_data;
    params.rounded = rounded;
    params.radius = radius;
    params.name = name;
    params.fps = fps;
    SendMessageA(WH_TO_HWND(win32_tray_window()), WM_DISPLAY_MESSAGE, 0,
                 (LPARAM)&params);
    return;
  }
  Platform_CreateDisplayMessageWindow(
      (CreateDisplayMessageWindowSpec){.message = message,
                                       .x = center_x,
                                       .y = center_y,
                                       .max_width = useMaxW,
                                       .max_height = useMaxH,
                                       .timeout = timeout,
                                       .color = color,
                                       .no_bg = no_bg,
                                       .render = render,
                                       .render_data = render_data,
                                       .rounded = rounded,
                                       .radius = radius,
                                       .name = name,
                                       .fps = fps});
}

void Platform_DismissDisplayMessage(void) {
  HWND tray = WH_TO_HWND(win32_tray_window());
  if (tray != NULL && !win32_is_main_thread()) {
    SendMessageA(tray, WM_DISMISS_MESSAGE, 0, 0);
    return;
  }
  win32_msg_slot_destroy_where(MSG_DESTROY_ALL, NULL);
}

bool Platform_DismissMessage(const char *name) {
  HWND tray = WH_TO_HWND(win32_tray_window());
  if (tray != NULL && !win32_is_main_thread()) {
    DismissMessageParams params;
    params.name = name;
    return SendMessageA(tray, WM_DISMISS_MESSAGE, 0, (LPARAM)&params) != 0;
  }
  if (!name || !name[0])
    return win32_msg_slot_destroy_where(MSG_DESTROY_ALL, NULL) > 0;
  return win32_msg_slot_destroy_where(MSG_DESTROY_NAMED, name) > 0;
}

void Platform_DismissIndefiniteMessage(void) {
  win32_msg_slot_destroy_where(MSG_DESTROY_INDEFINITE, NULL);
}

bool Platform_MessageActive(const char *name) {
  if (!name || !name[0])
    return false;
  for (int i = 0; i < g_msg_slot_count; i++) {
    HWND h = g_msg_slots[i];
    if (!h || !IsWindow(h))
      continue;
    const FadeWindowData *data =
        (FadeWindowData *)GetWindowLongPtrA(h, GWLP_USERDATA);
    if (data && strcmp(data->name, name) == 0)
      return true;
  }
  return false;
}

void Platform_HoverMessageRelease(const char *key) {
  win32_msg_hover_release(key);
}

void Platform_HoverMessage(HoverMessageSpec spec) {
  win32_msg_hover_present(spec);
}

static int win32_msg_stack_adjust_y(int x, int y, int w, int h, int dir) {
  for (;;) {
    bool hit = false;
    for (int i = 0; i < g_msg_slot_count; i++) {
      HWND other = g_msg_slots[i];
      RECT r;
      if (!GetWindowRect(other, &r))
        continue;
      int ox = r.left, oy = r.top, ow = r.right - r.left, oh = r.bottom - r.top;
      if (x < ox + ow && x + w > ox && y < oy + oh && y + h > oy) {
        if (dir > 0)
          y = oy + oh + MSG_COLLISION_GAP;
        else
          y = oy - h - MSG_COLLISION_GAP;
        hit = true;
        break;
      }
    }
    if (!hit)
      return y;
  }
}

void Platform_DisplayMessageAnchored(AnchoredMessageSpec spec) {
  ANCHORED_MSG_UNPACK(spec);

  if (!message && !render) {
    return;
  }
  if (max_width <= 0)
    max_width = MSG_AUTO_WRAP_MAX_W;
  if (max_height <= 0)
    max_height = MSG_AUTO_WRAP_MAX_H;

  FontHandle msgFont = Platform_CreateFont(FontSize, UsedFont);
  int textWidth = 0, textHeight = 0;
  Platform_GetTextExtents(msgFont, message, max_width - 30, &textWidth,
                          &textHeight);
  Platform_DestroyFont(msgFont);

  int winWidth = textWidth + 30;
  int winHeight = textHeight + 30;
  if (render) {
    if (want_width > 0 && want_height > 0 && !outward) {
      winWidth = want_width;
      winHeight = want_height;
    } else if (textWidth <= 0 && textHeight <= 0) {
      winWidth = 160;
      winHeight = 48;
    }
  }
  if (winWidth > max_width)
    winWidth = max_width;
  if (winHeight > max_height)
    winHeight = max_height;

  int center_x =
      msg_anchor_center(rect_x, rect_w, h_slot, gap, outward, winWidth);
  int center_y =
      msg_anchor_center(rect_y, rect_h, v_slot, gap, outward, winHeight);

  {
    int stack_dir = msg_stack_dir(v_slot, outward);
    int stack_y = center_y - winHeight / 2;
    stack_y = win32_msg_stack_adjust_y(center_x - winWidth / 2, stack_y,
                                       winWidth, winHeight, stack_dir);
    center_y = stack_y + winHeight / 2;
  }

  log_info("MSG anchored msg='%s' rect=(%d,%d,%d,%d) slot=(%d,%d) gap=%d "
           "outward=%d -> center=(%d,%d) win=(%d,%d)",
           message, rect_x, rect_y, rect_w, rect_h, h_slot, v_slot, gap,
           outward, center_x, center_y, winWidth, winHeight);

  Platform_DisplayMessage((DisplayMessageSpec){.message = message,
                                               .x = center_x,
                                               .y = center_y,
                                               .no_bg = no_bg,
                                               .w = winWidth,
                                               .h = winHeight,
                                               .max_width = max_width,
                                               .max_height = max_height,
                                               .timeout = timeout,
                                               .color = color,
                                               .render = render,
                                               .render_data = render_data,
                                               .rounded = rounded,
                                               .radius = radius,
                                               .name = name,
                                               .fps = fps});
}

void Platform_CreateDisplayMessageWindow(CreateDisplayMessageWindowSpec spec) {
  const char *message = spec.message;
  int center_x = spec.x;
  int center_y = spec.y;
  int max_width = spec.max_width;
  int max_height = spec.max_height;
  double timeout = spec.timeout;
  ColorRef color = spec.color;
  bool no_bg = spec.no_bg;
  void (*render)(void *ud) = spec.render;
  void *render_data = spec.render_data;
  bool rounded = spec.rounded;
  int radius = spec.radius;
  const char *name = spec.name;
  int fps = spec.fps;

  log_info("MSG create enter msg='%s' center=(%d,%d) max=(%d,%d) timeout=%g "
           "no_bg=%d render=%p fps=%d",
           message ? message : "(null)", center_x, center_y, max_width,
           max_height, timeout, no_bg ? 1 : 0, (void *)render, fps);
  win32_msg_created_clear();

  FontHandle msgFont = Platform_CreateFont(FontSize, UsedFont);
  int textWidth = 0, textHeight = 0;
  int winWidth = 0, winHeight = 0;

  if ((!message || !message[0]) && max_width > 0 && max_height > 0) {
    winWidth = max_width;
    winHeight = max_height;
  } else {
    Platform_GetTextExtents(msgFont, message, max_width - 30, &textWidth,
                            &textHeight);
    int padding = 30;
    winWidth = textWidth + padding;
    winHeight = textHeight + padding;
    if (max_width > 0 && winWidth > max_width)
      winWidth = max_width;
    if (max_height > 0 && winHeight > max_height)
      winHeight = max_height;
  }

  FadeWindowData *pWndData = (FadeWindowData *)malloc(sizeof(FadeWindowData));
  if (!pWndData) {
    log_error("MSG create failed: malloc returned NULL");
    Platform_DestroyFont(msgFont);
    return;
  }

  pWndData->message = _strdup(message ? message : "");
  pWndData->hFont = msgFont;
  pWndData->alpha = WindowOpacity;
  pWndData->color = color;
  pWndData->no_bg = no_bg;
  pWndData->render = render;
  pWndData->render_data = render_data;
  pWndData->render_owner[0] = '\0';
  if (render)
    owner_resolve_for_func((void *)render, pWndData->render_owner,
                           sizeof(pWndData->render_owner));
  if (name && name[0])
    snprintf(pWndData->name, sizeof(pWndData->name), "%s", name);
  else
    pWndData->name[0] = '\0';
  pWndData->fps = fps;
  pWndData->timeout = timeout;
  if (MsgTimeoutIsIndefinite(timeout)) {
    pWndData->deadline_ms = 0;
  } else {
    pWndData->deadline_ms = GetTickCount64() + (ULONGLONG)(timeout * 1000.0);
  }

  int topLeftX = center_x - (winWidth / 2);
  int topLeftY = center_y - (winHeight / 2);

  POINT msgPt;
  msgPt.x = topLeftX + winWidth / 2;
  msgPt.y = topLeftY + winHeight / 2;
  HMONITOR hmon = MonitorFromPoint(msgPt, MONITOR_DEFAULTTONULL);
  log_info(
      "MSG computed text=(%d,%d) win=(%d,%d) at (%d,%d) font=%d monitor=%s",
      textWidth, textHeight, winWidth, winHeight, topLeftX, topLeftY,
      FH_TO_HFONT(msgFont) ? 1 : 0, hmon ? "FOUND" : "NONE");

  s_msg_show_data = pWndData;
  s_msg_show_no_bg = no_bg;
  s_msg_rounded = rounded ? rounded : RoundedCorners;
  s_msg_radius = radius > 0 ? radius : CornerRadius;
  if (!win32_message_popup_create(topLeftX, topLeftY, winWidth, winHeight)) {
    log_error("MSG create failed for '%s'", message ? message : "");
    s_msg_show_data = NULL;
    s_msg_show_no_bg = false;
    s_msg_rounded = true;
    s_msg_radius = CornerRadius;
    Platform_DestroyFont(pWndData->hFont);
    free((void *)pWndData->message);
    free(pWndData);
    return;
  }
  log_info("MSG show ok text=(%d,%d) win=(%d,%d) at (%d,%d)", textWidth,
           textHeight, winWidth, winHeight, topLeftX, topLeftY);
  return;
}
