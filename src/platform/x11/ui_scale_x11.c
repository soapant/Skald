#include "platform/platform.h"
#include "config.h"
#include "platform/x11/display_x11.h"
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <stdlib.h>

#include "core/timer.h"
static float ui_scale = 0.0f;
static int ui_scale_applied = 0;
static int base_font_size = 0;

static float ui_scale_detect(void) {
        PROFILE_START(_pt);
const char* ov = getenv("FLINT_SCALE");
    if (!ov || !ov[0]) ov = getenv("FLINT_UISCALE");
    if (ov && ov[0]) {
        float v = (float)atof(ov);
        if (v >= 1.0f && v <= 3.0f) return v;
        if (v > 3.0f) return 3.0f;
        return 1.0f;
    }
    float best = 1.0f;
    Display* dpy = get_display();
    if (dpy) {
        XrmInitialize();
        char* rms = XResourceManagerString(dpy);
        if (rms) {
            XrmDatabase db = XrmGetStringDatabase(rms);
            if (db) {
                XrmValue val;
                char* type = NULL;
                if (XrmGetResource(db, "Xft.dpi", "Xft.Dpi", &type, &val) && val.addr) {
                    float dpi = (float)atof(val.addr);
                    if (dpi >= 96.0f) {
                        float s = dpi / 96.0f;
                        if (s > best) best = s;
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
                if (s > best) best = s;
            }
        }
    }
    const char* gs = getenv("GDK_SCALE");
    if (gs && gs[0]) {
        float v = (float)atof(gs);
        if (v > best && v <= 3.0f) best = v;
    }
    const char* qs = getenv("QT_SCALE_FACTOR");
    if (qs && qs[0]) {
        float v = (float)atof(qs);
        if (v > best && v <= 3.0f) best = v;
    }
    if (best < 1.0f) best = 1.0f;
    if (best > 3.0f) best = 3.0f;
    { PROFILE_END_VAR(_pt, "ui_scale_detect"); return best; }
}

float Platform_GetUIScale(void) {
        PROFILE_START(_pt);
if (ui_scale <= 0.0f) ui_scale = ui_scale_detect();
    { PROFILE_END_VAR(_pt, "Platform_GetUIScale"); return ui_scale; }
}

static void ui_scale_apply_locked(void) {
        PROFILE_START(_pt);
float s = Platform_GetUIScale();
    if (s > 1.0f) FontSize = (int)(base_font_size * s + 0.5f);
    else FontSize = base_font_size;
}

void Platform_ApplyUIScale(void) {
        PROFILE_START(_pt);
if (ui_scale_applied) { PROFILE_END_VAR(_pt, "Platform_ApplyUIScale"); return ; }
    ui_scale_applied = 1;
    base_font_size = FontSize;
    ui_scale_apply_locked();
}

int Platform_GetUnscaledFontSize(void) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "Platform_GetUnscaledFontSize"); return base_font_size > 0 ? base_font_size : FontSize; }
}

void Platform_AdjustFontSize(int delta) {
        PROFILE_START(_pt);
if (base_font_size <= 0) base_font_size = FontSize;
    base_font_size += delta;
    if (base_font_size < 8) base_font_size = 8;
    if (base_font_size > 48) base_font_size = 48;
    ui_scale_apply_locked();
}

void Platform_ResetUIScale(void) {
        PROFILE_START(_pt);
base_font_size = FontSize;
    ui_scale_apply_locked();
}
