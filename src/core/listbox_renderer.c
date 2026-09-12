#include "core/types.h"
#include "core/context.h"
#include "core/common_ui.h"
#include "config.h"
#include "platform/draw.h"
#include "core/timer.h"

#if !defined(_WIN32)
void Core_DrawListBox(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap) {
    PROFILE_START(timer);
    if (!state || !state->has_listbox) { PROFILE_END_VAR(timer, "listbox_renderer:Core_DrawListBox"); return; }
    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
#else
void Core_DrawListBox(PromptState *state, void *hdc) {
    PROFILE_START(timer);
    if (!state || !state->has_listbox) { PROFILE_END_VAR(timer, "listbox_renderer:Core_DrawListBox"); return; }
    Display *dpy = NULL; XftDraw *xd = NULL; Visual *vis = NULL; Colormap cmap = 0;
    FontHandle font = {0};
    (void)dpy; (void)xd; (void)vis; (void)cmap; (void)font;
#endif
    ListBoxInfo *lb = &state->listbox;
    if (!lb->populated_count) { PROFILE_END_VAR(timer, "listbox_renderer:Core_DrawListBox"); return; }

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

#if !defined(_WIN32)
    Platform_BeginListDraw(state, dpy, xd, vis, cmap);
#else
    Platform_BeginListDraw(state, hdc);
#endif

    for (int i = 0; i < visible; i++) {
        int idx = lb->view_offset + i;
        int y = y0 + i * itemH;
        bool sel = (idx == lb->selected_index);

#if !defined(_WIN32)
        Platform_DrawListBackground(state, dpy, xd, vis, cmap, y, itemH, sel);
#else
        Platform_DrawListBackground(state, hdc, y, itemH, sel);
#endif

        const char *icon_path = NULL;
#if !defined(_WIN32)
        Platform_DrawListItem(state, dpy, xd, vis, cmap, font, lb, idx, y, 45, icon_path);
#else
        Platform_DrawListItem(state, hdc, lb, idx, y, 45, icon_path);
#endif
    }

#if !defined(_WIN32)
    Platform_EndListDraw(state, dpy, xd, vis, cmap);
    Platform_DestroyFont(font);
#else
    Platform_EndListDraw(state, hdc);
#endif
    PROFILE_END_VAR(timer, "listbox_renderer:Core_DrawListBox");
}
