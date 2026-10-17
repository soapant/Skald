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

/* Inverse of RGB: takes a 0xRRGGBB literal and yields the ColorRef that
   RGB(r,g,b) would produce for the same colour. Use this instead of passing
   a bare hex literal to any fl_ colour field, which is read as BGR. */
#ifndef HEX
#define HEX(h) ((ColorRef)((((unsigned long)(h) & 0xFF0000UL) >> 16) \
                          | (((unsigned long)(h) & 0xFF00UL)) \
                          | (((unsigned long)(h) & 0xFFUL) << 16)))
#endif

#endif
