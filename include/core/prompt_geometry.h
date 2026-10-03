#ifndef PROMPT_GEOMETRY_H
#define PROMPT_GEOMETRY_H

#include "config.h"

/* Canonical prompt-popup geometry, in popup-relative pixels.
   The X11 backend is the reference implementation for every value below.
   src/platform/x11/draw_x11.c, helpers_x11.c and flapi_x11.c are defined in
   terms of these; src/platform/win32 must consume the same constants so the
   two backends stay pixel-identical. Do not re-introduce bare literals for any
   quantity named here. */

#define FLINT_PROMPT_W 900
#define FLINT_PROMPT_H 640

#define FLINT_EDIT_INSET 14
#define FLINT_EDIT_SCROLL_PAD 28

#define FLINT_CARET_LO_CLAMP 2
#define FLINT_CARET_HI_CLAMP 4
#define FLINT_CARET_W_INSERT 2
#define FLINT_CARET_W_NORMAL_BONUS 2
/* Win32 only: nudges the manually drawn caret along the text baseline.
   X11 needs no equivalent because it draws from the same Xft metrics it
   renders the glyphs with. On Win32 the caret x comes from GDI measurement
   of the control's own font, which lands a fraction of a glyph off the ink
   edge. Value = fraction of tmAveCharWidth. Flip the sign to move the other
   way; 0 disables. */
#define FLINT_CARET_X_BIAS_NUM 1
#define FLINT_CARET_X_BIAS_DEN 2
#define FLINT_EDIT_H_BONUS 12

#define FLINT_ROW_H_BONUS 12
#define FLINT_LIST_TOP_BONUS 38
#define FLINT_LIST_BOTTOM_TITLE 40

#define FLINT_ITEM_TEXT_X 10

#define FLINT_SEL_RADIUS 6
#define FLINT_SEL_INSET 1
#define FLINT_SEL_X 4
#define FLINT_SEL_W_SHRINK 8
#define FLINT_SEL_TOP_INSET 1

#define FLINT_FOOTER_X 5
#define FLINT_FOOTER_Y_FROM_BOTTOM 40
#define FLINT_FOOTER_PAD 5

/* POPUP_EDIT. src/platform/x11/edit_popup_x11.c is the reference:
   text inset 10, line box top at (popup_h / 2) - 15, mode indicator at (5,5).
   Its caret widths are fixed literals, not FontSize-derived like the prompt. */
#define FLINT_EDITPOPUP_INSET 10
#define FLINT_EDITPOPUP_CENTER_OFFSET_Y (-15)
#define FLINT_EDITPOPUP_MODE_X 5
#define FLINT_EDITPOPUP_MODE_Y 5
#define FLINT_EDITPOPUP_MODE_PAD 5
#define FLINT_EDITPOPUP_CARET_W_NORMAL 12

/* Dead helper src/platform/x11/helpers_x11.c:x11_draw_edit_cursor has its own
   literals and disagrees with the live prompt caret path. Kept as-is. */
#define FLINT_X11_DEADCURSOR_CLAMP 10
#define FLINT_X11_DEADCURSOR_Y 20

/* Defaults used by the custom-render (fl_place_*) path when the caller omits
   a dimension. Kept at the pre-existing X11 values; not applied to the native
   prompt path, where the list/edit geometry is derived from the prompt size. */
#define FLINT_CUSTOM_FALLBACK_W 880
#define FLINT_CUSTOM_FALLBACK_LIST_H 400
#define FLINT_CUSTOM_FALLBACK_LIST_W 650
#define FLINT_CUSTOM_EDIT_H_BONUS 28

static inline int FlintRowHeight(void) { return FontSize + FLINT_ROW_H_BONUS; }
static inline int FlintListTop(void) { return FontSize + FLINT_LIST_TOP_BONUS; }
static inline int FlintNormalCaretWidth(void) { return FontSize / 2 + FLINT_CARET_W_NORMAL_BONUS; }
static inline int FlintCustomEditHeight(void) { return FontSize + FLINT_CUSTOM_EDIT_H_BONUS; }
static inline int FlintListBottomMargin(void) {
  return (WindowTitleText || VimModeText) ? FLINT_LIST_BOTTOM_TITLE : 0;
}

#endif