#ifndef CORE_LISTBOX_LAYOUT_H
#define CORE_LISTBOX_LAYOUT_H

#include <stdbool.h>
#include "core/types/flapi_types.h"

typedef struct {
    int bx;
    int bw;
    int fw;
    int fh;
    int y0;
    int item_h;
    int list_h;
    int visible;
} ListboxLayout;

void Listbox_ComputeLayout(const FlListSpec* spec, int default_y0, int item_h,
                           int popup_w, int popup_h, int count, ListboxLayout* out);

#endif
