#include "core/context.h"
#include "deps/log.h"
#include "core/modal.h"
#include "config.h"
#include <stdlib.h>
#include <math.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/shape.h>
#include <unistd.h>
#include <png.h>
#include <stdint.h>
#include "config.h"
#include "platform/platform.h"
#include "platform/draw.h"
#include "helpers/helpers_ui.h"
#include "platform/x11/audio_x11.h"
#include "core/timer.h"
#include "wm/niri_wm.h"
#define NANOSVG_IMPLEMENTATION
#include "deps/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "deps/nanosvgrast.h"

#define X11_SHADER_SMOOTHNESS 10

static Pixmap g_bgPixmap = None;
static int g_bgW = 0, g_bgH = 0;
static unsigned long g_bgBase = 0;
static int g_bgPreset = -1;
static float g_bgIntensity = -1.0f;
static float g_bgSmoothness = -1.0f;
static int g_bgRadius = -1;
static bool g_bgRounded = false;
static Display* g_bgDpy = NULL;
static Visual* g_dblVisual = NULL;
static Colormap g_dblColormap = None;
static int g_dblDepth = 0;
static Display* g_dblDpy = NULL;
static int g_bgDepth = 0;

void x11_set_dbl_visual(Display* dpy, Visual* vis, Colormap cmap, int depth) {
    PROFILE_START(timer);
    g_dblVisual = vis;
    g_dblColormap = cmap;
    g_dblDepth = depth;
    g_dblDpy = dpy;
    PROFILE_END_VAR(timer, "helpers_x11:x11_set_dbl_visual");
}
void x11_clear_dbl_visual(void) {
    PROFILE_START(timer);
    g_dblVisual = NULL;
    g_dblColormap = None;
    g_dblDepth = 0;
    g_dblDpy = NULL;
    PROFILE_END_VAR(timer, "helpers_x11:x11_clear_dbl_visual");
}

bool x11_is_own_window_name(const char* name) {
    if (!name) return false;
    return strncmp(name, "flint-", 6) == 0;
}
bool x11_get_drawable_visual(Display* dpy, Drawable d, Visual** visOut, Colormap* cmapOut, int* depthOut) {
    PROFILE_START(timer);
    XWindowAttributes attrs;
    if (XGetWindowAttributes(dpy, d, &attrs)) {
        if (visOut) *visOut = attrs.visual;
        if (cmapOut) *cmapOut = attrs.colormap;
        if (depthOut) *depthOut = attrs.depth;
        PROFILE_END_VAR(timer, "helpers_x11:x11_get_drawable_visual");
        return true;
    }
    if (g_dblVisual && g_dblDpy == dpy) {
        if (visOut) *visOut = g_dblVisual;
        if (cmapOut) *cmapOut = g_dblColormap;
        if (depthOut) *depthOut = g_dblDepth;
        PROFILE_END_VAR(timer, "helpers_x11:x11_get_drawable_visual");
        return true;
    }
    PROFILE_END_VAR(timer, "helpers_x11:x11_get_drawable_visual");
    return false;
}

static int xft_width(Display* dpy, XftFont* xf, const char* text, int len) {
    PROFILE_START(timer);
    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy, xf, (FcChar8*)text, len, &ext);
    PROFILE_END_VAR(timer, "helpers_x11:xft_width");
    return ext.xOff;
}

void draw_wrapped_text(Display* dpy, XftDraw* xd, XftColor* color, FontHandle font, const char* text, int startY, char* centered_to, int xOffset) {
    PROFILE_START(timer);
    if (!text) { PROFILE_END_VAR(timer, "helpers_x11:draw_wrapped_text"); return; }
    XftFont* xf = FH_TO_XFT(font);
    if (!xf) { PROFILE_END_VAR(timer, "helpers_x11:draw_wrapped_text"); return; }
    int lineHeight = xf->ascent + xf->descent;
    int spaceWidth = xft_width(dpy, xf, " ", 1);
    int y = startY + xf->ascent;
    int text_len = strlen(text);
    int maxWidth = 0;   
    int alignment = 0;

    if (centered_to != NULL) {
        if (Platform_StringContainsCaseInsensitive(centered_to, "CENTER")) {
            alignment = 1;
            maxWidth = 870;
        } else if (Platform_StringContainsCaseInsensitive(centered_to, "RIGHT")) {
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
    PROFILE_END_VAR(timer, "helpers_x11:draw_wrapped_text");
}

void x11_xft_alloc_color(Display* dpy, Visual* vis, Colormap cmap, unsigned long hex, unsigned short alpha, XftColor* out) {
    PROFILE_START(timer);
    XRenderColor rc = {
        ((hex >> 16) & 0xFF) * 0x101,
        ((hex >> 8) & 0xFF) * 0x101,
        (hex & 0xFF) * 0x101,
        alpha
    };
    XftColorAllocValue(dpy, vis, cmap, &rc, out);
    PROFILE_END_VAR(timer, "helpers_x11:x11_xft_alloc_color");
}

unsigned long VimModeTextColor(void) {
    PROFILE_START(timer);
    if (g_current_mode == MODE_NORMAL) { PROFILE_END_VAR(timer, "helpers_x11:VimModeTextColor"); return rgb_to_hex(COLOR_NORMAL_MODE_TEXT); }
    if (g_current_mode == MODE_INSERT) { PROFILE_END_VAR(timer, "helpers_x11:VimModeTextColor"); return rgb_to_hex(COLOR_INSERT_MODE_TEXT); }
    PROFILE_END_VAR(timer, "helpers_x11:VimModeTextColor");
    return rgb_to_hex(COLOR_VISUAL_MODE_TEXT);
}

int x11_update_edit_scroll_offset(Display* dpy, XftFont* xf, const char* text, int text_len, int cursor, int width, int prev_scroll, int* out_cursor_x) {
    PROFILE_START(timer);
    int visible_w = width - 28;
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
    int margin = 0;
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
    PROFILE_END_VAR(timer, "helpers_x11:x11_update_edit_scroll_offset");
    return scroll;
}

int x11_font_row_height(int font_size) {
    return font_size + 14;
}

int x11_font_list_top(int ascent, int descent, int font_size) {
    return font_size + 38;
}

int x11_font_cursor_y(int font_size, int box_top) {
    int lineH = font_size;
    int y = box_top + (lineH - font_size) / 2;
    if (y < box_top) y = box_top;
    return y;
}

int x11_font_row_height_of(FontHandle font) {
    XftFont* xf = FH_TO_XFT(font);
    return x11_font_row_height(FontSize);
}

int x11_font_list_top_of(FontHandle font) {
    XftFont* xf = FH_TO_XFT(font);
    return x11_font_list_top(xf ? xf->ascent : 0, xf ? xf->descent : 0, FontSize);
}

int x11_font_cursor_y_of(FontHandle font, int box_top) {
    return x11_font_cursor_y(FontSize, box_top);
}

void x11_draw_edit_cursor(XftDraw* xd, XftColor* color, int cursor_x, int scroll, int width) {
    PROFILE_START(timer);
    int cursor_draw_x = 14 + cursor_x - scroll;
    if (cursor_draw_x > width - 10) cursor_draw_x = width - 10;
    int cursor_y = 20;
    if (IsInsertMode()) {
        XftDrawRect(xd, color, cursor_draw_x, cursor_y, 2, FontSize);
    } else {
        XftDrawRect(xd, color, cursor_draw_x, cursor_y, 12, FontSize);
    }
    PROFILE_END_VAR(timer, "helpers_x11:x11_draw_edit_cursor");
}

const char* ListBoxItemDisplayText(ListBoxInfo* lb, int idx, char* trunc, size_t trunc_size) {
    PROFILE_START(timer);
    int raw = (idx >= 0 && idx < lb->populated_count) ? lb->matching_indices[idx] : -1;
    const char* baseText = lb->populated_cache[idx];
    if (lb && lb->alias_suffix_active && raw >= 0 && raw < lb->alias_suffix_count && lb->alias_suffix[raw]) {
        snprintf(trunc, trunc_size, "%s [%s]", baseText ? baseText : "", lb->alias_suffix[raw]);
    } else {
        snprintf(trunc, trunc_size, "%.80s", baseText ? baseText : "");
    }
    PROFILE_END_VAR(timer, "helpers_x11:ListBoxItemDisplayText");
    return trunc;
}

static void draw_icon_png(Display* dpy, XftDraw* xd, const char* path,
                          int x, int y,int size)
{
    if (!path || !path[0]) return;
    if (access(path, F_OK) != 0) return;
    (void)size;
    size_t _plen = strlen(path);
    bool _is_svg = _plen > 4 && Platform_StringContainsCaseInsensitive(path + _plen - 4, ".svg");
    if (_is_svg) {
        NSVGimage* image = nsvgParseFromFile(path, "px", 96.0f);
        if (!image) return;
        if (image->width <= 0 || image->height <= 0) { nsvgDelete(image); return; }
        NSVGrasterizer* rast = nsvgCreateRasterizer();
        if (!rast) { nsvgDelete(image); return; }
        unsigned char* svg_data = (unsigned char*)calloc((size_t)IconSize * IconSize * 4, 1);
        if (!svg_data) { nsvgDeleteRasterizer(rast); nsvgDelete(image); return; }
        float scale = fminf((float)IconSize / image->width, (float)IconSize / image->height);
        float tx = ((float)IconSize - image->width * scale) * 0.5f;
        float ty = ((float)IconSize - image->height * scale) * 0.5f;
        nsvgRasterize(rast, image, tx, ty, scale, svg_data, IconSize, IconSize, IconSize * 4);
        unsigned char* argb = (unsigned char*)malloc((size_t)IconSize * IconSize * 4);
        if (!argb) { free(svg_data); nsvgDeleteRasterizer(rast); nsvgDelete(image); return; }
        for (int i = 0; i < IconSize * IconSize; i++) {
            unsigned char r = svg_data[i*4+0];
            unsigned char g = svg_data[i*4+1];
            unsigned char b = svg_data[i*4+2];
            unsigned char a = svg_data[i*4+3];
            unsigned int pr = (unsigned int)r * a / 255;
            unsigned int pg = (unsigned int)g * a / 255;
            unsigned int pb = (unsigned int)b * a / 255;
            argb[i*4+0] = (unsigned char)pb;
            argb[i*4+1] = (unsigned char)pg;
            argb[i*4+2] = (unsigned char)pr;
            argb[i*4+3] = a;
        }
        free(svg_data);
        nsvgDeleteRasterizer(rast);
        nsvgDelete(image);
        Drawable drw = XftDrawDrawable(xd);
        Screen* scr = DefaultScreenOfDisplay(dpy);
        Visual* vis = DefaultVisualOfScreen(scr);
        XImage* img = XCreateImage(dpy, vis, 32, ZPixmap, 0, (char*)argb, IconSize, IconSize, 32, 0);
        if (!img) { free(argb); return; }
        Pixmap pix = XCreatePixmap(dpy, drw, IconSize, IconSize, 32);
        if (!pix) { XDestroyImage(img); return; }
        GC gc = XCreateGC(dpy, pix, 0, NULL);
        XPutImage(dpy, pix, gc, img, 0, 0, 0, 0, IconSize, IconSize);
        XFreeGC(dpy, gc);
        XRenderPictFormat* src_fmt = XRenderFindStandardFormat(dpy, PictStandardARGB32);
        XRenderPictFormat* dst_fmt = XRenderFindVisualFormat(dpy, vis);
        if (src_fmt && dst_fmt) {
            Picture src = XRenderCreatePicture(dpy, pix, src_fmt, 0, NULL);
            Picture dst = XRenderCreatePicture(dpy, drw, dst_fmt, 0, NULL);
            if (src && dst) {
                XRenderComposite(dpy, PictOpOver, src, None, dst, 0, 0, 0, 0, x, y, IconSize, IconSize);
            }
            if (src) XRenderFreePicture(dpy, src);
            if (dst) XRenderFreePicture(dpy, dst);
        }
        XDestroyImage(img);
        XFreePixmap(dpy, pix);
        return;
    }

    FILE* fp = fopen(path, "rb");
    if (!fp) return;

    unsigned char sig[8];
    if (fread(sig, 1, 8, fp) != 8 || png_sig_cmp(sig, 0, 8) != 0) {
        fclose(fp);
        return;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) { fclose(fp); return; }

    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_read_struct(&png, NULL, NULL); fclose(fp); return; }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return;
    }

    png_init_io(png, fp);
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);

    int w = png_get_image_width(png, info);
    int h = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth  = png_get_bit_depth(png, info);

    if (bit_depth == 16) png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_add_alpha(png, 0xFF, PNG_FILLER_AFTER);
    if (color_type == PNG_COLOR_TYPE_GRAY) png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    int row_bytes = png_get_rowbytes(png, info);
    unsigned char* data = (unsigned char*)malloc((size_t)row_bytes * h);
    if (!data) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return;
    }

    png_bytep* rows = (png_bytep*)malloc(sizeof(png_bytep) * h);
    if (!rows) {
        free(data);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return;
    }
    for (int i = 0; i < h; i++) rows[i] = data + (size_t)i * row_bytes;

    png_read_image(png, rows);
    png_read_end(png, NULL);
    fclose(fp);
    png_destroy_read_struct(&png, &info, NULL);
    free(rows);

    unsigned char* argb = (unsigned char*)malloc((size_t)IconSize * IconSize * 4);
    if (!argb) { free(data); return; }

    for (int dy = 0; dy < IconSize; dy++) {
        float y0f = (float)dy       * h / IconSize;
        float y1f = (float)(dy + 1) * h / IconSize;

        int y0 = (int)y0f;
        int y1 = (int)(y1f + 0.5f);
        if (y1 > h) y1 = h;
        if (y1 <= y0) y1 = y0 + 1;
        if (y0 >= h) y0 = h - 1;

        unsigned char* dst_row = argb + (size_t)dy * IconSize * 4;

        for (int dx = 0; dx < IconSize; dx++) {
            float x0f = (float)dx       * w / IconSize;
            float x1f = (float)(dx + 1) * w / IconSize;

            int x0 = (int)x0f;
            int x1 = (int)(x1f + 0.5f);
            if (x1 > w) x1 = w;
            if (x1 <= x0) x1 = x0 + 1;
            if (x0 >= w) x0 = w - 1;

            unsigned int sum_r = 0, sum_g = 0, sum_b = 0, sum_a = 0;
            unsigned int count = 0;

            for (int sy = y0; sy < y1; sy++) {
                const unsigned char* src_row = data + (size_t)sy * row_bytes;
                for (int sx = x0; sx < x1; sx++) {
                    const unsigned char* s = src_row + (size_t)sx * 4;
                    sum_r += s[0];
                    sum_g += s[1];
                    sum_b += s[2];
                    sum_a += s[3];
                    count++;
                }
            }

            unsigned int r = sum_r / count;
            unsigned int g = sum_g / count;
            unsigned int b = sum_b / count;
            unsigned int a = sum_a / count;

            unsigned int pr = r * a / 255;
            unsigned int pg = g * a / 255;
            unsigned int pb = b * a / 255;

            unsigned char* d = dst_row + (size_t)dx * 4;
            d[0] = (unsigned char)pb;
            d[1] = (unsigned char)pg;
            d[2] = (unsigned char)pr;
            d[3] = (unsigned char)a;
        }
    }
    free(data);

    Drawable drw = XftDrawDrawable(xd);
    Screen* scr  = DefaultScreenOfDisplay(dpy);
    Visual* vis  = DefaultVisualOfScreen(scr);

    XImage* img = XCreateImage(dpy, vis, 32, ZPixmap, 0, (char*)argb,
                               IconSize, IconSize, 32, 0);
    if (!img) { free(argb); return; }

    Pixmap pix = XCreatePixmap(dpy, drw, IconSize, IconSize, 32);
    if (!pix) { XDestroyImage(img); return; }

    GC gc = XCreateGC(dpy, pix, 0, NULL);
    XPutImage(dpy, pix, gc, img, 0, 0, 0, 0, IconSize, IconSize);
    XFreeGC(dpy, gc);

    XRenderPictFormat* src_fmt = XRenderFindStandardFormat(dpy, PictStandardARGB32);
    XRenderPictFormat* dst_fmt = XRenderFindVisualFormat(dpy, vis);

    if (src_fmt && dst_fmt) {
        Picture src = XRenderCreatePicture(dpy, pix, src_fmt, 0, NULL);
        Picture dst = XRenderCreatePicture(dpy, drw, dst_fmt, 0, NULL);
        if (src && dst) {
            XRenderComposite(dpy, PictOpOver, src, None, dst,
                             0, 0, 0, 0, x, y, IconSize, IconSize);
        }
        if (src) XRenderFreePicture(dpy, src);
        if (dst) XRenderFreePicture(dpy, dst);
    }

    XDestroyImage(img);   /* frees argb via img->data */
    XFreePixmap(dpy, pix);
}

void DrawListBoxItemText(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, FontHandle font,
                         XftColor* num_color, XftColor* body_color, XftColor* plain_color,
                         ListBoxInfo* lb,char* icon_path,int idx, int y, int text_x) {
    PROFILE_START(timer);
    XftFont* xf = FH_TO_XFT(font);
    if (!xf) { PROFILE_END_VAR(timer, "helpers_x11:DrawListBoxItemText"); return; }
    int XOffset = 10;

    if (icon_path) {
        int iconX = XOffset;
        int iconY = y + (x11_font_row_height_of(font) - IconSize)/2;
        XOffset += IconSize + XOffset;
        draw_icon_png(dpy, xd, icon_path, iconX, iconY, IconSize);
    }

    char trunc[512];
    ListBoxItemDisplayText(lb, idx, trunc, sizeof(trunc));

    char* alias_text = strstr(trunc, " [");

    if (alias_text) {
        int XOffsetAlias = 0;
        eval_xoffset_for_alias(alias_text,strlen(trunc),&XOffsetAlias);
        Platform_SetDrawContext(dpy, xd, vis, cmap);
        Platform_DrawTextRaw(XOffsetAlias, y, alias_text, rgb_to_hex(COLOR_ALIAS_TEXT), font, "LEFT", XOffsetAlias, FontSize);
    }

    char* pos = strstr(trunc, " [");
    if (pos) {
      *pos = 0;
    }
    unsigned long pcol=0; Platform_GetItemColors(idx, lb->populated_count, NULL, NULL, &pcol);
    Platform_SetDrawContext(dpy, xd, vis, cmap);
    Platform_DrawTextRaw(XOffset, y, trunc, pcol, font, "LEFT", XOffset, FontSize);

    PROFILE_END_VAR(timer, "helpers_x11:DrawListBoxItemText");
}

bool CursorBack(size_t* cursor, size_t text_len) {
    PROFILE_START(timer);
    if (*cursor == 0) { PROFILE_END_VAR(timer, "helpers_x11:CursorBack"); return false;  }
    (*cursor)--;
    PROFILE_END_VAR(timer, "helpers_x11:CursorBack");
    return true;
}

bool CursorForward(size_t* cursor, size_t text_len) {
    PROFILE_START(timer);
    if (*cursor >= text_len) { PROFILE_END_VAR(timer, "helpers_x11:CursorForward"); return false; }
    (*cursor)++;
    PROFILE_END_VAR(timer, "helpers_x11:CursorForward");
    return true;
}

static int x11_error_occurred = 0;

static int dummy_x_error_handler(Display *dpy, XErrorEvent *error) {
    PROFILE_START(timer);
    if (error->error_code == BadWindow) {
        x11_error_occurred = 1;
        PROFILE_END_VAR(timer, "helpers_x11:dummy_x_error_handler");
        return 0;
    }
    PROFILE_END_VAR(timer, "helpers_x11:dummy_x_error_handler");
    return 0;
}

pid_t GetWindowPid(Display* dpy, Window w) {
    PROFILE_START(timer);
    if (w == 0) { PROFILE_END_VAR(timer, "helpers_x11:GetWindowPid"); return -1; }

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
        PROFILE_END_VAR(timer, "helpers_x11:GetWindowPid");
        return -1;
    }

    PROFILE_END_VAR(timer, "helpers_x11:GetWindowPid");
    return pid;
}

void GetExeNameFromPid(pid_t pid, char* outExe, size_t maxLen) {
    PROFILE_START(timer);
    if (pid <= 0) {
        strncpy(outExe, "unknown", maxLen);
        PROFILE_END_VAR(timer, "helpers_x11:GetExeNameFromPid");
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
    PROFILE_END_VAR(timer, "helpers_x11:GetExeNameFromPid");
}

Window x11_create_window_ex_or(Display* dpy, int x, int y, int w, int h, unsigned long bg_color, const char* res_name, const char* res_class, long event_mask, bool override_redirect);

Window x11_create_window_ex(Display* dpy, int x, int y, int w, int h, unsigned long bg_color, const char* res_name, const char* res_class, long event_mask) {
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "helpers_x11:x11_create_window_ex");
    return x11_create_window_ex_or(dpy, x, y, w, h, bg_color, res_name, res_class, event_mask, false);
}

static void x11_apply_shape_rounded(Display* dpy, Window win, int w, int h, int radius);

static bool x11_has_compositor(Display* dpy, int screen) {
    char sel[32];
    snprintf(sel, sizeof(sel), "_NET_WM_CM_S%d", screen);
    Atom a = XInternAtom(dpy, sel, False);
    return XGetSelectionOwner(dpy, a) != None;
}

Window x11_create_window_ex_or(Display* dpy, int x, int y, int w, int h, unsigned long bg_color, const char* res_name, const char* res_class, long event_mask, bool override_redirect) {
    PROFILE_START(timer);
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
    if (use_argb) {
        XSetWindowAttributes attrs = {0};
        unsigned int r = (bg_color >> 16) & 0xFF;
        unsigned int g = (bg_color >> 8) & 0xFF;
        unsigned int b = bg_color & 0xFF;
        unsigned int a = RoundedCorners ? 0 : (unsigned int)WindowOpacity;
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
        Atom motif_atom = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
        if (motif_atom != None) {
            struct { unsigned long flags; unsigned long functions; unsigned long decorations; long input_mode; unsigned long status; } motif;
            motif.flags = 2;
            motif.functions = 0;
            motif.decorations = 0;
            motif.input_mode = 0;
            motif.status = 0;
            XChangeProperty(dpy, win, motif_atom, motif_atom, 32, PropModeReplace, (unsigned char*)&motif, 5);
        }
        Atom wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
        Atom state_skip_taskbar = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
        Atom state_skip_pager = XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER", False);
        Atom states[2] = {state_skip_taskbar, state_skip_pager};
        XChangeProperty(dpy, win, wm_state, XA_ATOM, 32, PropModeReplace, (unsigned char*)states, 2);
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
    if (override_redirect && niri_is_active()) {
        XSizeHints* sh = XAllocSizeHints();
        sh->flags = PMinSize | PMaxSize;
        sh->min_width = w;
        sh->min_height = h;
        sh->max_width = w;
        sh->max_height = h;
        XSetWMNormalHints(dpy, win, sh);
        XFree(sh);
    }
    XSelectInput(dpy, win, event_mask);
    if (RoundedCorners && (!use_argb || !x11_has_compositor(dpy, screen))) {
        x11_apply_shape_rounded(dpy, win, w, h, CornerRadius);
    }
    PROFILE_END_VAR(timer, "helpers_x11:x11_create_window_ex_or");
    return win;
}

static void x11_apply_shape_rounded(Display* dpy, Window win, int w, int h, int radius) {
    PROFILE_START(timer);
    if (radius <= 0 || w <= 0 || h <= 0) { PROFILE_END_VAR(timer, "helpers_x11:x11_apply_shape_rounded"); return; }
    Pixmap mask = XCreatePixmap(dpy, win, w, h, 1);
    GC gc = XCreateGC(dpy, mask, 0, NULL);
    XSetForeground(dpy, gc, 0);
    XFillRectangle(dpy, mask, gc, 0, 0, w, h);
    XSetForeground(dpy, gc, 1);

    XFillRectangle(dpy, mask, gc, radius, 0, w - 2*radius, h);
    XFillRectangle(dpy, mask, gc, 0, radius, w, h - 2*radius);

    XFillArc(dpy, mask, gc, 0, 0, radius*2, radius*2, 0, 360*64);
    XFillArc(dpy, mask, gc, w - radius*2 -1, 0, radius*2, radius*2, 0, 360*64);
    XFillArc(dpy, mask, gc, 0, h - radius*2 -1, radius*2, radius*2, 0, 360*64);
    XFillArc(dpy, mask, gc, w - radius*2 -1, h - radius*2 -1, radius*2, radius*2, 0, 360*64);
    XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, mask, ShapeSet);
    XFreePixmap(dpy, mask);
    XFreeGC(dpy, gc);
    PROFILE_END_VAR(timer, "helpers_x11:x11_apply_shape_rounded");
}

void x11_fill_rounded_rect(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, unsigned long hex, unsigned short alpha16, int x, int y, int w, int h, int radius) {
    PROFILE_START(timer);
    if (!RoundedCorners || radius <= 0 || w <= 0 || h <= 0) {
        XftColor c;
        x11_xft_alloc_color(dpy, vis, cmap, hex, alpha16, &c);
        XftDrawRect(xd, &c, x, y, w, h);
        XftColorFree(dpy, vis, cmap, &c);
        PROFILE_END_VAR(timer, "helpers_x11:x11_fill_rounded_rect");
        return;
    }
    if (radius > w/2) radius = w/2;
    if (radius > h/2) radius = h/2;
    XftColor solid;
    x11_xft_alloc_color(dpy, vis, cmap, hex, alpha16, &solid);

    XftDrawRect(xd, &solid, x + radius, y, w - 2*radius, h);
    XftDrawRect(xd, &solid, x, y + radius, radius, h - 2*radius);
    XftDrawRect(xd, &solid, x + w - radius, y + radius, radius, h - 2*radius);

    for (int corner = 0; corner < 4; corner++) {
        int cx = (corner % 2 == 0) ? x + radius : x + w - radius - 1;
        int cy = (corner < 2) ? y + radius : y + h - radius - 1;
        int ox0 = (corner % 2 == 0) ? x : x + w - radius;
        int oy0 = (corner < 2) ? y : y + h - radius;
        for (int dy = 0; dy < radius; dy++) {
            for (int dx = 0; dx < radius; dx++) {
                float px = ox0 + dx + 0.5f;
                float py = oy0 + dy + 0.5f;
                float ddx2 = px - (cx + 0.5f);
                float ddy2 = py - (cy + 0.5f);
                float dist = sqrtf(ddx2*ddx2 + ddy2*ddy2);
                if (dist >= radius) continue;
                XftDrawRect(xd, &solid, ox0 + dx, oy0 + dy, 1, 1);
            }
        }
    }
    XftColorFree(dpy, vis, cmap, &solid);
    PROFILE_END_VAR(timer, "helpers_x11:x11_fill_rounded_rect");
}
static unsigned long LerpHex(unsigned long a, unsigned long b, float t) {
    return Color_Lerp(a, b, t);
}
static unsigned long DeriveLighter(unsigned long base, float intensity) {
    unsigned char r, g, b;
    Color_DeriveLighter((unsigned int)base, intensity, &r, &g, &b);
    return ((unsigned long)r << 16) | ((unsigned long)g << 8) | (unsigned long)b;
}
static unsigned int HashNoise(int x, int y) {
    return Color_HashNoise(x, y);
}
static unsigned int CornerAlpha(int x, int y, int w, int h, int R, unsigned int baseAlpha8) {
    float dx, dy, d_sq, R_plus_sq, R_minus_sq;
    if (x < R && y < R) { dx = R - x - 0.5f; dy = R - y - 0.5f; }
    else if (x >= w - R && y < R) { dx = x - (w - R) + 0.5f; dy = R - y - 0.5f; }
    else if (x < R && y >= h - R) { dx = R - x - 0.5f; dy = y - (h - R) + 0.5f; }
    else if (x >= w - R && y >= h - R) { dx = x - (w - R) + 0.5f; dy = y - (h - R) + 0.5f; }
    else return baseAlpha8;
    d_sq = dx*dx + dy*dy;
    R_plus_sq = (R + 0.5f) * (R + 0.5f);
    R_minus_sq = (R - 0.5f) * (R - 0.5f);
    if (d_sq >= R_plus_sq) return 0;
    if (d_sq > R_minus_sq) {
        float d = sqrtf(d_sq);
        return (unsigned int)(baseAlpha8 * (R + 0.5f - d) + 0.5f);
    }
    return baseAlpha8;
}

void x11_free_cached_background(void) {
    PROFILE_START(timer);
    if (g_bgPixmap != None && g_bgDpy) {
        XFreePixmap(g_bgDpy, g_bgPixmap);
        g_bgPixmap = None;
    }
    g_bgW = g_bgH = 0;
    g_bgBase = 0;
    g_bgPreset = -1;
    g_bgIntensity = -1.0f;
    g_bgSmoothness = -1.0f;
    g_bgRadius = -1;
    g_bgRounded = false;
    g_bgDpy = NULL;
    g_bgDepth = 0;
    PROFILE_END_VAR(timer, "helpers_x11:x11_free_cached_background");
}

static void x11_generate_background_pixels(uint32_t* img_data, int bytes_per_line, int w, int h, int R, unsigned int baseAlpha8, unsigned long base, unsigned long fill) {
    uint32_t *row;
    unsigned int r = (base >> 16) & 0xFF, g = (base >> 8) & 0xFF, b = base & 0xFF;
    float _gamma = Color_ShaderGamma(X11_SHADER_SMOOTHNESS);
    #define _DITHER(x,y) (Color_BayerDither((x),(y)))
    #define _APPLY_DITHER(cr,cg,cb,xx,yy) do { float _d = _DITHER(xx,yy); int _nr = (int)(cr + _d); int _ng = (int)(cg + _d); int _nb = (int)(cb + _d); if (_nr < 0) _nr = 0; if (_nr > 255) _nr = 255; if (_ng < 0) _ng = 0; if (_ng > 255) _ng = 255; if (_nb < 0) _nb = 0; if (_nb > 255) _nb = 255; cr = (unsigned int)_nr; cg = (unsigned int)_ng; cb = (unsigned int)_nb; } while(0)

    if (!EnableShaders || ShaderPreset == 0) {
        if (R <= 0) {
            row = (uint32_t*)img_data;
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) row[x] = fill;
                row = (uint32_t*)((char*)row + bytes_per_line);
            }
        } else {
            for (int y = 0; y < h; y++) {
                row = (uint32_t*)((char*)img_data + y * bytes_per_line);
                for (int x = 0; x < w; x++) {
                    unsigned int a = CornerAlpha(x, y, w, h, R, baseAlpha8);
                    row[x] = a ? (a << 24) | (r << 16) | (g << 8) | b : 0;
                }
            }
        }
    } else if (ShaderPreset == 1) {
        unsigned long light = DeriveLighter(base, 1.0f);
        for (int y = 0; y < h; y++) {
            float t = h > 1 ? (float)y / (float)(h - 1) : 0;
            t = powf(t, _gamma);
            unsigned long col = LerpHex(base, light, t * 0.9f);
            unsigned int cr0 = (col >> 16) & 0xFF, cg0 = (col >> 8) & 0xFF, cb0 = col & 0xFF;
            row = (uint32_t*)((char*)img_data + y * bytes_per_line);
            for (int x = 0; x < w; x++) {
                unsigned int cr = cr0, cg = cg0, cb = cb0;
                _APPLY_DITHER(cr,cg,cb,x,y);
                unsigned int a = CornerAlpha(x, y, w, h, R, baseAlpha8);
                row[x] = a ? (a << 24) | (cr << 16) | (cg << 8) | cb : 0;
            }
        }
    } else if (ShaderPreset == 2) {
        unsigned long light = DeriveLighter(base, 1.0f);
        float denom = (float)(w + h); if (denom < 1) denom = 1;
        for (int y = 0; y < h; y++) {
            row = (uint32_t*)((char*)img_data + y * bytes_per_line);
            for (int x = 0; x < w; x++) {
                float t = (float)(x + y) / denom;
                t = powf(t, _gamma);
                unsigned long col = LerpHex(base, light, t * 0.9f);
                unsigned int cr = (col >> 16) & 0xFF, cg = (col >> 8) & 0xFF, cb = col & 0xFF;
                _APPLY_DITHER(cr,cg,cb,x,y);
                unsigned int a = CornerAlpha(x, y, w, h, R, baseAlpha8);
                row[x] = a ? (a << 24) | (cr << 16) | (cg << 8) | cb : 0;
            }
        }
    } else if (ShaderPreset == 3) {
        unsigned long light = DeriveLighter(base, 1.0f);
        float cx = w * 0.5f, cy = h * 0.35f;
        float maxDist = sqrtf(cx*cx + cy*cy); if (maxDist < 1) maxDist = 1;
        for (int y = 0; y < h; y++) {
            row = (uint32_t*)((char*)img_data + y * bytes_per_line);
            for (int x = 0; x < w; x++) {
                float dx = x - cx, dy = y - cy;
                float t = sqrtf(dx*dx + dy*dy) / maxDist; if (t > 1) t = 1;
                t = powf(t, _gamma);
                unsigned long col = LerpHex(light, base, t * 0.85f);
                unsigned int cr = (col >> 16) & 0xFF, cg = (col >> 8) & 0xFF, cb = col & 0xFF;
                _APPLY_DITHER(cr,cg,cb,x,y);
                unsigned int a = CornerAlpha(x, y, w, h, R, baseAlpha8);
                row[x] = a ? (a << 24) | (cr << 16) | (cg << 8) | cb : 0;
            }
        }
    } else if (ShaderPreset == 4) {
        unsigned long light = DeriveLighter(base, 1.0f);
        for (int y = 0; y < h; y++) {
            float tBase = h > 1 ? (float)y / (float)(h - 1) : 0;
            tBase = powf(tBase, _gamma);
            row = (uint32_t*)((char*)img_data + y * bytes_per_line);
            for (int x = 0; x < w; x++) {
                float t = tBase * 0.9f;
                unsigned long col = LerpHex(base, light, t);
                unsigned int n = HashNoise(x, y);
                int delta = (int)(n & 0x1F) - 16;
                delta = (int)(delta * ShaderIntensity * 0.5f);
                int nr = (int)((col >> 16) & 0xFF) + delta;
                int ng = (int)((col >> 8) & 0xFF) + delta;
                int nb = (int)(col & 0xFF) + delta;
                float d = _DITHER(x,y);
                nr = (int)(nr + d); ng = (int)(ng + d); nb = (int)(nb + d);
                if (nr < 0) nr = 0; if (nr > 255) nr = 255;
                if (ng < 0) ng = 0; if (ng > 255) ng = 255;
                if (nb < 0) nb = 0; if (nb > 255) nb = 255;
                unsigned int a = CornerAlpha(x, y, w, h, R, baseAlpha8);
                row[x] = a ? ((a << 24) | ((unsigned int)nr << 16) | ((unsigned int)ng << 8) | (unsigned int)nb) : 0;
            }
        }
    } else {
        for (int y = 0; y < h; y++) {
            row = (uint32_t*)((char*)img_data + y * bytes_per_line);
            for (int x = 0; x < w; x++) {
                unsigned int a = CornerAlpha(x, y, w, h, R, baseAlpha8);
                row[x] = a ? (a << 24) | (r << 16) | (g << 8) | b : 0;
            }
        }
    }

    #undef _DITHER
    #undef _APPLY_DITHER
}

static Pixmap x11_create_background_pixmap(Display* dpy, Window win, int w, int h, XWindowAttributes* attrs, unsigned long base, unsigned int baseAlpha8) {
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

    unsigned int r = (base >> 16) & 0xFF, g = (base >> 8) & 0xFF, b = base & 0xFF;
    unsigned long fill = hasAlpha ? (baseAlpha8 << 24) | (r << 16) | (g << 8) | b : (r << 16) | (g << 8) | b;
    x11_generate_background_pixels((uint32_t*)img->data, img->bytes_per_line, w, h, R, baseAlpha8, base, fill);

    GC gc = XCreateGC(dpy, pm, 0, NULL);
    XPutImage(dpy, pm, gc, img, 0, 0, 0, 0, w, h);
    XFreeGC(dpy, gc);
    XDestroyImage(img);
    return pm;
}

Pixmap x11_get_cached_background(Display* dpy, Window win, int w, int h) {
    PROFILE_START(timer);
    if (!dpy || w <= 0 || h <= 0) { PROFILE_END_VAR(timer, "helpers_x11:x11_get_cached_background"); return None; }
    unsigned long base = rgb_to_hex(COLOR_BG);
    int preset = ShaderPreset;
    float intensity = ShaderIntensity;
    float smoothness = 1;
    int radius = CornerRadius;
    bool rounded = RoundedCorners;
    if (g_bgPixmap != None && g_bgDpy == dpy && g_bgW == w && g_bgH == h && g_bgBase == base && g_bgPreset == preset && g_bgIntensity == intensity && g_bgSmoothness == smoothness && g_bgRadius == radius && g_bgRounded == rounded) {
        PROFILE_END_VAR(timer, "helpers_x11:x11_get_cached_background");
        return g_bgPixmap;
    }
    if (g_bgPixmap != None) {
        XFreePixmap(g_bgDpy, g_bgPixmap);
        g_bgPixmap = None;
        g_bgW = g_bgH = 0;
    }
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(dpy, win, &attrs)) { PROFILE_END_VAR(timer, "helpers_x11:x11_get_cached_background"); return None; }
    unsigned int baseAlpha8 = (unsigned int)WindowOpacity;
    if (baseAlpha8 > 255) baseAlpha8 = 255;
    Pixmap pm = x11_create_background_pixmap(dpy, win, w, h, &attrs, base, baseAlpha8);
    if (!pm) { PROFILE_END_VAR(timer, "helpers_x11:x11_get_cached_background"); return None; }
    g_bgPixmap = pm;
    g_bgW = w; g_bgH = h; g_bgBase = base; g_bgPreset = preset; g_bgIntensity = intensity; g_bgSmoothness = smoothness; g_bgRadius = radius; g_bgRounded = rounded; g_bgDpy = dpy; g_bgDepth = attrs.depth;
    PROFILE_END_VAR(timer, "helpers_x11:x11_get_cached_background");
    return g_bgPixmap;
}

void x11_draw_background_to_drawable(Display* dpy, Drawable drw, int w, int h) {
    if (!dpy || w <= 0 || h <= 0) return;
    unsigned long base = rgb_to_hex(COLOR_BG);
    int preset = ShaderPreset;
    float intensity = ShaderIntensity;
    float smoothness = 1;
    int radius = CornerRadius;
    bool rounded = RoundedCorners;
    Pixmap cached = None;
    if (g_bgPixmap != None && g_bgDpy == dpy && g_bgW == w && g_bgH == h && g_bgBase == base && g_bgPreset == preset && g_bgIntensity == intensity && g_bgSmoothness == smoothness && g_bgRadius == radius && g_bgRounded == rounded) {
        cached = g_bgPixmap;
    }
    if (cached != None) {
        GC gc = XCreateGC(dpy, drw, 0, NULL);
        XCopyArea(dpy, cached, drw, gc, 0, 0, w, h, 0, 0);
        XFreeGC(dpy, gc);
        return;
    }
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
    unsigned int r = (base >> 16) & 0xFF, g = (base >> 8) & 0xFF, b = base & 0xFF;
    unsigned long fill = hasAlpha ? (baseAlpha8 << 24) | (r << 16) | (g << 8) | b : (r << 16) | (g << 8) | b;
    GC gc = XCreateGC(dpy, drw, 0, NULL);
    x11_generate_background_pixels((uint32_t*)img->data, img->bytes_per_line, w, h, R, baseAlpha8, base, fill);
    XPutImage(dpy, drw, gc, img, 0, 0, 0, 0, w, h);
    XFreeGC(dpy, gc);
    XDestroyImage(img);
}

void x11_draw_background(Display* dpy, Window win, int w, int h) {
    PROFILE_START(timer);
    const char* presetName = "solid";
    if (!EnableShaders || ShaderPreset == 0) presetName = "solid";
    else if (ShaderPreset == 1) presetName = "vertical";
    else if (ShaderPreset == 2) presetName = "radial";
    else if (ShaderPreset == 3) presetName = "diagonal";
    else if (ShaderPreset == 4) presetName = "noise";
    else presetName = "solid-fallback";
    Pixmap bg = x11_get_cached_background(dpy, win, w, h);
    if (bg) {
        GC gc = XCreateGC(dpy, win, 0, NULL);
        XCopyArea(dpy, bg, win, gc, 0, 0, w, h, 0, 0);
        XFreeGC(dpy, gc);
    }
    (void)presetName;
    PROFILE_END_VAR(timer, "helpers_x11:x11_draw_background");
}

void x11_draw_selection_background(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, int x, int y, int w, int h, unsigned long baseHex) {
    if (w <= 0 || h <= 0) return;
    XftColor c; x11_xft_alloc_color(dpy, vis, cmap, baseHex, OpacityAlpha16(), &c);
    XftDrawRect(xd, &c, x, y, w, h);
    XftColorFree(dpy, vis, cmap, &c);
}

Window x11_create_prompt_window(Display* dpy, int x, int y, int w, int h) {
    PROFILE_START(timer);
    unsigned long bg = rgb_to_hex(COLOR_BG);
    PROFILE_END_VAR(timer, "helpers_x11:x11_create_prompt_window");
    if (niri_is_active()) {
        log_error("niri override_redirect prompt w=%d h=%d", w, h);
        return x11_create_window_ex_or(dpy, x, y, w, h, bg, "colored", "colored", KeyPressMask | ExposureMask | FocusChangeMask, true);
    }
    Window win = x11_create_window_ex(dpy, x, y, w, h, bg, "colored", "colored", KeyPressMask | ExposureMask | FocusChangeMask);
    Window focus_win = None;
    int revert_to = 0;
    XGetInputFocus(dpy, &focus_win, &revert_to);
    if (focus_win != None && focus_win != PointerRoot && focus_win != win) XSetTransientForHint(dpy, win, focus_win);
    char unique_title[64];
    snprintf(unique_title, sizeof(unique_title), "flint-%lu-%d", (unsigned long)win, (int)getpid());
    XStoreName(dpy, win, unique_title);
    XSizeHints* sh = XAllocSizeHints();
    sh->flags = PMinSize | PMaxSize;
    sh->min_width = w;
    sh->min_height = h;
    sh->max_width = w;
    sh->max_height = h;
    XSetWMNormalHints(dpy, win, sh);
    XFree(sh);
    return win;
}
Window x11_create_edit_popup_window(Display* dpy, int x, int y, int w, int h) {
    PROFILE_START(timer);
    unsigned long bg = rgb_to_hex(COLOR_BG);
    PROFILE_END_VAR(timer, "helpers_x11:x11_create_edit_popup_window");
    if (niri_is_active()) {
        log_error("niri override_redirect edit w=%d h=%d", w, h);
        return x11_create_window_ex_or(dpy, x, y, w, h, bg, "colored", "colored", KeyPressMask | ExposureMask | FocusChangeMask, true);
    }
    Window win = x11_create_window_ex(dpy, x, y, w, h, bg, "colored", "colored", KeyPressMask | ExposureMask | FocusChangeMask);
    Window focus_win = None;
    int revert_to = 0;
    XGetInputFocus(dpy, &focus_win, &revert_to);
    if (focus_win != None && focus_win != PointerRoot && focus_win != win) XSetTransientForHint(dpy, win, focus_win);
    char unique_title[64];
    snprintf(unique_title, sizeof(unique_title), "flint-%lu-%d", (unsigned long)win, (int)getpid());
    XStoreName(dpy, win, unique_title);
    XSizeHints* sh = XAllocSizeHints();
    sh->flags = PMinSize | PMaxSize;
    sh->min_width = w;
    sh->min_height = h;
    sh->max_width = w;
    sh->max_height = h;
    XSetWMNormalHints(dpy, win, sh);
    XFree(sh);
    return win;
}
Window x11_create_display_message_window(Display* dpy, int x, int y, int w, int h) {
    PROFILE_START(timer);
    unsigned long bg = rgb_to_hex(COLOR_BG);
    PROFILE_END_VAR(timer, "helpers_x11:x11_create_display_message_window");
    return x11_create_window_ex_or(dpy, x, y, w, h, bg, "flashtext", "flashtext", ExposureMask, true);
}

XftDraw* x11_create_xft_draw(Display* dpy, Window win) {
    PROFILE_START(timer);
    Visual* vis = NULL; Colormap cmap = 0; int depth = 0;
    if (x11_get_drawable_visual(dpy, win, &vis, &cmap, &depth)) {
        PROFILE_END_VAR(timer, "helpers_x11:x11_create_xft_draw");
        return XftDrawCreate(dpy, win, vis, cmap);
    }
    int screen = DefaultScreen(dpy);
    PROFILE_END_VAR(timer, "helpers_x11:x11_create_xft_draw");
    return XftDrawCreate(dpy, win, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen));
}

gchar* FindPlayerBusName(GDBusConnection *conn) {
    GVariant *reply = g_dbus_connection_call_sync(conn,
        "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
        "ListNames", NULL, G_VARIANT_TYPE("(as)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);
    
    gchar* player_name = NULL;
    if (reply) {
        GVariantIter *iter;
        const gchar *name;
        g_variant_get(reply, "(as)", &iter);
        while (g_variant_iter_next(iter, "&s", &name)) {
            if (g_str_has_prefix(name, "org.mpris.MediaPlayer2.")) {
                player_name = g_strdup(name);
                break;
            }
        }
        g_variant_iter_free(iter);
        g_variant_unref(reply);
    }
    return player_name;
}
