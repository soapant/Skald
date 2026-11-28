#ifndef BG_IMAGE_WIN32_H
#define BG_IMAGE_WIN32_H

#include <windows.h>
#include <stdbool.h>

bool win32_bg_image_enabled(void);
typedef struct {
  HDC hdc;
  const RECT *rc;
  int containerW;
  int containerH;
  int originX;
  int originY;
} Win32DrawBgImageSlice;

bool win32_draw_bg_image_slice(Win32DrawBgImageSlice *argspec);
void win32_bg_image_purge(void);
int win32_bg_image_prewarm(int containerW, int containerH);

#endif
