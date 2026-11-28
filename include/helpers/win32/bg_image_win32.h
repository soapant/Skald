#ifndef BG_IMAGE_WIN32_H
#define BG_IMAGE_WIN32_H

#include <windows.h>
#include <stdbool.h>

bool win32_bg_image_enabled(void);
typedef struct {
  HDC f_hdc;
  const RECT *f_rc;
  int f_containerW;
  int f_containerH;
  int f_originX;
  int f_originY;
} FnWin32DrawBgImageSliceSpec;

bool win32_draw_bg_image_slice(FnWin32DrawBgImageSliceSpec *argspec);
void win32_bg_image_purge(void);
int win32_bg_image_prewarm(int containerW, int containerH);

#endif