#include "helpers/win32/raw_widget_win32.h"

static unsigned int widget_dib_color(unsigned long colorref) {
  return 0xFF000000u | ((colorref & 0x000000FFu) << 16) |
         (colorref & 0x0000FF00u) | ((colorref >> 16) & 0x000000FFu);
}

static void layer_fill(PlatformWidget *widget, unsigned int argb) {

  unsigned int *px = (unsigned int *)widget->layer_bits;
  size_t n = (size_t)widget->layer_w * (size_t)widget->layer_h;
  for (size_t i = 0; i < n; i++)
    px[i] = argb;
}

static LRESULT CALLBACK win32_widget_proc(HWND hwnd, UINT msg, WPARAM wParam,
                                          LPARAM lParam);

static void widget_queue_click(void (*fn)(void));

void win32_widget_destroy_on_owner_thread(void *ud);

void win32_widget_register_class(void) {

  static LONG registered = 0;
  if (InterlockedCompareExchange(&registered, 1, 0) != 0) {
    return;
  }
  WNDCLASSA wc = {0};
  wc.lpfnWndProc = win32_widget_proc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = NULL;
  wc.lpszClassName = WIDGET_CLASS;
  if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    log_warn("widget: RegisterClassA failed err=%lu", GetLastError());
  }
}

static void layer_release(PlatformWidget *widget) {

  if (widget->layer_dc) {
    if (widget->layer_old)
      SelectObject(widget->layer_dc, widget->layer_old);
    if (widget->layer_bmp)
      DeleteObject(widget->layer_bmp);
    DeleteDC(widget->layer_dc);
    win32_release_draw_cache();
  }
  widget->layer_dc = NULL;
  widget->layer_bmp = NULL;
  widget->layer_old = NULL;
  widget->layer_bits = NULL;
  widget->layer_w = 0;
  widget->layer_h = 0;
}

static bool layer_acquire(PlatformWidget *widget) {

  if (widget->layer_bits && widget->layer_w == widget->base.w &&
      widget->layer_h == widget->base.h)
    return true;
  layer_release(widget);
  memset(&widget->layer_bi, 0, sizeof(widget->layer_bi));
  widget->layer_bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  widget->layer_bi.bmiHeader.biWidth = widget->base.w;
  widget->layer_bi.bmiHeader.biHeight = -widget->base.h;
  widget->layer_bi.bmiHeader.biPlanes = 1;
  widget->layer_bi.bmiHeader.biBitCount = 32;
  widget->layer_bi.bmiHeader.biCompression = BI_RGB;
  HDC screen = GetDC(NULL);
  if (!screen)
    return false;
  HDC dc = CreateCompatibleDC(screen);
  ReleaseDC(NULL, screen);
  if (!dc)
    return false;
  void *bits = NULL;
  HBITMAP bmp =
      CreateDIBSection(dc, &widget->layer_bi, DIB_RGB_COLORS, &bits, NULL, 0);
  if (!bmp) {
    DeleteDC(dc);
    return false;
  }
  widget->layer_dc = dc;
  widget->layer_bmp = bmp;
  widget->layer_old = SelectObject(dc, bmp);
  widget->layer_bits = bits;
  widget->layer_w = widget->base.w;
  widget->layer_h = widget->base.h;
  return true;
}

static void widget_push_layered(PlatformWidget *widget, unsigned long bg,
                                unsigned char alpha) {

  if (!layer_acquire(widget)) {
    return;
  }

  if (widget->base.no_bg) {
    layer_fill(widget, 0xFF000000u);
  } else if (win32_bg_image_enabled()) {
    RECT full = {0, 0, widget->layer_w, widget->layer_h};
    if (!win32_draw_bg_image_slice(widget->layer_dc, &full, widget->layer_w,
                                   widget->layer_h, 0, 0)) {
      layer_fill(widget, widget_dib_color(
                             widget->base.has_bg ? widget->base.bg_color : 0u));
    }
  } else {
    layer_fill(widget, widget_dib_color(bg));
  }
  SetDrawBgColor(0);
  SetBkMode(widget->layer_dc, TRANSPARENT);
  win32_set_draw_context(widget->layer_dc);
  {
    PlDrawState *ds = PlDrawState_Get();
    ds->win32_widget_surface = true;
    ds->win32_listbox_surface = false;
  }
  if (widget->base.fn)
    widget->base.fn(widget->base.ud);
  {
    PlDrawState *ds = PlDrawState_Get();
    ds->win32_widget_surface = false;
  }
  if (!widget->base.no_bg)
    win32_draw_window_border(widget->layer_dc, 0, 0, widget->layer_w,
                             widget->layer_h, NULL);
  GdiFlush();

  {
    unsigned int *px = (unsigned int *)widget->layer_bits;
    size_t n = (size_t)widget->layer_w * (size_t)widget->layer_h;
    if (widget->base.no_bg) {
      for (size_t i = 0; i < n; i++) {
        unsigned int b = px[i] & 0xFFu;
        unsigned int g = (px[i] >> 8) & 0xFFu;
        unsigned int r = (px[i] >> 16) & 0xFFu;
        unsigned int a = b > g ? b : g;
        if (r > a)
          a = r;
        px[i] = (px[i] & 0x00FFFFFFu) | (a << 24);
      }
    } else {
      for (size_t i = 0; i < n; i++)
        px[i] |= 0xFF000000u;
    }

    if (widget->base.no_bg) {
      for (size_t i = 0; i < n; i++) {
        if ((px[i] & 0x00FFFFFFu) != 0)
          px[i] = (px[i] & 0x00FFFFFFu) | 0xFF000000u;
        else
          px[i] = 0u;
      }
    }
  }

  HDC screen = GetDC(NULL);
  if (!screen) {
    return;
  }
  POINT dst;
  dst.x = widget->base.x;
  dst.y = widget->base.y;
  SIZE size;
  size.cx = widget->layer_w;
  size.cy = widget->layer_h;
  POINT src;
  src.x = 0;
  src.y = 0;
  BLENDFUNCTION blend;
  blend.BlendOp = AC_SRC_OVER;
  blend.BlendFlags = 0;
  blend.SourceConstantAlpha = alpha;
  blend.AlphaFormat = AC_SRC_ALPHA;
  UpdateLayeredWindow(widget->hwnd, screen, &dst, &size, widget->layer_dc, &src,
                      0, &blend, ULW_ALPHA);
  ReleaseDC(NULL, screen);
}

static void widget_paint_layered(PlatformWidget *widget) {

  widget_push_layered(widget, 0xFF000000u, 255);
}

static void widget_paint(PlatformWidget *widget, HDC hdc, RECT *rect) {

  (void)hdc;
  (void)rect;
  widget_push_layered(widget, widget->base.has_bg ? widget->base.bg_color : 0u,
                      (unsigned char)WindowOpacity);
}

static LRESULT CALLBACK win32_widget_proc(HWND hwnd, UINT msg, WPARAM wParam,
                                          LPARAM lParam) {

  PlatformWidget *widget =
      (PlatformWidget *)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

  switch (msg) {
  case WM_NCCREATE: {
    CREATESTRUCTA *cs = (CREATESTRUCTA *)lParam;
    SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
    return DefWindowProcA(hwnd, msg, wParam, lParam);
  }
  case WM_NCHITTEST: {
    if (widget && widget->base.on_click)
      return HTCLIENT;
    return 0;
  }
  case WM_LBUTTONDOWN: {
    if (widget && widget->base.on_click)
      widget_queue_click(widget->base.on_click);
    return 0;
  }
  case WM_PAINT: {
    if (!widget || widget->base.dead)
      break;
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    if (widget->base.no_bg) {
      widget_paint_layered(widget);
    } else if (hdc) {
      RECT rect;
      GetClientRect(hwnd, &rect);
      widget_paint(widget, hdc, &rect);
    }
    if (hdc)
      EndPaint(hwnd, &ps);
    return 0;
  }
  case WM_NCDESTROY: {
    if (widget)
      layer_release(widget);
    SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
    break;
  }
  }
  return DefWindowProcA(hwnd, msg, wParam, lParam);
}

void win32_widget_frame(void *ud) {

  PlatformWidget *widget = (PlatformWidget *)ud;
  if (!widget || widget->base.dead) {
    return;
  }
  HWND hwnd = widget->hwnd;
  if (!hwnd || !IsWindow(hwnd)) {
    return;
  }
  DWORD now = GetTickCount();
  if (!widget->base.dirty) {
    DWORD interval =
        widget->base.fps > 0 ? (DWORD)(1000 / widget->base.fps) : 1000;
    if (interval < 16)
      interval = 16;
    if (now - widget->base.last_draw_ms < interval)
      return;
  }
  widget->base.dirty = false;
  widget->base.last_draw_ms = now;
  InvalidateRect(hwnd, NULL, FALSE);
  UpdateWindow(hwnd);
  MSG msg;
  while (PeekMessageA(&msg, hwnd, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
  }
}

void (*g_wclick[WCLICK_MAX])(void);

int g_wclick_head = 0, g_wclick_tail = 0;

static CRITICAL_SECTION wclick_cs;

static INIT_ONCE wclick_cs_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK wclick_cs_init(PINIT_ONCE once, PVOID param, PVOID *ctx) {
  InitializeCriticalSection(&wclick_cs);
  return TRUE;
}

PlatformWidget *g_wdestroy[WDESTROY_MAX];

int g_wdestroy_count = 0;

static CRITICAL_SECTION wdestroy_cs;

static INIT_ONCE wdestroy_cs_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK wdestroy_cs_init(PINIT_ONCE once, PVOID param,
                                      PVOID *ctx) {
  InitializeCriticalSection(&wdestroy_cs);
  return TRUE;
}

void win32_wclick_lock(void) {

  InitOnceExecuteOnce(&wclick_cs_once, wclick_cs_init, NULL, NULL);
  EnterCriticalSection(&wclick_cs);
}

void win32_wclick_unlock(void) { LeaveCriticalSection(&wclick_cs); }

void win32_wdestroy_lock(void) {
  InitOnceExecuteOnce(&wdestroy_cs_once, wdestroy_cs_init, NULL, NULL);
  EnterCriticalSection(&wdestroy_cs);
}

void win32_wdestroy_unlock(void) { LeaveCriticalSection(&wdestroy_cs); }

static void widget_queue_click(void (*fn)(void)) {

  if (!fn) {
    return;
  }
  win32_wclick_lock();
  int next = (g_wclick_tail + 1) % WCLICK_MAX;
  if (next != g_wclick_head) {
    g_wclick[g_wclick_tail] = fn;
    g_wclick_tail = next;
  }
  win32_wclick_unlock();
}

void win32_widget_destroy_on_owner_thread(void *ud) {

  PlatformWidget *widget = (PlatformWidget *)ud;
  if (!widget) {
    return;
  }
  if (widget->hwnd == NULL && widget->base.dead) {
    return;
  }
  widget->base.dead = true;
  RenderThread_RemoveFrameCallback(win32_widget_frame, widget);
  if (widget->hwnd && IsWindow(widget->hwnd)) {
    DestroyWindow(widget->hwnd);
  }
  widget->hwnd = NULL;
  free(widget);
}
