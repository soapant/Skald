#ifndef fl_window_GEOMETRY_H
#define fl_window_GEOMETRY_H

#include "config.h"

#define fl_window_W 900
#define fl_window_H 640

#define fl_window_BORDER_W 1
/* how far inside the corner arc the band stops; 0.5 keeps it 1px on the
   straight runs without opening gaps in the rasterised arc. */
#define fl_window_BORDER_INNER_OFF 0.5f

#define fl_window_EDIT_INSET 14
#define fl_window_EDIT_SCROLL_PAD 28

#define fl_window_CARET_LO_CLAMP 2
#define fl_window_CARET_HI_CLAMP 4
#define fl_window_CARET_W_INSERT 2
#define fl_window_CARET_W_NORMAL_BONUS 2
#define fl_window_CARET_H_BONUS 6
#define fl_window_EDIT_H_BONUS 12

#define fl_window_ROW_H_BONUS 12
#define fl_window_LIST_TOP_BONUS 38
#define fl_window_LIST_BOTTOM_TITLE 40

#define fl_window_ITEM_TEXT_X 10

#define fl_window_SEL_RADIUS 6
#define fl_window_SEL_INSET 1
#define fl_window_SEL_X 4
#define fl_window_SEL_W_SHRINK 8
#define fl_window_SEL_TOP_INSET 1

#define fl_window_FOOTER_X 5
#define fl_window_FOOTER_Y_FROM_BOTTOM 40
#define fl_window_FOOTER_PAD 5

#define fl_window_EDITPOPUP_INSET 10
#define fl_window_EDITPOPUP_CENTER_OFFSET_Y (-15)
#define fl_window_EDITPOPUP_MODE_X 5
#define fl_window_EDITPOPUP_MODE_Y 5
#define fl_window_EDITPOPUP_MODE_PAD 5
#define fl_window_EDITPOPUP_CARET_W_NORMAL 12
#define fl_window_EDITPOPUP_CARET_X_NUDGE 2

#define fl_window_X11_DEADCURSOR_CLAMP 10
#define fl_window_X11_DEADCURSOR_Y 20

#define fl_window_WRAP_BOX_CENTER 870
#define fl_window_WRAP_BOX_RIGHT 900

#define fl_window_CUSTOM_FALLBACK_W 880
#define fl_window_CUSTOM_FALLBACK_LIST_H 400
#define fl_window_CUSTOM_FALLBACK_LIST_W 650
#define fl_window_CUSTOM_EDIT_H_BONUS 28

static inline int PromptRowHeight(void) { return FontSize + fl_window_ROW_H_BONUS; }
static inline int PromptListTop(void) { return FontSize + fl_window_LIST_TOP_BONUS; }
static inline int PromptNormalCaretWidth(void) { return FontSize / 2 + fl_window_CARET_W_NORMAL_BONUS; }
static inline int PromptCaretHeight(void) { return FontSize + fl_window_CARET_H_BONUS; }
static inline int PromptCustomEditHeight(void) { return FontSize + fl_window_CUSTOM_EDIT_H_BONUS; }
static inline int PromptListBottomMargin(void) {
  return (WindowTitleText || VimModeText) ? fl_window_LIST_BOTTOM_TITLE : 0;
}

#endif
