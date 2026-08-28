#ifndef DISPLAY_X11_H
#define DISPLAY_X11_H

#include "core/types.h"

bool init_display(void);
Display* get_display(void);
void cleanup_display(void);

#endif
