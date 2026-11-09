#ifndef CORE_THEME_H
#define CORE_THEME_H

#include <stdbool.h>
#include <stddef.h>

#include "core/types/color.h"

typedef struct {
  const char *name;
  ColorRef bg;
  ColorRef text;
  ColorRef alternate;
  ColorRef selected_text;
  ColorRef cursor;
  ColorRef border;
} ThemeEntry;

void Theme_Apply(void);
bool Theme_ColorPinned(const char *name);
const ThemeEntry *Theme_Find(const char *name);
size_t Theme_Count(void);
const ThemeEntry *Theme_EntryAt(size_t index);
bool Theme_PinScanFile(const char *path);
void Theme_ResetPinned(void);

#endif
