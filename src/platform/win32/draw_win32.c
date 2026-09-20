#include "platform/draw.h"
#include "core/platform.h"
#include "platform/win32/win32_state.h"
#include "config.h"
#include "platform/ui.h"
#include "core/context.h"
#include "helpers/helpers_ui.h"

static HFONT g_sizeFont = NULL;
static int g_sizeFontSize = 0;

static HFONT Win32_SizedFont(int size) {
  if (size <= 0) size = FontSize;
  if (!g_sizeFont || g_sizeFontSize != size) {
    if (g_sizeFont) { DeleteObject(g_sizeFont); g_sizeFont = NULL; g_sizeFontSize = 0; }
    g_sizeFont = FH_TO_HFONT(Platform_CreateFont(size, UsedFont));
    g_sizeFontSize = size;
  }
  return g_sizeFont;
}

static const char *g_prerender_text = NULL;
static void *g_prerender_rect = NULL;
void Platform_SetPreRenderStateWin32(const char *text, void *rect) {
  g_prerender_text = text; g_prerender_rect = rect; 
}

bool Platform_PreRender(void) {
    if (!g_prerender_text || !g_prerender_rect) return false;
    return true;
}

__attribute__((weak)) bool fl_prerender(void) {
    return Platform_PreRender();
}

static HDC g_draw_hdc = NULL;
void Platform_SetDrawContextWin32(HDC hdc) {
  g_draw_hdc = hdc; 
}
void Platform_GetDrawContextWin32(HDC *out) {
  if (out) *out = g_draw_hdc; 
}
bool Platform_DrawTextRaw(int x, int y, const char *text, unsigned long color, FontHandle font, const char *align, int pad,int fontsize) {
    if (!text || !g_draw_hdc) return false;
    if (color != 0) color = rgb_to_hex(color);
    if (color == 0) color = COLOR_TEXT;
    const char *a = align ? align : "CENTER";
    HFONT hfont = font.hfont ? font.hfont : Win32_SizedFont(fontsize);
    if (!hfont) hfont = g_win32_hMainFont;
    HFONT old = NULL;
    if (hfont) old = (HFONT)SelectObject(g_draw_hdc, hfont);
    SetBkMode(g_draw_hdc, TRANSPARENT);
    SetTextColor(g_draw_hdc, color);
    RECT rc = {x, y, x+800, y+40};
    UINT fmt = DT_SINGLELINE | DT_VCENTER;
    if (Platform_StringContainsCaseInsensitive(a,"CENTER")) fmt |= DT_CENTER;
    else if (Platform_StringContainsCaseInsensitive(a,"RIGHT")) fmt |= DT_RIGHT;
    else fmt |= DT_LEFT;
    if (pad) rc.left += pad;
    DrawTextA(g_draw_hdc, text, -1, &rc, fmt);
    if (old) SelectObject(g_draw_hdc, old);
    return true;
}

int Platform_MeasureTextWidth(const char* text, int len, int fontsize) {
    if (!text || len <= 0) return 0;
    if (!g_draw_hdc) return len * (fontsize > 0 ? fontsize : 16) / 2;
    HFONT hfont = Win32_SizedFont(fontsize);
    HFONT old = hfont ? (HFONT)SelectObject(g_draw_hdc, hfont) : NULL;
    SIZE sz;
    sz.cx = 0;
    GetTextExtentPoint32A(g_draw_hdc, text, len, &sz);
    if (old) SelectObject(g_draw_hdc, old);
    return sz.cx;
}

bool Platform_DrawTextLine(int x, int y, const char* text, int len, unsigned long color, int fontsize) {
    if (!text || len <= 0 || !g_draw_hdc) return false;
    if (color != 0) color = rgb_to_hex(color);
    if (color == 0) color = COLOR_TEXT;
    HFONT hfont = Win32_SizedFont(fontsize);
    HFONT old = hfont ? (HFONT)SelectObject(g_draw_hdc, hfont) : NULL;
    SetBkMode(g_draw_hdc, TRANSPARENT);
    SetTextColor(g_draw_hdc, color);
    TextOutA(g_draw_hdc, x, y, text, len);
    if (old) SelectObject(g_draw_hdc, old);
    return true;
}

void Platform_GetItemColors(int idx, int count, unsigned long *num_color, unsigned long *body_color, unsigned long *plain_color) {
    (void)count;
    unsigned long col_text = COLOR_TEXT;
    unsigned long col_alt = COLOR_ALTERNATE;
    float t = (GradientItems > 1) ? (float)(idx % GradientItems) / (float)(GradientItems - 1) : 0;
    unsigned long col_rainbow = GetRainbowColor(t, 0.85f, GradientBrightness);
    unsigned long n = col_text, b = col_text, p = col_rainbow;
    if (RainbowText) {
        n = b = p = col_rainbow;
    } else if (AlternatingText && idx % 2 == 0) {
        n = b = p = col_alt;
    }
    if (num_color) *num_color = n;
    if (body_color) *body_color = b;
    if (plain_color) *plain_color = p;
}

bool Platform_DrawRectRaw(int x, int y, int w, int h, unsigned long color) {
    RECT rect;
    rect.left = x;
    rect.top = y;
    rect.right = x + w;
    rect.bottom = y + h;

    HBRUSH hBrush = CreateSolidBrush(color);

    FillRect(g_draw_hdc, &rect, hBrush);

    DeleteObject(hBrush);

    return true;
}
