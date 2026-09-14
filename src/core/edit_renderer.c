#include "core/types.h"
#include "core/context.h"
#include "core/common_ui.h"
#include "config.h"
#include "platform/draw.h"
#include "core/timer.h"

#if !defined(_WIN32)
void Core_DrawEditBox(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap) {
    PROFILE_START(timer);
    if (!state) { PROFILE_END_VAR(timer, "edit_renderer:Core_DrawEditBox"); return; }
    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    Platform_DrawEditBackground(state, dpy, xd, vis, cmap, font);
    Platform_DrawEditText(state, dpy, xd, vis, cmap, font);
    Platform_DrawEditCursor(state, dpy, xd, vis, cmap, font);
    Platform_DestroyFont(font);
    PROFILE_END_VAR(timer, "edit_renderer:Core_DrawEditBox");
}
#else
void Core_DrawEditBox(PromptState *state, void *hdc) {
    (void)state; (void)hdc;
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "edit_renderer:Core_DrawEditBox");
}
#endif
