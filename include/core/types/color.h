#ifndef TYPES_COLOR_H
#define TYPES_COLOR_H

#include <stdint.h>
#include <stdbool.h>

typedef uint32_t ColorRef;

#if !defined(RGB)
#define RGB(r, g, b)                                                           \
  ((uint32_t)(((r) & 0xFF) | (((g) & 0xFF) << 8) | (((b) & 0xFF) << 16)))
#endif

#ifndef HEX
#define HEX(h)                                                                 \
  ((ColorRef)((((unsigned long)(h) & 0xFF0000UL) >> 16) |                      \
              (((unsigned long)(h) & 0xFF00UL)) |                              \
              (((unsigned long)(h) & 0xFFUL) << 16)))
#endif

#endif
