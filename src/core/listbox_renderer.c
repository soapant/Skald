#include "core/types.h"
#include "core/context.h"
#include "core/common_ui.h"
#include "config.h"
#include "platform/draw.h"
#if !defined(_WIN32)
#include "platform/x11/drawing.h"
#endif
#include "helpers/ui.h"
#include "core/timer.h"

#if !defined(_WIN32)
void Core_DrawListBox(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap) {
    PROFILE_START(timer);
    if (!state || !state->has_listbox) { PROFILE_END_VAR(timer, "listbox_renderer:Core_DrawListBox"); return; }
    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    ListBoxInfo *lb = &state->listbox;
    if (!lb->populated_count) { Platform_DestroyFont(font); PROFILE_END_VAR(timer, "listbox_renderer:Core_DrawListBox"); return; }

    int y0 = 70;
    int itemH = FontSize + 14;
    int visible = (state->height - y0 - 40) / itemH;
    if (visible < 1) visible = 1;
    if (visible > lb->populated_count) visible = lb->populated_count;

    if (lb->selected_index >= 0) {
        if (lb->selected_index < lb->view_offset + PaddingForScrolling)
            lb->view_offset = lb->selected_index - PaddingForScrolling;
        else if (lb->selected_index >= lb->view_offset + visible - PaddingForScrolling)
            lb->view_offset = lb->selected_index - visible + PaddingForScrolling + 1;
    }
    if (lb->view_offset < 0) lb->view_offset = 0;
    if (lb->view_offset + visible > lb->populated_count)
        lb->view_offset = lb->populated_count - visible;
    if (lb->view_offset < 0) lb->view_offset = 0;

    for (int i = 0; i < visible; i++) {
        int idx = lb->view_offset + i;
        int y = y0 + i * itemH;
        bool sel = (idx == lb->selected_index);
        if (RoundedCorners && CornerRadius>0) {
            int R=6;
            int bx=3, bw=state->width-14;
            Linux_DrawRoundedBorder(dpy, xd, vis, cmap, rgb_to_hex(COLOR_SEPERATOR), OpacityAlpha16(), bx, y+1, bw, itemH-2, R);
            if (sel) {
                Linux_FillRoundedRect(dpy, xd, vis, cmap, rgb_to_hex(COLOR_SELECTED_TEXT), OpacityAlpha16(), bx+1, y+2, bw-2, itemH-4, R-1);
            } else if (!EnableShaders || ShaderPreset==0) {
                Linux_FillRoundedRect(dpy, xd, vis, cmap, rgb_to_hex(COLOR_BG), OpacityAlpha16(), bx+1, y+2, bw-2, itemH-4, R-1);
            }
        } else {
            Platform_DrawListBackground(state, dpy, xd, vis, cmap, y, itemH, sel);
        }
        //temporary
        const char *icon_path = NULL;
        Platform_DrawListItem(state, dpy, xd, vis, cmap, font, lb, idx, y, 45, icon_path);
    }

    Platform_DestroyFont(font);
    PROFILE_END_VAR(timer, "listbox_renderer:Core_DrawListBox");
}
#else
void Core_DrawListBox(PromptState *state, void *hdc) {
    (void)state; (void)hdc;
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "listbox_renderer:Core_DrawListBox");
}
#endif
