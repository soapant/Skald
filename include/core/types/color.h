#ifndef TYPES_COLOR_H
#define TYPES_COLOR_H

#include <stdint.h>
#include <stdbool.h>

typedef uint32_t ColorRef;

#if !defined(FALSE)
#define FALSE 0
#define TRUE 1
#endif

#if !defined(RGB)
#define RGB(r, g, b) ((uint32_t)(((r) & 0xFF) | (((g) & 0xFF) << 8) | (((b) & 0xFF) << 16)))
#endif

#ifndef RGB
#define RGB(r, g, b)  ((unsigned long)(((unsigned char)(r)|((unsigned short)((unsigned char)(g))<<8))|(((unsigned long)(unsigned char)(b))<<16)))
#endif
#define COLOR_BG      RGB(5, 5, 10)   
#define COLOR_TEXT    RGB(255, 255, 255)
#define COLOR_ALTERNATE RGB(57, 255, 20)
#define COLOR_SELECTED_TEXT     RGB(57, 105, 20)
#define COLOR_VISUAL_SELECTED_TEXT    RGB(48,25,52)
#define COLOR_ALIAS_TEXT    RGB(255,255,255)
#define COLOR_INSERT_MODE_TEXT (ColoredVimModeText ? RGB(50, 255, 50) : COLOR_TEXT)
#define COLOR_NORMAL_MODE_TEXT (ColoredVimModeText ? RGB(100, 255, 255) : COLOR_TEXT)
#define COLOR_VISUAL_MODE_TEXT (ColoredVimModeText ? RGB(200, 3, 222) : COLOR_TEXT)
#define COLOR_ERROR             RGB(180, 0, 0) 
#define COLOR_NORMAL            RGB(200, 200, 0) 
#define COLOR_TIP               RGB(0, 160, 0)

#endif
