#include "helpers/helpers_text.h"
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>
#include <X11/extensions/shape.h>
#include <unistd.h>
#include "helpers/x11/helpers_x11.h"
#include "helpers/x11/window_factory_x11.h"

#include "core/window_manager.h"
#include "helpers/x11/bg_image_x11.h"
#include "core/prompt_geometry.h"
#include "core/listbox_hover.h"
#include "platform/platform.h"
#include "platform/draw.h"
#include "platform/x11/drawing_x11.h"
#include "helpers/helpers_ui.h"
#include "wm/niri_wm.h"
#include "config_values.h"
#include "core/types.h"
#include "core/types/window.h"
#include "freetype/freetype.h"

int Platform_GetTextLineHeight(int fontsize) {
  if (fontsize <= 0)
    fontsize = FontSize;
  Display *dpy = x11_get_display();
  if (!dpy)
    return fontsize;
  FontHandle font = x11_create_font_on_display(dpy, fontsize, UsedFont);
  XftFont *xf = FH_TO_XFT(font);
  int r = xf ? xf->ascent + xf->descent : fontsize;
  Platform_DestroyFont(font);
  return r;
}
void Platform_PrewarmBackgroundPixmap(void) {
  Display *dpy = x11_get_display();
  if (!dpy) {
    return;
  }
  x11_bg_image_prewarm(dpy, PROMPT_W, PROMPT_H);
  Window _pw = x11_create_prompt_window(dpy, 0, 0, PROMPT_W, PROMPT_H);
  if (_pw != None) {
    x11_get_cached_background(dpy, _pw, PROMPT_W, PROMPT_H);
    XDestroyWindow(dpy, _pw);
  }
}
