#include "platform/draw.h"
#include "config.h"
#include "platform/ui.h"
#include "core/context.h"

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
    if (color == 0) color = COLOR_TEXT;
    const char *a = align ? align : "CENTER";
    HFONT hfont = font.hfont ? font.hfont : (FlintContext_Get()->hMainFont ? FlintContext_Get()->hMainFont : NULL);
    HFONT old = NULL;
    if (hfont) old = (HFONT)SelectObject(g_draw_hdc, hfont);
    SetBkMode(g_draw_hdc, TRANSPARENT);
    SetTextColor(g_draw_hdc, color);
    RECT rc = {x, y, x+800, y+40};
    UINT fmt = DT_SINGLELINE | DT_VCENTER;
    if (strcmp(a,"CENTER")==0) fmt |= DT_CENTER;
    else if (strcmp(a,"RIGHT")==0) fmt |= DT_RIGHT;
    else fmt |= DT_LEFT;
    if (pad) rc.left += pad;
    DrawTextA(g_draw_hdc, text, -1, &rc, fmt);
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
