#ifndef PLATFORM_X11_DRAWING_H
#define PLATFORM_X11_DRAWING_H

#include "core/types.h"
#if !defined(_WIN32)
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#endif

#if !defined(_WIN32)
void draw_wrapped_text(Display* dpy, XftDraw* xd, XftColor* color, FontHandle font, const char* text, int startY,char* centered_to,int xOffset, int boxWidth);
int x11_draw_utf8_fallback(Display* dpy, XftDraw* xd, XftColor* color, int x, int y, XftFont* xf, const char* s, int n, int px);
int x11_measure_utf8_fallback(Display* dpy, XftFont* xf, const char* s, int n, int px);
void x11_xft_alloc_color(Display* dpy, Visual* vis, Colormap cmap, unsigned long hex, unsigned short alpha, XftColor* out);

int x11_update_edit_scroll_offset(Display* dpy, XftFont* xf, const char* text, int text_len, int cursor, int width, int prev_scroll, int* out_cursor_x);
void x11_draw_edit_cursor(XftDraw* xd, XftColor* color, int cursor_x, int scroll, int width);
const char* ListBoxItemDisplayText(ListBoxInfo* lb, int idx, char* trunc, size_t trunc_size);
void DrawListBoxItemText(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, FontHandle font,
                         XftColor* num_color, XftColor* body_color, XftColor* plain_color,
                         ListBoxInfo* lb,char* icon_path,int idx, int y, int text_x);
bool CursorBack(size_t* cursor, size_t text_len);
bool CursorForward(size_t* cursor, size_t text_len);
void x11_fill_rounded_rect(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, unsigned long hex, unsigned short alpha16, int x, int y, int w, int h, int radius, int win_w, int win_h);
void x11_draw_background(Display* dpy, Window win, int w, int h);
void x11_draw_background_to_drawable(Display* dpy, Drawable d, int w, int h);
Pixmap x11_get_cached_background(Display* dpy, Window win, int w, int h);
void x11_set_dbl_visual(Display* dpy, Visual* vis, Colormap cmap, int depth);
void x11_clear_dbl_visual(void);
bool x11_get_drawable_visual(Display* dpy, Drawable d, Visual** visOut, Colormap* cmapOut, int* depthOut);
void x11_draw_selection_background(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, int x, int y, int w, int h, unsigned long baseHex);
Pixmap x11_get_cached_background(Display* dpy, Window win, int w, int h);
void x11_draw_background_to_drawable(Display* dpy, Drawable drw, int w, int h);
void x11_free_cached_background(void);
void x11_release_prompt_buffer(Display* dpy, Window win);
bool x11_is_own_window_name(const char* name);

int x11_font_row_height(int ascent, int descent, int font_size);
int x11_font_list_top(int ascent, int descent, int font_size);
int x11_font_cursor_y(int ascent, int descent, int font_size, int box_top);
int x11_font_row_height_of(FontHandle font);
int x11_font_list_top_of(FontHandle font);
int x11_font_cursor_y_of(FontHandle font, int box_top);
#endif

#endif
