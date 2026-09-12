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
    Platform_BeginEditDraw(state, dpy, xd, vis, cmap);
    Platform_DrawEditBackground(state, dpy, xd, vis, cmap, font);
    Platform_DrawEditText(state, dpy, xd, vis, cmap, font);
    Platform_DrawEditCursor(state, dpy, xd, vis, cmap, font);
    Platform_DrawSeparators(state, dpy, xd, vis, cmap);
    Platform_EndEditDraw(state, dpy, xd, vis, cmap);
    Platform_DestroyFont(font);
#else
void Core_DrawEditBox(PromptState *state, void *hdc) {
    PROFILE_START(timer);
    if (!state) { PROFILE_END_VAR(timer, "edit_renderer:Core_DrawEditBox"); return; }
    Platform_BeginEditDraw(state, hdc);
    Platform_DrawEditBackground(state, hdc);
    Platform_DrawEditText(state, hdc);
    Platform_DrawEditCursor(state, hdc);
    Platform_DrawSeparators(state, hdc);
    Platform_EndEditDraw(state, hdc);
#endif
    PROFILE_END_VAR(timer, "edit_renderer:Core_DrawEditBox");
}
