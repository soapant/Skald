#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <string.h>
#include <X11/X.h>
#include <X11/extensions/Xrender.h>
#include <fontconfig/fontconfig.h>
#include <stdbool.h>

#include "core/modal.h"
#include "bindings/vim_state.h"
#include "core/window_manager.h"
#include "wm/wm.h"
#include "platform/platform.h"
#include "core/prompt_geometry.h"
#include "core/editbox.h"
#include "core/hotkey_text.h"
#include "core/edit_popup_keys.h"
#include "platform/draw.h"
#include "platform/wm_platform.h"
#include "platform/x11/window_factory_x11.h"
#include "platform/x11/drawing_x11.h"
#include "platform/x11/display_x11.h"
#include "helpers/helpers_ui.h"
#include "config_values.h"
#include "core/types/window.h"

bool Platform_EditPopupCreateWindow(Popup *self, int x, int y, int w, int h) {
  WindowManager *wm = (WindowManager *)self->manager;
  Display *dpy = wm ? wm->display : NULL;
  ((WMEditPopupData *)self)->base.handle.xid =
      x11_create_edit_popup_window(dpy, x, y, w, h);
  return true;
}

void Platform_EditPopupShowWindow(Popup *self) {
  Display *dpy = ((WindowManager *)self->manager)->display;
  x11_draw_background(dpy, self->handle.xid, ((WMEditPopupData *)self)->w,
                      ((WMEditPopupData *)self)->h);
  XMapWindow(dpy, self->handle.xid);
  XFlush(dpy);
  XSync(dpy, False);
  {
    int (*old)(Display *, XErrorEvent *) = XSetErrorHandler(x11_error_handler);
    XSetInputFocus(dpy, self->handle.xid, RevertToParent, CurrentTime);
    XSync(dpy, False);
    XSetErrorHandler(old);
    for (int _f = 0; _f < 2; _f++) {
      Window _fw = None;
      int _rev = 0;
      XGetInputFocus(dpy, &_fw, &_rev);
      if (_fw == self->handle.xid)
        break;
      old = XSetErrorHandler(x11_error_handler);
      XSetInputFocus(dpy, self->handle.xid, RevertToParent, CurrentTime);
      XSync(dpy, False);
      XSetErrorHandler(old);
    }
    char *_nm = NULL;
    if (XFetchName(dpy, self->handle.xid, &_nm) && _nm) {
      if (!x11_is_own_window_name(_nm)) {
        Wm_FocusByTitle(_nm);
      }
      XFree(_nm);
    }
  }
  XFlush(dpy);
}

void Platform_EditPopupHideWindow(Popup *self) {
  Display *dpy = get_display();
  if (dpy) {
    XUnmapWindow(dpy, self->handle.xid);
    XFlush(dpy);
  }
  WindowManager *wm = WindowManager_Get();
  if (wm) {
    Popup *top = WindowManager_GetTopPopup(wm);
    if (top) {
      Display *d2 = get_display();
      if (d2) {
        XSetInputFocus(d2, top->handle.xid, RevertToParent, CurrentTime);
        XFlush(d2);
        if (top->vtable && top->vtable->on_render)
          top->vtable->on_render(top);
        XFlush(d2);
      }
    }
  }
}

void Platform_EditPopupRender(Popup *self) {
  WMEditPopupData *data = (WMEditPopupData *)self;
  Display *dpy = get_display();
  if (!dpy) {
    return;
  }
  XWindowAttributes attrs;
  if (!XGetWindowAttributes(dpy, self->handle.xid, &attrs)) {
    return;
  }
  Visual *vis = attrs.visual;
  int depth = attrs.depth;
  Colormap cmap = attrs.colormap;
  Pixmap dbl = XCreatePixmap(dpy, self->handle.xid, data->w, data->h, depth);
  XftDraw *xd = dbl ? XftDrawCreate(dpy, dbl, vis, cmap) : NULL;
  bool buffered = (dbl && xd);
  if (buffered) {
    x11_set_dbl_visual(dpy, vis, cmap, depth);
    Pixmap bg =
        x11_get_cached_background(dpy, self->handle.xid, data->w, data->h);
    if (bg) {
      GC gc = XCreateGC(dpy, dbl, 0, NULL);
      XCopyArea(dpy, bg, dbl, gc, 0, 0, data->w, data->h, 0, 0);
      XFreeGC(dpy, gc);
    } else {
      x11_draw_background_to_drawable(dpy, dbl, data->w, data->h);
    }
  } else {
    if (dbl) {
      XFreePixmap(dpy, dbl);
      dbl = None;
    }
    x11_draw_background(dpy, self->handle.xid, data->w, data->h);
    xd = x11_create_xft_draw(dpy, self->handle.xid);
    if (!xd) {
      return;
    }
  }
  FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
  XftFont *xf = FH_TO_XFT(font);
  Platform_SetDrawContext(dpy, xd, vis, cmap);
  if (xf && EditModeText) {
    Platform_DrawTextRaw((PlDrawTextRawSpec){.x = PROMPT_EDITPOPUP_MODE_X,
                                             .y = PROMPT_EDITPOPUP_MODE_Y,
                                             .text = "Insert",
                                             .color = EditModeTextColor(),
                                             .font = font,
                                             .align = "LEFT",
                                             .pad = PROMPT_EDITPOPUP_MODE_PAD,
                                             .fontsize = FontSize});
  }
  int text_len = data->text ? strlen(data->text) : 0;
  int cursor_x = 0;
  int text_w = 0;
  if (xf) {
    XGlyphInfo cext;
    XftTextExtentsUtf8(dpy, xf, (FcChar8 *)(data->text ? data->text : ""),
                       data->cursor, &cext);
    cursor_x = cext.xOff;
    if (text_len > 0) {
      XGlyphInfo textExt;
      XftTextExtentsUtf8(dpy, xf, (FcChar8 *)data->text, text_len, &textExt);
      text_w = textExt.xOff;
    }
  }
  int draw_y = data->h / 2 + PROMPT_EDITPOPUP_CENTER_OFFSET_Y;
  int sel_start = 0, sel_end = 0;
  bool has_sel = false;
  Editbox_ResolveSelection(NULL, text_len, false, &sel_start, &sel_end,
                           &has_sel);

  EditboxX11Metrics metrics = {dpy, xf};
  EditboxSpec eb;
  memset(&eb, 0, sizeof(eb));
  eb.text = data->text ? data->text : "";
  eb.text_len = text_len;
  eb.cursor = data->cursor;
  eb.sel_start = sel_start;
  eb.sel_end = sel_end;
  eb.has_selection = has_sel;
  eb.sel_mode = EDITBOX_SEL_INCLUSIVE;
  eb.insert_mode = true;
  eb.x = 0;
  eb.y = draw_y;
  eb.w = data->w;
  eb.inset = PROMPT_EDITPOPUP_INSET;
  eb.visible_w = data->w - PROMPT_EDIT_SCROLL_PAD;
  eb.scroll = 0;
  eb.cursor_x = cursor_x;
  eb.text_w = text_w;
  eb.text_y = draw_y;
  eb.caret_y =
      Editbox_CaretY(draw_y, xf ? xf->ascent : 0, xf ? xf->descent : 0);
  eb.caret_h = PromptCaretHeight();
  eb.caret_w_normal = PROMPT_EDITPOPUP_CARET_W_NORMAL;
  eb.caret_w_insert = PROMPT_CARET_W_INSERT;
  eb.align = data->align;
  eb.placeholder_dy = 0;
  eb.rainbow_limit_x = data->w - PROMPT_EDITPOPUP_INSET;
  eb.placeholder = data->placeholder;
  eb.text_color = COLOR_TEXT;
  eb.font = font;
  eb.measure = x11_editbox_measure;
  eb.measure_user = &metrics;

  Editbox_Render(&eb, NULL);

  Platform_SetDrawContext(NULL, NULL, NULL, 0);
  if (buffered) {
    x11_clear_dbl_visual();
    GC gc = XCreateGC(dpy, self->handle.xid, 0, NULL);
    XCopyArea(dpy, dbl, self->handle.xid, gc, 0, 0, data->w, data->h, 0, 0);
    XFreeGC(dpy, gc);
  }
  x11_draw_window_border(dpy, self->handle.xid, data->w, data->h, false);
  XftDrawDestroy(xd);
  if (dbl)
    XFreePixmap(dpy, dbl);
  Platform_DestroyFont(font);
  XFlush(dpy);
}

void Platform_EditPopupDestroyWindow(Popup *self) {
  Display *dpy = get_display();
  if (dpy)
    XDestroyWindow(dpy, self->handle.xid);
}

bool Platform_EditPopupOnKey(Popup *self, const PopupKeyEvent *key) {
  WMEditPopupData *ed = (WMEditPopupData *)self;
  if (!ed || !ed->text || !key)  return false;
  if (EditPopup_HandleKey(ed, key)) return true;

  int wParam = key->vk;
  bool ctrl_pressed = key->ctrl;
  if (ctrl_pressed) {
    if (wParam == 'A' || wParam == 'a') {
      EditPopup_SelectAll(ed);
      return true;
    }
    if (wParam == 'C' || wParam == 'c') {
      EditPopup_CopySelection(ed);
      return true;
    }
    if (wParam == 'C' || wParam == 'c') {
      int ss = 0, se = 0;
      if (EditState_GetSelection(&ss, &se)) {
        if (se < ss) {
          int t = ss;
          ss = se;
          se = t;
        }
        int len = (int)strlen(ed->text);
        if (ss < 0)
          ss = 0;
        if (se > len)
          se = len;
        if (se > ss) {
          char buf[1024];
          int n = se - ss;
          if (n >= (int)sizeof(buf))
            n = (int)sizeof(buf) - 1;
          memcpy(buf, ed->text + ss, n);
          buf[n] = '\0';
          Platform_CopyToClipboard(buf);
        }
      }
      return true;
    }
  }
  if (true && key->text_len > 0) {
    bool swallow = HotkeyText_ShouldSwallow(key);
    if (!swallow && HotkeyText_IsPrintable(key)) {
      EditPopup_InsertText(ed, key->text, key->text_len);
      return true;
    }
  }
  return false;
}
