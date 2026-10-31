#include "core/listbox_layout.h"
#include "core/prompt_geometry.h"
#include "core/timer.h"
#include <string.h>

void Listbox_ComputeLayout(const PlListSpec* spec, int default_y0, int item_h,
                           int popup_w, int popup_h, int count, ListboxLayout* out) {
        
    if (!out) {  return; }
    memset(out, 0, sizeof(*out));
    if (!spec) {  return; }

    out->y0 = spec->y ? spec->y : default_y0;
    out->item_h = item_h > 0 ? item_h : 1;
    out->list_h = spec->h ? spec->h
                           : (popup_h > 0 ? (popup_h - out->y0 - PromptListBottomMargin())
                                          : PROMPT_CUSTOM_FALLBACK_LIST_H);
    if (!spec->x && !spec->w) {
        out->bx = PROMPT_SEL_X;
        out->bw = (popup_w > 0 ? popup_w : PROMPT_CUSTOM_FALLBACK_W) - PROMPT_SEL_W_SHRINK;
        out->fw = popup_w > 0 ? popup_w : PROMPT_W;
        out->fh = popup_h > 0 ? popup_h : PROMPT_H;
    } else {
        out->bx = spec->x + PROMPT_SEL_INSET;
        out->bw = (spec->w ? spec->w : PROMPT_CUSTOM_FALLBACK_LIST_W) - 2;
        out->fw = spec->w ? spec->w : PROMPT_W;
        out->fh = spec->h ? spec->h : PROMPT_H;
    }
    int visible = out->list_h / out->item_h;
    if (visible < 1) visible = 1;
    if (count > 0 && visible > count) visible = count;
    out->visible = visible;
    
}
