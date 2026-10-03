#ifndef PROMPT_GEOMETRY_H
#define PROMPT_GEOMETRY_H

#include "config.h"

#define PROMPT_W 900
#define PROMPT_H 640

#define PROMPT_EDIT_INSET 14
#define PROMPT_EDIT_SCROLL_PAD 28

#define PROMPT_CARET_LO_CLAMP 2
#define PROMPT_CARET_HI_CLAMP 4
#define PROMPT_CARET_W_INSERT 2
#define PROMPT_CARET_W_NORMAL_BONUS 2
#define PROMPT_CARET_X_BIAS_NUM 1
#define PROMPT_CARET_X_BIAS_DEN 2
#define PROMPT_EDIT_H_BONUS 12

#define PROMPT_ROW_H_BONUS 12
#define PROMPT_LIST_TOP_BONUS 38
#define PROMPT_LIST_BOTTOM_TITLE 40

#define PROMPT_ITEM_TEXT_X 10

#define PROMPT_SEL_RADIUS 6
#define PROMPT_SEL_INSET 1
#define PROMPT_SEL_X 4
#define PROMPT_SEL_W_SHRINK 8
#define PROMPT_SEL_TOP_INSET 1

#define PROMPT_FOOTER_X 5
#define PROMPT_FOOTER_Y_FROM_BOTTOM 40
#define PROMPT_FOOTER_PAD 5

#define PROMPT_EDITPOPUP_INSET 10
#define PROMPT_EDITPOPUP_CENTER_OFFSET_Y (-15)
#define PROMPT_EDITPOPUP_MODE_X 5
#define PROMPT_EDITPOPUP_MODE_Y 5
#define PROMPT_EDITPOPUP_MODE_PAD 5
#define PROMPT_EDITPOPUP_CARET_W_NORMAL 12

#define PROMPT_X11_DEADCURSOR_CLAMP 10
#define PROMPT_X11_DEADCURSOR_Y 20

#define PROMPT_CUSTOM_FALLBACK_W 880
#define PROMPT_CUSTOM_FALLBACK_LIST_H 400
#define PROMPT_CUSTOM_FALLBACK_LIST_W 650
#define PROMPT_CUSTOM_EDIT_H_BONUS 28

static inline int PromptRowHeight(void) { return FontSize + PROMPT_ROW_H_BONUS; }
static inline int PromptListTop(void) { return FontSize + PROMPT_LIST_TOP_BONUS; }
static inline int PromptNormalCaretWidth(void) { return FontSize / 2 + PROMPT_CARET_W_NORMAL_BONUS; }
static inline int PromptCustomEditHeight(void) { return FontSize + PROMPT_CUSTOM_EDIT_H_BONUS; }
static inline int PromptListBottomMargin(void) {
  return (WindowTitleText || VimModeText) ? PROMPT_LIST_BOTTOM_TITLE : 0;
}

#endif
