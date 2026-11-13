#ifndef HELPERS_WIN32_DRAW_H
#define HELPERS_WIN32_DRAW_H

#include <windows.h>
#include <stdbool.h>
#include <stdint.h>
#include "core/types.h"
#include "core/editbox.h"
#include "helpers/helpers_text.h"
#include "config_values.h"
#include "core/types/window.h"
#include "core/types/color.h"
#include "core/types/prompt.h"
#include "platform/draw.h"
#include "core/prompt_geometry.h"
#include "platform/platform.h"
#include "helpers/win32/win32_state.h"
#include "helpers/helpers_ui.h"
#include "core/draw_state.h"
#include "deps/log.h"
#include <stdio.h>
#include <string.h>
#include <objidl.h>
#include <shellapi.h>
#include <gdiplus/gdiplus.h>

typedef struct PromptState PromptState;

#define WIN32_SIZE_FONT_SLOTS 16
#define WIN32_SYM_SLOTS 16
#define WIN32_IMG_CACHE_SLOTS 24
#define WIN32_SRC_CACHE_SLOTS 8
#define WIN32_IMG_REVALIDATE_MS 1000
#define WIN32_IMG_FAILED ((HBITMAP) - 1)
#define WIN32_SYMBOL_SIZE_NUM 5
#define WIN32_SYMBOL_SIZE_DEN 4

extern __thread HDC g_draw_hdc;
extern __thread HFONT g_sizeFonts[WIN32_SIZE_FONT_SLOTS];
extern __thread int g_sizeFontSizes[WIN32_SIZE_FONT_SLOTS];
extern HFONT g_sym_fonts[WIN32_SYM_SLOTS];
extern int g_sym_sizes[WIN32_SYM_SLOTS];
extern wchar_t g_sym_cp[WIN32_SYM_SLOTS][8];
extern int g_sym_len[WIN32_SYM_SLOTS];
extern ULONGLONG g_sym_tick;

typedef struct {
  char key[512];
  int w, h;
  FILETIME mtime;
  DWORD size;
  HDC screen;
  void *bmp;
  ULONGLONG stamp;
  DWORD checked_ms;
  int failed;
} Win32ImgSlot;

typedef struct {
  char key[512];
  FILETIME mtime;
  DWORD size;
  void *src;
  ULONGLONG stamp;
  DWORD checked_ms;
} Win32SrcSlot;

extern Win32ImgSlot g_img_cache[WIN32_IMG_CACHE_SLOTS];
extern Win32SrcSlot g_src_cache[WIN32_SRC_CACHE_SLOTS];
extern ULONGLONG g_img_cache_tick;
extern CRITICAL_SECTION g_img_cache_cs;
extern INIT_ONCE g_img_cache_cs_once;
extern ULONG_PTR g_gdip_token;
extern HDC s_cached_hdc;
#include <gdiplus.h>
extern GpGraphics *s_cached_g;

HFONT win32_sized_font(int size);
HFONT win32_symbol_font(int size, const wchar_t *wide, int len);
bool win32_text_is_symbolic(const wchar_t *wide, int len);
bool win32_font_covers(HFONT f, const wchar_t *wide, int len);
bool win32_text_needs_boost(const wchar_t *wide, int len);
HFONT win32_font_for_text(const wchar_t *wide, int len, int fontsize,
                          HFONT *out_old, int *out_dy);
COLORREF win32_colorref(unsigned long hex);
COLORREF win32_resolve_color(unsigned long hex, bool opaque);

wchar_t *win32_widen(const char *in, int in_len, int *out_len);
void win32_gdip_ensure(void);
void win32_gdip_dispose(void *img);
HDC win32_screen_dc(void);
void win32_img_cache_lock(void);
void win32_img_cache_unlock(void);
void *win32_img_cache_get(const char *path, HDC screen, int w, int h);
void win32_img_cache_put(const char *path, HDC screen, int w, int h, void *img);
void *win32_src_cache_get(const char *path);
void win32_src_cache_put(const char *path, void *src);
void win32_img_cache_free_all(void);
void *win32_load_scaled_bitmap(const char *path, int w, int h);
void *win32_load_shell_icon_bitmap(const char *path);
void *win32_load_shell_icon_ex(const char *path, bool use_file_attributes);
void win32_utf8_to_wide(const char *in, wchar_t *out, int out_max);

void win32_prompt_editbox_spec(PromptState *state, FontHandle font,
                               EditboxSpec *eb);

#endif
