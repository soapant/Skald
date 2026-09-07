#include "core/context.h"
#include "core/modal.h"
#include "config.h"
#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#if !defined(_WIN32)
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/shape.h>
#include <unistd.h>
#endif
#include "helpers.h"
#include "core/timer.h"
#include "core/common_ui.h"
#include "platform/x11/drawing.h"

#if !defined(_WIN32)
// --- Cached background pixmap (double-buffer + shader cache) ---
static Pixmap g_bgPixmap = None;
static int g_bgW = 0, g_bgH = 0;
static unsigned long g_bgBase = 0;
static int g_bgPreset = -1;
static float g_bgIntensity = -1.0f;
static int g_bgRadius = -1;
static bool g_bgRounded = false;
static Display* g_bgDpy = NULL;
static Visual* g_dblVisual = NULL;
static Colormap g_dblColormap = None;
static int g_dblDepth = 0;
static Display* g_dblDpy = NULL;
static int g_bgDepth = 0;

void Linux_SetDblVisual(Display* dpy, Visual* vis, Colormap cmap, int depth) {
    g_dblVisual = vis;
    g_dblColormap = cmap;
    g_dblDepth = depth;
    g_dblDpy = dpy;
}
void Linux_ClearDblVisual(void) {
    g_dblVisual = NULL;
    g_dblColormap = None;
    g_dblDepth = 0;
    g_dblDpy = NULL;
}
bool Linux_GetDrawableVisual(Display* dpy, Drawable d, Visual** visOut, Colormap* cmapOut, int* depthOut) {
    XWindowAttributes attrs;
    if (XGetWindowAttributes(dpy, d, &attrs)) {
        if (visOut) *visOut = attrs.visual;
        if (cmapOut) *cmapOut = attrs.colormap;
        if (depthOut) *depthOut = attrs.depth;
        return true;
    }
    if (g_dblVisual && g_dblDpy == dpy) {
        if (visOut) *visOut = g_dblVisual;
        if (cmapOut) *cmapOut = g_dblColormap;
        if (depthOut) *depthOut = g_dblDepth;
        return true;
    }
    return false;
}

static int xft_width(Display* dpy, XftFont* xf, const char* text, int len) {
    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy, xf, (FcChar8*)text, len, &ext);
    return ext.xOff;
}

void draw_wrapped_text(Display* dpy, XftDraw* xd, XftColor* color, FontHandle font, const char* text, int startY, char* centered_to, int xOffset) {
    if (!text) return;
    XftFont* xf = FH_TO_XFT(font);
    if (!xf) return;
    int lineHeight = xf->ascent + xf->descent;
    int spaceWidth = xft_width(dpy, xf, " ", 1);
    int y = startY + xf->ascent;
    int text_len = strlen(text);
    int maxWidth = 0;   
    int alignment = 0;

    if (centered_to != NULL) {
        if (strstr(centered_to, "CENTER") != NULL) {
            alignment = 1;
            maxWidth = 870;
        } else if (strstr(centered_to, "RIGHT") != NULL) {
            alignment = 2;
            maxWidth = 900;
        } else {
            alignment = 0;
            maxWidth = FontSize * text_len;
        }
    } else {
        alignment = 0;
        maxWidth = FontSize * text_len;
    }

    const char* p = text;
    const char* lineStart = text;
    int lineWidth = 0;

    while (*p) {
        const char* word_start = p;
        while (*p && *p != ' ') p++;
        int wordWidth = xft_width(dpy, xf, word_start, p - word_start);
        int add = wordWidth + (lineWidth > 0 ? spaceWidth : 0);
        
        if (lineWidth > 0 && lineWidth + add > maxWidth) {
            int lw = xft_width(dpy, xf, lineStart, word_start - lineStart);
            
            int drawX = 0;
            if (alignment == 1) drawX = (maxWidth - lw) / 2 + xOffset;
            else if (alignment == 2) drawX = maxWidth - lw - xOffset;      
            else                     drawX = xOffset;                      
            
            XftDrawStringUtf8(xd, color, xf, drawX, y, (FcChar8*)lineStart, word_start - lineStart);
            y += lineHeight;
            lineStart = word_start;
            lineWidth = wordWidth;
        } else {
            lineWidth += add;
        }
        while (*p == ' ') p++;
    }
    
    if (p > lineStart) {
        int lw = xft_width(dpy, xf, lineStart, p - lineStart);

        int drawX = 0;
        if (alignment == 1)      drawX = xOffset + (maxWidth - lw) / 2;
        else if (alignment == 2) drawX = maxWidth - lw - xOffset;
        else                     drawX = xOffset;

        XftDrawStringUtf8(xd, color, xf, drawX, y, (FcChar8*)lineStart, p - lineStart); 
    }
}

void Linux_XftAllocColor(Display* dpy, Visual* vis, Colormap cmap, unsigned long hex, unsigned short alpha, XftColor* out) {
    XRenderColor rc = {
        ((hex >> 16) & 0xFF) * 0x101,
        ((hex >> 8) & 0xFF) * 0x101,
        (hex & 0xFF) * 0x101,
        alpha
    };
    XftColorAllocValue(dpy, vis, cmap, &rc, out);
}

unsigned long VimModeTextColor(void) {
    if (g_current_mode == MODE_NORMAL) return rgb_to_hex(COLOR_NORMAL_MODE_TEXT);
    if (g_current_mode == MODE_INSERT) return rgb_to_hex(COLOR_INSERT_MODE_TEXT);
    return rgb_to_hex(COLOR_VISUAL_MODE_TEXT);
}

int Linux_UpdateEditScrollOffset(Display* dpy, XftFont* xf, const char* text, int text_len, int cursor, int width, int prev_scroll, int* out_cursor_x) {
    int visible_w = width - 40;
    int cursor_x = 0;
    int text_w = 0;
    if (xf) {
        XGlyphInfo cext;
        XftTextExtentsUtf8(dpy, xf, (FcChar8*)text, cursor, &cext);
        cursor_x = cext.xOff;
        XGlyphInfo textExt;
        XftTextExtentsUtf8(dpy, xf, (FcChar8*)text, text_len, &textExt);
        text_w = textExt.xOff;
    }
    int scroll = prev_scroll;
    int margin = 20;
    if (cursor_x - scroll > visible_w - margin) scroll = cursor_x - visible_w + margin;
    if (cursor_x - scroll < margin) scroll = cursor_x - margin;
    if (scroll < 0) scroll = 0;
    if (text_w - scroll < visible_w && scroll > 0) {
        if (text_w < visible_w) scroll = 0;
        else scroll = text_w - visible_w;
    }
    if (cursor_x - scroll < 0) scroll = cursor_x;
    if (cursor_x - scroll > visible_w) scroll = cursor_x - visible_w;
    *out_cursor_x = cursor_x;
    return scroll;
}

void Linux_DrawEditCursor(XftDraw* xd, XftColor* color, int cursor_x, int scroll, int width) {
    int cursor_draw_x = 22 + cursor_x - scroll;
    if (cursor_draw_x < 20) cursor_draw_x = 20;
    if (cursor_draw_x > width - 10) cursor_draw_x = width - 10;
    if (IsInsertMode()) {
        XftDrawRect(xd, color, cursor_draw_x, 47, 2, FontSize);
    } else {
        XftDrawRect(xd, color, cursor_draw_x, 47, 12, FontSize);
    }
}

const char* ListBoxItemDisplayText(ListBoxInfo* lb, int idx, char* trunc, size_t trunc_size) {
    int raw = (idx >= 0 && idx < lb->populated_count) ? lb->matching_indices[idx] : -1;
    const char* baseText = NULL;
    if (g_alias_display_active && raw >= 0 && raw < g_alias_display_count && g_alias_display[raw][0] != '\0')
        baseText = g_alias_display[raw];
    else
        baseText = lb->populated_cache[idx];
    snprintf(trunc, trunc_size, "%.80s", baseText ? baseText : "");
    return trunc;
}

void DrawListBoxItemText(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, FontHandle font,
                         XftColor* num_color, XftColor* body_color, XftColor* plain_color,
                         ListBoxInfo* lb, int idx, int y, int text_x) {
    XftFont* xf = FH_TO_XFT(font);
    if (!xf) return;


    char trunc[512];
    ListBoxItemDisplayText(lb, idx, trunc, sizeof(trunc));
    int XOffset = 10;
    if (EnableLineNumbers) {
        char num[8];
        snprintf(num, sizeof(num), "%d.", idx + 1);
        draw_wrapped_text(dpy, xd, num_color, font, num, y, "LEFT", 10);
        XOffset = get_offset_based_on_number(idx);
    }

    char* alias_text = strstr(trunc, " [");

    if (alias_text) {
        XftColor aliasCol;
        int XOffsetAlias = 0;
        Linux_XftAllocColor(dpy, vis, cmap, rgb_to_hex(COLOR_ALIAS_TEXT), 0xFFFF, &aliasCol);
        eval_xoffset_for_alias(alias_text,strlen(trunc),&XOffsetAlias);
        draw_wrapped_text(dpy, xd, &aliasCol, font, alias_text, y, "LEFT",XOffsetAlias);
        XftColorFree(dpy, vis, cmap, &aliasCol);
    }

    char* pos =strchr(trunc,'['); 
    if(pos) {
      *pos = 0;
    }
    draw_wrapped_text(dpy, xd, plain_color, font, trunc, y, "LEFT", XOffset);

}

bool CursorBack(size_t* cursor, size_t text_len) {
    if (*cursor == 0) return false; 
    (*cursor)--;
    return true;
}

bool CursorForward(size_t* cursor, size_t text_len) {
    if (*cursor >= text_len) return false;
    (*cursor)++;
    return true;
}

static int x11_error_occurred = 0;

static int dummy_x_error_handler(Display *dpy, XErrorEvent *error) {
    if (error->error_code == BadWindow) {
        x11_error_occurred = 1;
        return 0;
    }
    return 0;
}

pid_t GetWindowPid(Display* dpy, Window w) {
    if (w == 0) return -1;

    Atom prop = XInternAtom(dpy, "_NET_WM_PID", True);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char* prop_data = NULL;

    x11_error_occurred = 0;
    int (*old_handler)(Display*, XErrorEvent*) = XSetErrorHandler(dummy_x_error_handler);

    pid_t pid = -1;
    if (prop != None) {
        int status = XGetWindowProperty(dpy, w, prop, 0, 1, False, XA_CARDINAL,
                                        &actual_type, &actual_format, &nitems,
                                        &bytes_after, &prop_data);
        if (status == Success && !x11_error_occurred && prop_data && nitems > 0) {
            pid = *((pid_t*)prop_data);
        }
    }

    if (prop_data) {
        XFree(prop_data);
    }

    XSetErrorHandler(old_handler);

    if (x11_error_occurred) {
        return -1; // Window was destroyed mid-call
    }

    return pid;
}

void GetExeNameFromPid(pid_t pid, char* outExe, size_t maxLen) {
    if (pid <= 0) {
        strncpy(outExe, "unknown", maxLen);
        return;
    }

    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/exe", pid);
    
    ssize_t len = readlink(path, outExe, maxLen - 1);
    if (len != -1) {
        outExe[len] = '\0';
        char* basename = strrchr(outExe, '/');
        if (basename) {
            memmove(outExe, basename + 1, strlen(basename));
        }
    } else {
        strncpy(outExe, "unknown", maxLen);
    }
}

int get_offset_based_on_number(int index) {
    if (index < 9) {        
        return 35;
    } else if (index < 99) { 
        return 50;
    } else if (index < 999) {
        return 60;
    } else if (index < 9999) {
        return 70;
    }
    return 0;
}

Window Linux_CreateWindowExOR(Display* dpy, int x, int y, int w, int h, unsigned long bg_color, const char* res_name, const char* res_class, long event_mask, bool override_redirect);

Window Linux_CreateWindowEx(Display* dpy, int x, int y, int w, int h, unsigned long bg_color, const char* res_name, const char* res_class, long event_mask) {
    return Linux_CreateWindowExOR(dpy, x, y, w, h, bg_color, res_name, res_class, event_mask, false);
}

bool Linux_ShouldUseRounded(void) { return RoundedCorners; }

static void Linux_ApplyShapeRounded(Display* dpy, Window win, int w, int h, int radius);

Window Linux_CreateWindowExOR(Display* dpy, int x, int y, int w, int h, unsigned long bg_color, const char* res_name, const char* res_class, long event_mask, bool override_redirect) {
    int screen = DefaultScreen(dpy);
    XVisualInfo vinfo;
    Visual* visual = NULL;
    int depth = 0;
    Colormap cmap = 0;
    bool use_argb = false;
    static Colormap cached_cmap = None;
    static Visual* cached_visual = NULL;
    static int cached_screen = -1;
    if (XMatchVisualInfo(dpy, screen, 32, TrueColor, &vinfo)) {
        visual = vinfo.visual;
        depth = 32;
        if (cached_cmap == None || cached_screen != screen || cached_visual != visual) {
            if (cached_cmap != None) XFreeColormap(dpy, cached_cmap);
            cached_cmap = XCreateColormap(dpy, RootWindow(dpy, screen), visual, AllocNone);
            cached_visual = visual;
            cached_screen = screen;
        }
        cmap = cached_cmap;
        use_argb = true;
    }
    Window win;
    bool rounded = Linux_ShouldUseRounded() && use_argb;
    if (use_argb) {
        XSetWindowAttributes attrs = {0};
        unsigned int r = (bg_color >> 16) & 0xFF;
        unsigned int g = (bg_color >> 8) & 0xFF;
        unsigned int b = bg_color & 0xFF;
        unsigned int a = rounded ? 0 : (unsigned int)WindowOpacity;
        attrs.background_pixel = (a << 24) | (r << 16) | (g << 8) | b;
        attrs.colormap = cmap;
        attrs.border_pixel = 0;
        if (override_redirect) attrs.override_redirect = True;
        win = XCreateWindow(dpy, RootWindow(dpy, screen), x, y, w, h, 0, depth, InputOutput, visual,
                            CWBackPixel|CWColormap|CWBorderPixel|(override_redirect ? CWOverrideRedirect : 0), &attrs);
    } else {
        XSetWindowAttributes attrs = {0};
        if (override_redirect) attrs.override_redirect = True;
        win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), x, y, w, h, 0, bg_color, bg_color);
        if (override_redirect) XChangeWindowAttributes(dpy, win, CWOverrideRedirect, &attrs);
    }
    if (!override_redirect) {
        Atom wm_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
        Atom wm_type_dialog = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
        XChangeProperty(dpy, win, wm_type, XA_ATOM, 32, PropModeReplace, (unsigned char*)&wm_type_dialog, 1);
    } else if (res_name && strcmp(res_name, "flashtext")==0) {
        Atom wm_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
        Atom wm_type_note = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
        XChangeProperty(dpy, win, wm_type, XA_ATOM, 32, PropModeReplace, (unsigned char*)&wm_type_note, 1);
        Atom wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
        Atom state_skip_taskbar = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
        Atom state_skip_pager = XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER", False);
        Atom states[2] = {state_skip_taskbar, state_skip_pager};
        XChangeProperty(dpy, win, wm_state, XA_ATOM, 32, PropModeReplace, (unsigned char*)states, 2);
    }
    if (res_name || res_class) {
        XClassHint* ch = XAllocClassHint();
        ch->res_name = (char*)(res_name ? res_name : "colored");
        ch->res_class = (char*)(res_class ? res_class : "colored");
        XSetClassHint(dpy, win, ch);
        XFree(ch);
    }
    XWMHints* hints = XAllocWMHints();
    hints->flags = InputHint;
    hints->input = True;
    if (res_name && strcmp(res_name, "flashtext")==0) hints->input = False;
    XSetWMHints(dpy, win, hints);
    XFree(hints);
    XSelectInput(dpy, win, event_mask);
    if (Linux_ShouldUseRounded() && !use_argb) {
        Linux_ApplyShapeRounded(dpy, win, w, h, CornerRadius);
    }
    return win;
}

static void Linux_ApplyShapeRounded(Display* dpy, Window win, int w, int h, int radius) {
    if (radius <= 0 || w <= 0 || h <= 0) return;
    Pixmap mask = XCreatePixmap(dpy, win, w, h, 1);
    GC gc = XCreateGC(dpy, mask, 0, NULL);
    XSetForeground(dpy, gc, 0);
    XFillRectangle(dpy, mask, gc, 0, 0, w, h);
    XSetForeground(dpy, gc, 1);
    // central rects
    XFillRectangle(dpy, mask, gc, radius, 0, w - 2*radius, h);
    XFillRectangle(dpy, mask, gc, 0, radius, w, h - 2*radius);
    // corners as filled arcs (1-bit, slightly pixelated but fallback)
    XFillArc(dpy, mask, gc, 0, 0, radius*2, radius*2, 0, 360*64);
    XFillArc(dpy, mask, gc, w - radius*2 -1, 0, radius*2, radius*2, 0, 360*64);
    XFillArc(dpy, mask, gc, 0, h - radius*2 -1, radius*2, radius*2, 0, 360*64);
    XFillArc(dpy, mask, gc, w - radius*2 -1, h - radius*2 -1, radius*2, radius*2, 0, 360*64);
    XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, mask, ShapeSet);
    XFreePixmap(dpy, mask);
    XFreeGC(dpy, gc);
}

void Linux_FillRoundedRect(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, unsigned long hex, unsigned short alpha16, int x, int y, int w, int h, int radius) {
    if (!RoundedCorners || radius <= 0 || w <= 0 || h <= 0) {
        XftColor c;
        Linux_XftAllocColor(dpy, vis, cmap, hex, alpha16, &c);
        XftDrawRect(xd, &c, x, y, w, h);
        XftColorFree(dpy, vis, cmap, &c);
        return;
    }
    if (radius > w/2) radius = w/2;
    if (radius > h/2) radius = h/2;
    XftColor solid;
    Linux_XftAllocColor(dpy, vis, cmap, hex, alpha16, &solid);
    // central rects (no corners)
    XftDrawRect(xd, &solid, x + radius, y, w - 2*radius, h);
    XftDrawRect(xd, &solid, x, y + radius, radius, h - 2*radius);
    XftDrawRect(xd, &solid, x + w - radius, y + radius, radius, h - 2*radius);
    // corners with anti-alias via per-pixel 1x1 with varying alpha
    for (int corner = 0; corner < 4; corner++) {
        int cx = (corner % 2 == 0) ? x + radius : x + w - radius - 1;
        int cy = (corner < 2) ? y + radius : y + h - radius - 1;
        int ox0 = (corner % 2 == 0) ? x : x + w - radius;
        int oy0 = (corner < 2) ? y : y + h - radius;
        for (int dy = 0; dy < radius; dy++) {
            for (int dx = 0; dx < radius; dx++) {
                float fx = (corner % 2 == 0) ? (radius - dx - 0.5f) : (dx + 0.5f);
                float fy = (corner < 2) ? (radius - dy - 0.5f) : (dy + 0.5f);
                // Actually compute distance from corner center
                float ddx = (corner % 2 == 0) ? (radius - dx - 0.5f) : (dx + 0.5f - radius);
                // Simpler: distance from (cx,cy)
                float px = ox0 + dx + 0.5f;
                float py = oy0 + dy + 0.5f;
                float ddx2 = px - (cx + 0.5f);
                float ddy2 = py - (cy + 0.5f);
                float dist = sqrtf(ddx2*ddx2 + ddy2*ddy2);
                if (dist >= radius + 0.5f) continue;
                float coverage = 1.0f;
                if (dist > radius - 0.5f) coverage = (radius + 0.5f - dist);
                if (coverage <= 0) continue;
                if (coverage >= 1.0f) {
                    XftDrawRect(xd, &solid, ox0 + dx, oy0 + dy, 1, 1);
                } else {
                    unsigned short a = (unsigned short)(alpha16 * coverage + 0.5f);
                    XftColor ac;
                    Linux_XftAllocColor(dpy, vis, cmap, hex, a, &ac);
                    XftDrawRect(xd, &ac, ox0 + dx, oy0 + dy, 1, 1);
                    XftColorFree(dpy, vis, cmap, &ac);
                }
            }
        }
    }
    XftColorFree(dpy, vis, cmap, &solid);
}

void Linux_DrawRoundedBackground(Display* dpy, Window win, int w, int h, unsigned long hex, unsigned short alpha) {
    if (!RoundedCorners) return;
    if (CornerRadius <= 0) return;
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(dpy, win, &attrs)) return;
    XftDraw* xd = XftDrawCreate(dpy, win, attrs.visual, attrs.colormap);
    if (!xd) return;
    Linux_FillRoundedRect(dpy, xd, attrs.visual, attrs.colormap, hex, alpha, 0, 0, w, h, CornerRadius);
    XftDrawDestroy(xd);
}

// --- Shader helpers (derived from COLOR_BG only) ---
static unsigned long LerpHex(unsigned long a, unsigned long b, float t) {
    if (t < 0) t = 0; if (t > 1) t = 1;
    unsigned int ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    unsigned int br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
    unsigned int rr = (unsigned int)(ar + (br - ar) * t + 0.5f);
    unsigned int rg = (unsigned int)(ag + (bg - ag) * t + 0.5f);
    unsigned int rb = (unsigned int)(ab + (bb - ab) * t + 0.5f);
    return (rr << 16) | (rg << 8) | rb;
}
static unsigned long DeriveLighter(unsigned long base, float intensity) {
    // intensity 0..1, base 5,5,10 -> lighter up to ~ 60,60,110 at 1.0
    float f = ShaderIntensity;
    if (f < 0) f = 0; if (f > 1) f = 1;
    // combine global intensity with per-call intensity
    float eff = f * intensity;
    unsigned int r = (base >> 16) & 0xFF, g = (base >> 8) & 0xFF, b = base & 0xFF;
    // move towards 255 + slight hue shift (keep bluish)
    unsigned int nr = r + (unsigned int)((90 - r) * eff + 35 * eff);
    unsigned int ng = g + (unsigned int)((90 - g) * eff + 35 * eff);
    unsigned int nb = b + (unsigned int)((140 - b) * eff + 40 * eff);
    if (nr > 255) nr = 255; if (ng > 255) ng = 255; if (nb > 255) nb = 255;
    return (nr << 16) | (ng << 8) | nb;
}
static unsigned int HashNoise(int x, int y) {
    unsigned int h = (unsigned int)(x * 374761393U + y * 668265263U);
    h = (h ^ (h >> 13)) * 1274126177U;
    return h & 0xFF;
}

void Linux_FreeCachedBackground(void) {
    if (g_bgPixmap != None && g_bgDpy) {
        XFreePixmap(g_bgDpy, g_bgPixmap);
        g_bgPixmap = None;
    }
    g_bgW = g_bgH = 0;
    g_bgBase = 0;
    g_bgPreset = -1;
    g_bgIntensity = -1.0f;
    g_bgRadius = -1;
    g_bgRounded = false;
    g_bgDpy = NULL;
    g_bgDepth = 0;
}

static Pixmap Linux_CreateBackgroundPixmap(Display* dpy, Window win, int w, int h, XWindowAttributes* attrs, unsigned long base, unsigned int baseAlpha8) {
    Visual* vis = attrs->visual;
    int depth = attrs->depth;
    bool hasAlpha = (depth == 32);
    bool rounded = RoundedCorners && CornerRadius > 0 && hasAlpha;
    int R = rounded ? CornerRadius : 0;
    if (R > w/2) R = w/2; if (R > h/2) R = h/2;
    Pixmap pm = XCreatePixmap(dpy, win, w, h, depth);
    if (!pm) return None;
    XImage* img = XCreateImage(dpy, vis, depth, ZPixmap, 0, NULL, w, h, 32, 0);
    if (!img) { XFreePixmap(dpy, pm); return None; }
    img->data = (char*)calloc(1, h * img->bytes_per_line);
    if (!img->data) { XDestroyImage(img); XFreePixmap(dpy, pm); return None; }
    unsigned long light = DeriveLighter(base, 1.0f);
    if (!EnableShaders || ShaderPreset == 0) {
        unsigned int r = (base >> 16) & 0xFF, g = (base >> 8) & 0xFF, b = base & 0xFF;
        unsigned long pixel = hasAlpha ? (baseAlpha8 << 24) | (r << 16) | (g << 8) | b : (r << 16) | (g << 8) | b;
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                bool inside = true;
                unsigned int a = baseAlpha8;
                if (rounded) {
                    if (x < R && y < R) { float dx = R - x -0.5f, dy = R - y -0.5f; float d = sqrtf(dx*dx+dy*dy); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                    else if (x >= w - R && y < R) { float dx = x - (w - R) +0.5f, dy = R - y -0.5f; float d = sqrtf(dx*dx+dy*dy); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                    else if (x < R && y >= h - R) { float dx = R - x -0.5f, dy = y - (h - R) +0.5f; float d = sqrtf(dx*dx+dy*dy); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                    else if (x >= w - R && y >= h - R) { float dx = x - (w - R) +0.5f, dy = y - (h - R) +0.5f; float d = sqrtf(dx*dx+dy*dy); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                }
                unsigned long px = 0;
                if (!inside) { px = hasAlpha ? 0 : pixel; if (!hasAlpha) px = 0; else a = 0; }
                if (inside) px = hasAlpha ? (a << 24) | (r << 16) | (g << 8) | b : (r << 16) | (g << 8) | b;
                else if (hasAlpha) px = 0;
                XPutPixel(img, x, y, px);
            }
        }
    } else if (ShaderPreset == 1) { // vertical
        for (int y = 0; y < h; y++) {
            float t = h > 1 ? (float)y / (float)(h - 1) : 0;
            unsigned long col = LerpHex(base, light, t * 0.9f);
            unsigned int r = (col >> 16) & 0xFF, g = (col >> 8) & 0xFF, b = col & 0xFF;
            for (int x = 0; x < w; x++) {
                bool inside = true;
                unsigned int a = baseAlpha8;
                if (rounded) {
                    if (x < R && y < R) { float dx = R - x -0.5f, dy = R - y -0.5f; float d = sqrtf(dx*dx+dy*dy); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                    else if (x >= w - R && y < R) { float dx = x - (w - R) +0.5f, dy = R - y -0.5f; float d = sqrtf(dx*dx+dy*dy); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                    else if (x < R && y >= h - R) { float dx = R - x -0.5f, dy = y - (h - R) +0.5f; float d = sqrtf(dx*dx+dy*dy); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                    else if (x >= w - R && y >= h - R) { float dx = x - (w - R) +0.5f, dy = y - (h - R) +0.5f; float d = sqrtf(dx*dx+dy*dy); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                }
                unsigned long px = hasAlpha ? (a << 24) | (r << 16) | (g << 8) | b : (r << 16) | (g << 8) | b;
                if (!inside) px = hasAlpha ? 0 : 0;
                XPutPixel(img, x, y, px);
            }
        }
    } else if (ShaderPreset == 3) { // diagonal
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                float t = (w + h > 0) ? ((float)(x + y) / (float)(w + h)) : 0;
                unsigned long col = LerpHex(base, light, t * 0.9f);
                unsigned int r = (col >> 16) & 0xFF, g = (col >> 8) & 0xFF, b = col & 0xFF;
                bool inside = true;
                unsigned int a = baseAlpha8;
                if (rounded) {
                    if (x < R && y < R) { float dx = R - x -0.5f, dy = R - y -0.5f; float d = sqrtf(dx*dx+dy*dy); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                    else if (x >= w - R && y < R) { float dx = x - (w - R) +0.5f, dy = R - y -0.5f; float d = sqrtf(dx*dx+dy*dy); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                    else if (x < R && y >= h - R) { float dx = R - x -0.5f, dy = y - (h - R) +0.5f; float d = sqrtf(dx*dx+dy*dy); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                    else if (x >= w - R && y >= h - R) { float dx = x - (w - R) +0.5f, dy = y - (h - R) +0.5f; float d = sqrtf(dx*dx+dy*dy); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                }
                unsigned long px = hasAlpha ? (a << 24) | (r << 16) | (g << 8) | b : (r << 16) | (g << 8) | b;
                if (!inside) px = hasAlpha ? 0 : 0;
                XPutPixel(img, x, y, px);
            }
        }
    } else if (ShaderPreset == 2) { // radial
        float cx = w * 0.5f, cy = h * 0.35f;
        float maxDist = sqrtf(cx*cx + cy*cy);
        if (maxDist < 1) maxDist = 1;
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                float dx = x - cx, dy = y - cy;
                float dist = sqrtf(dx*dx + dy*dy) / maxDist;
                if (dist > 1) dist = 1;
                float t = dist * 0.85f;
                unsigned long col = LerpHex(light, base, t);
                unsigned int r = (col >> 16) & 0xFF, g = (col >> 8) & 0xFF, b = col & 0xFF;
                bool inside = true;
                unsigned int a = baseAlpha8;
                if (rounded) {
                    if (x < R && y < R) { float dxc = R - x -0.5f, dyc = R - y -0.5f; float d = sqrtf(dxc*dxc+dyc*dyc); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                    else if (x >= w - R && y < R) { float dxc = x - (w - R) +0.5f, dyc = R - y -0.5f; float d = sqrtf(dxc*dxc+dyc*dyc); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                    else if (x < R && y >= h - R) { float dxc = R - x -0.5f, dyc = y - (h - R) +0.5f; float d = sqrtf(dxc*dxc+dyc*dyc); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                    else if (x >= w - R && y >= h - R) { float dxc = x - (w - R) +0.5f, dyc = y - (h - R) +0.5f; float d = sqrtf(dxc*dxc+dyc*dyc); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                }
                unsigned long px = hasAlpha ? (a << 24) | (r << 16) | (g << 8) | b : (r << 16) | (g << 8) | b;
                if (!inside) px = hasAlpha ? 0 : 0;
                XPutPixel(img, x, y, px);
            }
        }
    } else if (ShaderPreset == 4) { // noise
        for (int y = 0; y < h; y++) {
            float tBase = h > 1 ? (float)y / (float)(h - 1) : 0;
            for (int x = 0; x < w; x++) {
                float t = tBase * 0.9f;
                unsigned long col = LerpHex(base, light, t);
                unsigned int n = HashNoise(x, y);
                int delta = (int)(n & 0x1F) - 16;
                delta = (int)(delta * ShaderIntensity * 0.5f);
                unsigned int r0 = (col >> 16) & 0xFF, g0 = (col >> 8) & 0xFF, b0 = col & 0xFF;
                int nr = (int)r0 + delta, ng = (int)g0 + delta, nb = (int)b0 + delta;
                if (nr < 0) nr = 0; if (nr > 255) nr = 255;
                if (ng < 0) ng = 0; if (ng > 255) ng = 255;
                if (nb < 0) nb = 0; if (nb > 255) nb = 255;
                unsigned int r = (unsigned int)nr, g = (unsigned int)ng, b = (unsigned int)nb;
                bool inside = true;
                unsigned int a = baseAlpha8;
                if (rounded) {
                    if (x < R && y < R) { float dx = R - x -0.5f, dy = R - y -0.5f; float d = sqrtf(dx*dx+dy*dy); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                    else if (x >= w - R && y < R) { float dx = x - (w - R) +0.5f, dy = R - y -0.5f; float d = sqrtf(dx*dx+dy*dy); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                    else if (x < R && y >= h - R) { float dx = R - x -0.5f, dy = y - (h - R) +0.5f; float d = sqrtf(dx*dx+dy*dy); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                    else if (x >= w - R && y >= h - R) { float dx = x - (w - R) +0.5f, dy = y - (h - R) +0.5f; float d = sqrtf(dx*dx+dy*dy); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                }
                unsigned long px = hasAlpha ? (a << 24) | (r << 16) | (g << 8) | b : (r << 16) | (g << 8) | b;
                if (!inside) px = hasAlpha ? 0 : 0;
                XPutPixel(img, x, y, px);
            }
        }
    } else {
        unsigned int r = (base >> 16) & 0xFF, g = (base >> 8) & 0xFF, b = base & 0xFF;
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                bool inside = true;
                unsigned int a = baseAlpha8;
                if (rounded) {
                    if (x < R && y < R) { float dx = R - x -0.5f, dy = R - y -0.5f; float d = sqrtf(dx*dx+dy*dy); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                    else if (x >= w - R && y < R) { float dx = x - (w - R) +0.5f, dy = R - y -0.5f; float d = sqrtf(dx*dx+dy*dy); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                    else if (x < R && y >= h - R) { float dx = R - x -0.5f, dy = y - (h - R) +0.5f; float d = sqrtf(dx*dx+dy*dy); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                    else if (x >= w - R && y >= h - R) { float dx = x - (w - R) +0.5f, dy = y - (h - R) +0.5f; float d = sqrtf(dx*dx+dy*dy); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                }
                unsigned long px = hasAlpha ? (a << 24) | (r << 16) | (g << 8) | b : (r << 16) | (g << 8) | b;
                if (!inside) px = hasAlpha ? 0 : 0;
                XPutPixel(img, x, y, px);
            }
        }
    }
    GC gc = XCreateGC(dpy, pm, 0, NULL);
    XPutImage(dpy, pm, gc, img, 0, 0, 0, 0, w, h);
    XFreeGC(dpy, gc);
    XDestroyImage(img);
    return pm;
}

Pixmap Linux_GetCachedBackground(Display* dpy, Window win, int w, int h) {
    if (!dpy || w <= 0 || h <= 0) return None;
    unsigned long base = rgb_to_hex(COLOR_BG);
    int preset = ShaderPreset;
    float intensity = ShaderIntensity;
    int radius = CornerRadius;
    bool rounded = RoundedCorners;
    if (g_bgPixmap != None && g_bgDpy == dpy && g_bgW == w && g_bgH == h && g_bgBase == base && g_bgPreset == preset && g_bgIntensity == intensity && g_bgRadius == radius && g_bgRounded == rounded) {
        return g_bgPixmap;
    }
    if (g_bgPixmap != None) {
        XFreePixmap(g_bgDpy, g_bgPixmap);
        g_bgPixmap = None;
        g_bgW = g_bgH = 0;
    }
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(dpy, win, &attrs)) return None;
    unsigned int baseAlpha8 = (unsigned int)WindowOpacity;
    if (baseAlpha8 > 255) baseAlpha8 = 255;
    Pixmap pm = Linux_CreateBackgroundPixmap(dpy, win, w, h, &attrs, base, baseAlpha8);
    if (!pm) return None;
    g_bgPixmap = pm;
    g_bgW = w; g_bgH = h; g_bgBase = base; g_bgPreset = preset; g_bgIntensity = intensity; g_bgRadius = radius; g_bgRounded = rounded; g_bgDpy = dpy; g_bgDepth = attrs.depth;
    return g_bgPixmap;
}

void Linux_DrawBackgroundToDrawable(Display* dpy, Drawable drw, int w, int h) {
    if (!dpy || w <= 0 || h <= 0) return;
    // Prefer cached pixmap if valid for same w/h and settings
    unsigned long base = rgb_to_hex(COLOR_BG);
    int preset = ShaderPreset;
    float intensity = ShaderIntensity;
    int radius = CornerRadius;
    bool rounded = RoundedCorners;
    Pixmap cached = None;
    if (g_bgPixmap != None && g_bgDpy == dpy && g_bgW == w && g_bgH == h && g_bgBase == base && g_bgPreset == preset && g_bgIntensity == intensity && g_bgRadius == radius && g_bgRounded == rounded) {
        cached = g_bgPixmap;
    }
    if (cached != None) {
        GC gc = XCreateGC(dpy, drw, 0, NULL);
        XCopyArea(dpy, cached, drw, gc, 0, 0, w, h, 0, 0);
        XFreeGC(dpy, gc);
        return;
    }
    // Fallback: try to generate via DefaultVisual if no window available
    int screen = DefaultScreen(dpy);
    Visual* vis = DefaultVisual(dpy, screen);
    int depth = DefaultDepth(dpy, screen);
    bool hasAlpha = (depth == 32);
    bool rded = rounded && radius > 0 && hasAlpha;
    int R = rded ? radius : 0;
    if (R > w/2) R = w/2; if (R > h/2) R = h/2;
    unsigned int baseAlpha8 = (unsigned int)WindowOpacity;
    if (baseAlpha8 > 255) baseAlpha8 = 255;
    XImage* img = XCreateImage(dpy, vis, depth, ZPixmap, 0, NULL, w, h, 32, 0);
    if (!img) return;
    img->data = (char*)calloc(1, h * img->bytes_per_line);
    if (!img->data) { XDestroyImage(img); return; }
    unsigned long light = DeriveLighter(base, 1.0f);
    GC gc = XCreateGC(dpy, drw, 0, NULL);
    // Simplified fill: solid fallback if no window attrs available
    unsigned int r = (base >> 16) & 0xFF, g = (base >> 8) & 0xFF, b = base & 0xFF;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            unsigned long px = hasAlpha ? (baseAlpha8 << 24) | (r << 16) | (g << 8) | b : (r << 16) | (g << 8) | b;
            // handle rounded for fallback (simple)
            if (rded) {
                bool inside = true;
                unsigned int a = baseAlpha8;
                if (x < R && y < R) { float dx = R - x -0.5f, dy = R - y -0.5f; float d = sqrtf(dx*dx+dy*dy); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                else if (x >= w - R && y < R) { float dx = x - (w - R) +0.5f, dy = R - y -0.5f; float d = sqrtf(dx*dx+dy*dy); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                else if (x < R && y >= h - R) { float dx = R - x -0.5f, dy = y - (h - R) +0.5f; float d = sqrtf(dx*dx+dy*dy); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                else if (x >= w - R && y >= h - R) { float dx = x - (w - R) +0.5f, dy = y - (h - R) +0.5f; float d = sqrtf(dx*dx+dy*dy); if (d >= R+0.5f) inside=false; else if (d > R-0.5f) a = (unsigned int)(baseAlpha8 * (R+0.5f - d) + 0.5f); }
                if (!inside) px = hasAlpha ? 0 : 0;
                else if (a != baseAlpha8) px = hasAlpha ? (a << 24) | (r << 16) | (g << 8) | b : px;
            }
            XPutPixel(img, x, y, px);
        }
    }
    (void)light; // unused in fallback solid
    XPutImage(dpy, drw, gc, img, 0, 0, 0, 0, w, h);
    XFreeGC(dpy, gc);
    XDestroyImage(img);
}

void Linux_DrawBackground(Display* dpy, Window win, int w, int h) {
    WinTimer timer = TimerStart();
    const char* presetName = "solid";
    if (!EnableShaders || ShaderPreset == 0) presetName = "solid";
    else if (ShaderPreset == 1) presetName = "vertical";
    else if (ShaderPreset == 2) presetName = "radial";
    else if (ShaderPreset == 3) presetName = "diagonal";
    else if (ShaderPreset == 4) presetName = "noise";
    else presetName = "solid-fallback";
    Pixmap bg = Linux_GetCachedBackground(dpy, win, w, h);
    if (bg) {
        GC gc = XCreateGC(dpy, win, 0, NULL);
        XCopyArea(dpy, bg, win, gc, 0, 0, w, h, 0, 0);
        XFreeGC(dpy, gc);
    }
    TimerEnd(timer, presetName);
}

Window Linux_CreatePromptWindow(Display* dpy, int x, int y, int w, int h) {
    unsigned long bg = rgb_to_hex(COLOR_BG);
    return Linux_CreateWindowEx(dpy, x, y, w, h, bg, "colored", "colored", KeyPressMask | ExposureMask | FocusChangeMask);
}
Window Linux_CreateSimplePromptWindow(Display* dpy, int x, int y, int w, int h) {
    unsigned long bg = rgb_to_hex(COLOR_BG);
    return Linux_CreateWindowEx(dpy, x, y, w, h, bg, "colored", "colored", KeyPressMask | ExposureMask);
}
Window Linux_CreateEditPopupWindow(Display* dpy, int x, int y, int w, int h) {
    unsigned long bg = rgb_to_hex(COLOR_BG);
    return Linux_CreateWindowEx(dpy, x, y, w, h, bg, "colored", "colored", KeyPressMask | ExposureMask | FocusChangeMask);
}
Window Linux_CreateDisplayMessageWindow(Display* dpy, int x, int y, int w, int h) {
    unsigned long bg = rgb_to_hex(COLOR_BG);
    return Linux_CreateWindowExOR(dpy, x, y, w, h, bg, "flashtext", "flashtext", ExposureMask, true);
}

XftDraw* Linux_CreateXftDraw(Display* dpy, Window win) {
    Visual* vis = NULL; Colormap cmap = 0; int depth = 0;
    if (Linux_GetDrawableVisual(dpy, win, &vis, &cmap, &depth)) {
        return XftDrawCreate(dpy, win, vis, cmap);
    }
    int screen = DefaultScreen(dpy);
    return XftDrawCreate(dpy, win, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen));
}

#endif
