#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <stdlib.h>
#include "platform/platform.h"
#include "helpers/x11/display_x11.h"
#include "config_values.h"

static float ui_scale = 0.0f;
static int ui_scale_applied = 0;
static int base_font_size = 0;

static float ui_scale_detect(void) {
  float best = 1.0f;
  Display *dpy = x11_get_display();
  if (dpy) {
    XrmInitialize();
    char *rms = XResourceManagerString(dpy);
    if (rms) {
      XrmDatabase db = XrmGetStringDatabase(rms);
      if (db) {
        XrmValue val;
        char *type = NULL;
        if (XrmGetResource(db, "Xft.dpi", "Xft.Dpi", &type, &val) && val.addr) {
          float dpi = (float)atof(val.addr);
          if (dpi >= 96.0f) {
            float s = dpi / 96.0f;
            if (s > best)
              best = s;
          }
        }
        XrmDestroyDatabase(db);
      }
    }
    int pw = DisplayWidth(dpy, DefaultScreen(dpy));
    int mmw = DisplayWidthMM(dpy, DefaultScreen(dpy));
    if (pw > 0 && mmw > 0) {
      float dpi = 25.4f * (float)pw / (float)mmw;
      if (dpi >= 96.0f) {
        float s = dpi / 96.0f;
        if (s > best)
          best = s;
      }
    }
  }
  const char *gs = getenv("GDK_SCALE");
  if (gs && gs[0]) {
    float v = (float)atof(gs);
    if (v > best && v <= 3.0f)
      best = v;
  }
  const char *qs = getenv("QT_SCALE_FACTOR");
  if (qs && qs[0]) {
    float v = (float)atof(qs);
    if (v > best && v <= 3.0f)
      best = v;
  }
  if (best < 1.0f)
    best = 1.0f;
  if (best > 3.0f)
    best = 3.0f;
  if (dpy) {
    int pw = DisplayWidth(dpy, DefaultScreen(dpy));
    int ph = DisplayHeight(dpy, DefaultScreen(dpy));
    if (pw > 0 && ph > 0) {
      int max_int_scale = pw / 900;
      if (max_int_scale < 1)
        max_int_scale = 1;
      int max_h_scale = ph / 640;
      if (max_h_scale < max_int_scale)
        max_int_scale = max_h_scale;
      if (max_int_scale < 1)
        max_int_scale = 1;
      if (best > (float)max_int_scale)
        best = (float)max_int_scale;
    }
  }
  return best;
}

float Platform_GetUIScale(void) {
  if (ui_scale <= 0.0f)
    ui_scale = ui_scale_detect();
  return ui_scale;
}

static void ui_scale_apply_locked(void) {
  float s = Platform_GetUIScale();
  if (s > 1.0f)
    FontSize = (int)(base_font_size * s + 0.5f);
  else
    FontSize = base_font_size;
}

void Platform_ApplyUIScale(void) {
  if (ui_scale_applied) {
    return;
  }
  ui_scale_applied = 1;
  base_font_size = FontSize;
  ui_scale_apply_locked();
}

int Platform_GetUnscaledFontSize(void) {
  return base_font_size > 0 ? base_font_size : FontSize;
}

int Platform_EditboxPlaceholderOffset(void) { return 0; }

void Platform_ResetUIScale(void) {
  base_font_size = FontSize;
  ui_scale_apply_locked();
}
