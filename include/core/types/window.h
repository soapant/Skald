#ifndef TYPES_WINDOW_H
#define TYPES_WINDOW_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "core/types/prompt.h"
#if defined(__linux__)
typedef struct _XDisplay Display;
typedef struct _XftFont XftFont;
#endif

typedef struct WindowHandle {
#if defined(_WIN32)
  void *hwnd;
#elif defined(__linux__)
  unsigned long xid;
#endif
} WindowHandle;

typedef struct FontHandle {
#if defined(_WIN32)
  void *hfont;
#elif defined(__linux__)
  XftFont *xfont;
  Display *dpy;
#endif
} FontHandle;

#define WH_NULL ((WindowHandle){0})

#if defined(_WIN32)
#define WH_FROM_HWND(h) ((WindowHandle){.hwnd = (void *)(h)})
#define WH_TO_HWND(h) ((h).hwnd)
#define WH_EQUALS(a, b) ((a).hwnd == (b).hwnd)
#elif defined(__linux__)
#define WH_FROM_XID(x) ((WindowHandle){.xid = (unsigned long)(x)})
#define WH_TO_XID(h) ((h).xid)
#define WH_EQUALS(a, b) ((a).xid == (b).xid)
#endif

#define FH_NULL ((FontHandle){0})

#if defined(_WIN32)
#define FH_FROM_HFONT(h) ((FontHandle){.hfont = (void *)(h)})
#define FH_TO_HFONT(h) ((h).hfont)
#elif defined(__linux__)
#define FH_TO_XFT(h) ((h).xfont)
#define FH_TO_DPY(h) ((h).dpy)
#endif

typedef struct {
  int x, y, w, h;
  int item_height;
} WindowLayout;

typedef struct {
  WindowHandle hwnd;
  char title[256];
  char exe[256];
} WindowData;

typedef struct {
  size_t r, g, b;
} PlColor;

typedef struct {
  int x, y, w, h;
  unsigned long color;
  float thickness;
  bool transparent;
  bool filled;
  int radius;
  bool rounded;
} PlRect;

typedef struct {
  int x1, y1, x2, y2;
  unsigned long color;
  float thickness;
} PlLine;

typedef struct {
  int x, y, radius;
  unsigned long color;
  float thickness;
} PlCircle;

typedef struct {
  int x, y;
} PlPos;

typedef struct {
  char **windows;
  int count;
} PlWindowList;

typedef struct {
  uint64_t ticks;
} PlTimer;

typedef struct {
  ListBoxInfo *lb;
  const char **items;
  int count;
  int x;
  int y;
  int w;
  int h;
  int item_height;
} PlListSpec;

#define GAP 20
#define IDC_INPUT_BOX 101
#define IDC_LIST_BOX 102

#endif
