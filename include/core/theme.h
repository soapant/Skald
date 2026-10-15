#ifndef CORE_THEME_H
#define CORE_THEME_H

#include <stdbool.h>

void Theme_Apply(void);
bool Theme_ColorPinned(const char *name);
void Theme_MarkColorPinned(const char *name, bool pinned);

#endif
