#include "helpers/win32/raw_widget_win32.h"

void Platform_Widget_DrainClicks(void) {
  for (;;) {
    void (*fn)(void) = NULL;
    win32_wclick_lock();
    if (g_wclick_head != g_wclick_tail) {
      fn = g_wclick[g_wclick_head];
      g_wclick[g_wclick_head] = NULL;
      g_wclick_head = (g_wclick_head + 1) % WCLICK_MAX;
    }
    win32_wclick_unlock();
    if (!fn)
      break;
    fn();
  }
}

void Platform_Widget_FlushDestruction(void) {
  win32_wdestroy_lock();
  for (int i = 0; i < g_wdestroy_count; i++) {
    win32_widget_destroy_on_owner_thread(g_wdestroy[i]);
  }
  g_wdestroy_count = 0;
  win32_wdestroy_unlock();
}

PlatformWidget *Platform_Widget_Create(PlatformWidgetCreate *argspec) {

  if (argspec->f_w <= 0 || argspec->f_h <= 0)
    return NULL;
  if (!render_thread_ensure_started(argspec->f_fps))
    return NULL;
  win32_widget_register_class();

  PlatformWidget *widget = (PlatformWidget *)calloc(1, sizeof(*widget));
  if (!widget)
    return NULL;
  widget_init_base(&(WidgetInitBase){.f_base = &widget->base, .f_x = argspec->f_x, .f_y = argspec->f_y, .f_w = argspec->f_w, .f_h = argspec->f_h, .f_fps = argspec->f_fps, .f_no_bg = argspec->f_no_bg, .f_on_click = argspec->f_on_click, .f_name = argspec->f_name});

  HINSTANCE hInstance = GetModuleHandle(NULL);
  DWORD exstyle =
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE;
  if (!argspec->f_on_click)
    exstyle |= WS_EX_TRANSPARENT;
  widget->hwnd = CreateWindowExA(exstyle, WIDGET_CLASS, NULL, WS_POPUP, argspec->f_x, argspec->f_y, argspec->f_w,
                                 argspec->f_h, NULL, NULL, hInstance, widget);
  if (!widget->hwnd) {
    log_warn("widget: CreateWindowExA failed err=%lu", GetLastError());
    free(widget);
    return NULL;
  }

  if (!widget->base.no_bg) {
    win32_apply_rounded_corners(widget->hwnd, argspec->f_w, argspec->f_h);
  }
  SetWindowPos(widget->hwnd, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

  if (!render_thread_add_frame_callback(win32_widget_frame, widget)) {
    DestroyWindow(widget->hwnd);
    free(widget);
    return NULL;
  }

  ColorRef amb = get_draw_bg_color();
  if (amb) {
    widget->base.has_bg = true;
    widget->base.bg_color = amb;
  }

  win32_widget_frame(widget);

  ShowWindow(widget->hwnd, SW_SHOWNOACTIVATE);
  SetWindowPos(widget->hwnd, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  widget->base.mapped = true;
  return widget;
}

void Platform_Widget_Destroy(PlatformWidget *widget) {
  if (!widget || widget->base.dead) {
    return;
  }
  widget->base.dead = true;
  win32_wdestroy_lock();
  if (g_wdestroy_count < WDESTROY_MAX) {
    g_wdestroy[g_wdestroy_count++] = widget;
  } else {
    win32_widget_destroy_on_owner_thread(widget);
  }
  win32_wdestroy_unlock();
}

void Platform_Widget_Show(PlatformWidget *widget) {
  if (!widget || !widget_base_ready(&widget->base, false)) {
    return;
  }
  if (!widget->hwnd || !IsWindow(widget->hwnd)) {
    return;
  }
  ShowWindow(widget->hwnd, SW_SHOWNOACTIVATE);
  SetWindowPos(widget->hwnd, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  widget->base.mapped = true;
}

void Platform_Widget_Hide(PlatformWidget *widget) {
  if (!widget || !widget_base_ready(&widget->base, true)) {
    return;
  }
  if (!widget->hwnd || !IsWindow(widget->hwnd)) {
    return;
  }
  ShowWindow(widget->hwnd, SW_HIDE);
  widget->base.mapped = false;
}

void Platform_Widget_Move(PlatformWidget *widget, int x, int y) {
  if (!widget || !widget_base_move(&widget->base, x, y)) {
    return;
  }
  if (!widget->hwnd || !IsWindow(widget->hwnd)) {
    return;
  }
  SetWindowPos(widget->hwnd, HWND_TOPMOST, x, y, 0, 0,
               SWP_NOSIZE | SWP_NOACTIVATE);
}

void Platform_Widget_SetNoBg(PlatformWidget *widget, bool no_bg) {
  if (!widget || !widget_base_set_no_bg(&widget->base, no_bg)) {
    return;
  }
  widget_invalidate(widget);
}

void Platform_Widget_SetBgColor(PlatformWidget *widget, ColorRef color) {
  if (!widget || !widget_base_set_bg_color(&widget->base, color)) {
    return;
  }
  widget_invalidate(widget);
}

void Platform_Widget_Resize(PlatformWidget *widget, int w, int h) {
  if (!widget || !widget_base_resize(&widget->base, w, h)) {
    return;
  }
  if (!widget->hwnd || !IsWindow(widget->hwnd)) {
    return;
  }
  if (!widget->base.no_bg)
    win32_apply_rounded_corners(widget->hwnd, w, h);
  SetWindowPos(widget->hwnd, HWND_TOPMOST, widget->base.x, widget->base.y, w, h,
               SWP_NOACTIVATE);
  widget_invalidate(widget);
}

