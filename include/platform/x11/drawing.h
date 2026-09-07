#ifndef PLATFORM_X11_DRAWING_H
#define PLATFORM_X11_DRAWING_H

#include "core/types.h"
#if !defined(_WIN32)
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#endif

#if !defined(_WIN32)
void draw_wrapped_text(Display* dpy, XftDraw* xd, XftColor* color, FontHandle font, const char* text, int startY,char* centered_to,int xOffset);
int get_offset_based_on_number(int index);
void Linux_XftAllocColor(Display* dpy, Visual* vis, Colormap cmap, unsigned long hex, unsigned short alpha, XftColor* out);
unsigned long VimModeTextColor(void);
int Linux_UpdateEditScrollOffset(Display* dpy, XftFont* xf, const char* text, int text_len, int cursor, int width, int prev_scroll, int* out_cursor_x);
void Linux_DrawEditCursor(XftDraw* xd, XftColor* color, int cursor_x, int scroll, int width);
const char* ListBoxItemDisplayText(ListBoxInfo* lb, int idx, char* trunc, size_t trunc_size);
void DrawListBoxItemText(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, FontHandle font,
                         XftColor* num_color, XftColor* body_color, XftColor* plain_color,
                         ListBoxInfo* lb, int idx, int y, int text_x);
bool CursorBack(size_t* cursor, size_t text_len);
bool CursorForward(size_t* cursor, size_t text_len);
void Linux_FillRoundedRect(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, unsigned long hex, unsigned short alpha16, int x, int y, int w, int h, int radius);
void Linux_DrawRoundedBackground(Display* dpy, Window win, int w, int h, unsigned long hex, unsigned short alpha);
bool Linux_ShouldUseRounded(void);
void Linux_DrawBackground(Display* dpy, Window win, int w, int h);
void Linux_DrawBackgroundToDrawable(Display* dpy, Drawable d, int w, int h);
Pixmap Linux_GetCachedBackground(Display* dpy, Window win, int w, int h);
void Linux_SetDblVisual(Display* dpy, Visual* vis, Colormap cmap, int depth);
void Linux_ClearDblVisual(void);
bool Linux_GetDrawableVisual(Display* dpy, Drawable d, Visual** visOut, Colormap* cmapOut, int* depthOut);
void Linux_DrawSelectionBackground(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, int x, int y, int w, int h, unsigned long baseHex);
Pixmap Linux_GetCachedBackground(Display* dpy, Window win, int w, int h);
void Linux_DrawBackgroundToDrawable(Display* dpy, Drawable drw, int w, int h);
void Linux_FreeCachedBackground(void);
#endif

#endif
