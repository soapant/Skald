#ifndef HELPERS_WIN32_WIDGET_H
#define HELPERS_WIN32_WIDGET_H

#include <windows.h>
#include <stdbool.h>
#include "core/types/color.h"
#include "platform/widget.h"

#define WCLICK_MAX 32
#define WDESTROY_MAX 32
#define WIDGET_CLASS "SkaldWidget"

struct PlatformWidget {
  PlatformWidgetBase base;
  HWND hwnd;
  DWORD owner_tid;
  BITMAPINFO layer_bi;
  HDC layer_dc;
  HBITMAP layer_bmp;
  HGDIOBJ layer_old;
  void *layer_bits;
  int layer_w, layer_h;
};

extern void (*g_wclick[WCLICK_MAX])(void);
extern int g_wclick_head, g_wclick_tail;
extern PlatformWidget *g_wdestroy[WDESTROY_MAX];
extern int g_wdestroy_count;

void win32_widget_register_class(void);
void win32_widget_frame(void *ud);
void win32_widget_destroy_on_owner_thread(void *ud);
void win32_wclick_lock(void);
void win32_wclick_unlock(void);
void win32_wdestroy_lock(void);
void win32_wdestroy_unlock(void);

#endif
