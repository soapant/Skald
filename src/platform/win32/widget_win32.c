#include <windows.h>
#include <wingdi.h>
#include <stdlib.h>
#include <string.h>
#include "core/types/color.h"
#include "config_values.h"
#include "platform/widget.h"
#include "core/widget_registry.h"
#include "platform/draw.h"
#include "core/draw_state.h"
#include "helpers/win32/win32_state.h"
#include "helpers/helpers_render_thread.h"
#include "helpers/helpers_ui.h"
#include "helpers/win32/common_ui_win32.h"
#include "core/widget_registry.h"
#include "deps/log.h"
#include "helpers/win32/bg_image_win32.h"
#include "helpers/win32/widget_win32.h"









































void Platform_Widget_DrainClicks(void) {
  for (;;) {
    void (*fn)(void) = NULL;
    wclick_lock();
    if (g_wclick_head != g_wclick_tail) {
      fn = g_wclick[g_wclick_head];
      g_wclick[g_wclick_head] = NULL;
      g_wclick_head = (g_wclick_head + 1) % WCLICK_MAX;
    }
    wclick_unlock();
    if (!fn)
      break;
    fn();
  }
}



void Platform_Widget_FlushDestruction(void) {
  wdestroy_lock();
  for (int i = 0; i < g_wdestroy_count; i++) {
    widget_destroy_on_owner_thread(g_wdestroy[i]);
  }
  g_wdestroy_count = 0;
  wdestroy_unlock();
}





PlatformWidget *Platform_Widget_Create(int x, int y, int w, int h, int fps,
                                       bool no_bg, void (*on_click)(void),
                                       const char *name) {

  if (w <= 0 || h <= 0)
    return NULL;
  if (!RenderThread_EnsureStarted(fps))
    return NULL;
  widget_register_class();

  PlatformWidget *widget = (PlatformWidget *)calloc(1, sizeof(*widget));
  if (!widget)
    return NULL;
  widget_init_base(&widget->base, x, y, w, h, fps, no_bg, on_click, name);
  widget->owner_tid = GetCurrentThreadId();

  HINSTANCE hInstance = GetModuleHandle(NULL);
  DWORD exstyle =
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE;
  if (!on_click)
    exstyle |= WS_EX_TRANSPARENT;
  widget->hwnd = CreateWindowExA(exstyle, WIDGET_CLASS, NULL, WS_POPUP, x, y, w,
                                 h, NULL, NULL, hInstance, widget);
  if (!widget->hwnd) {
    log_warn("widget: CreateWindowExA failed err=%lu", GetLastError());
    free(widget);
    return NULL;
  }

  if (!widget->base.no_bg) {
    win32_apply_rounded_corners(widget->hwnd, w, h);
  }
  SetWindowPos(widget->hwnd, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

  if (!RenderThread_AddFrameCallback(widget_frame, widget)) {
    DestroyWindow(widget->hwnd);
    free(widget);
    return NULL;
  }

  ColorRef amb = GetDrawBgColor();
  if (amb) {
    widget->base.has_bg = true;
    widget->base.bg_color = amb;
  }

  widget_frame(widget);

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
  wdestroy_lock();
  if (g_wdestroy_count < WDESTROY_MAX) {
    g_wdestroy[g_wdestroy_count++] = widget;
  } else {
    widget_destroy_on_owner_thread(widget);
  }
  wdestroy_unlock();
}







void Platform_Widget_Show(PlatformWidget *widget) {
  if (!widget || widget->base.dead || widget->base.mapped) {
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
  if (!widget || widget->base.dead || !widget->base.mapped) {
    return;
  }
  if (!widget->hwnd || !IsWindow(widget->hwnd)) {
    return;
  }
  ShowWindow(widget->hwnd, SW_HIDE);
  widget->base.mapped = false;
}



void Platform_Widget_Move(PlatformWidget *widget, int x, int y) {
  if (!widget || widget->base.dead) {
    return;
  }
  if (!widget->hwnd || !IsWindow(widget->hwnd)) {
    return;
  }
  if (widget->base.x == x && widget->base.y == y) {
    return;
  }
  widget->base.x = x;
  widget->base.y = y;
  SetWindowPos(widget->hwnd, HWND_TOPMOST, x, y, 0, 0,
               SWP_NOSIZE | SWP_NOACTIVATE);
}



void Platform_Widget_SetNoBg(PlatformWidget *widget, bool no_bg) {
  if (!widget || widget->base.dead) {
    return;
  }
  widget->base.no_bg = no_bg;
  widget_invalidate(widget);
}





void Platform_Widget_SetBgColor(PlatformWidget *widget, ColorRef color) {
  if (!widget || widget->base.dead) {
    return;
  }
  widget->base.has_bg = color != 0;
  widget->base.bg_color = color;
  widget_invalidate(widget);
}



void Platform_Widget_Resize(PlatformWidget *widget, int w, int h) {
  if (!widget || widget->base.dead) {
    return;
  }
  if (w <= 0 || h <= 0) {
    return;
  }
  if (!widget->hwnd || !IsWindow(widget->hwnd)) {
    return;
  }
  if (widget->base.w == w && widget->base.h == h) {
    return;
  }
  widget->base.w = w;
  widget->base.h = h;
  if (!widget->base.no_bg)
    win32_apply_rounded_corners(widget->hwnd, w, h);
  SetWindowPos(widget->hwnd, HWND_TOPMOST, widget->base.x, widget->base.y, w, h,
               SWP_NOACTIVATE);
  widget_invalidate(widget);
}





