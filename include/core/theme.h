#ifndef CORE_THEME_H
#define CORE_THEME_H

#include <stdbool.h>
#include <stddef.h>

#include "core/types/color.h"

typedef struct {
  const char *name;
  ColorRef bg;
  ColorRef text;
  ColorRef selected_text;
  ColorRef cursor;
  ColorRef border;
} ThemeEntry;

void theme_apply(void);
bool theme_color_pinned(const char *name);
const ThemeEntry *theme_find(const char *name);
size_t theme_count(void);
const ThemeEntry *theme_entry_at(size_t index);
bool theme_pin_scan_file(const char *path);
void theme_reset_pinned(void);

#endif
