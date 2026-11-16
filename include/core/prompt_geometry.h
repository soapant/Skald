#ifndef PROMPT_GEOMETRY_H
#define PROMPT_GEOMETRY_H

#include "config.h"

#define PROMPT_BORDER_W 1
#define PROMPT_SEP_W 1
#define PROMPT_BORDER_INNER_OFF 1.0f

#define PROMPT_EDIT_INSET 14
#define PROMPT_EDIT_SCROLL_PAD 28

#define PROMPT_CARET_LO_CLAMP 2
#define PROMPT_CARET_HI_CLAMP 4
#define PROMPT_CARET_H_BONUS 4
#define PROMPT_EDIT_H_BONUS 12

#define PROMPT_ROW_H_BONUS 12
#define PROMPT_LIST_TOP_BONUS 38
#define PROMPT_LIST_BOTTOM_TITLE 40

#define PROMPT_ITEM_TEXT_X 10

#define PROMPT_GLYPH_INK_PAD 10

#define PROMPT_SEL_RADIUS 6
#define PROMPT_SEL_INSET 1
#define PROMPT_SEL_X 4
#define PROMPT_SEL_W_SHRINK 8
#define PROMPT_SEL_TOP_INSET 1

#define PROMPT_FOOTER_X 5
#define PROMPT_FOOTER_Y_FROM_BOTTOM 40
#define PROMPT_FOOTER_PAD 5

#define PROMPT_DEADCURSOR_CLAMP 10
#define PROMPT_DEADCURSOR_Y 2

#define PROMPT_WRAP_BOX_CENTER (PromptWidth - 30)
#define PROMPT_WRAP_BOX_RIGHT PromptWidth

#define PROMPT_CUSTOM_FALLBACK_W 880
#define PROMPT_CUSTOM_FALLBACK_LIST_H 400
#define PROMPT_CUSTOM_FALLBACK_LIST_W 650
#define PROMPT_CUSTOM_EDIT_H_BONUS 28

int prompt_row_height(void);
int prompt_list_top(void);
int prompt_caret_height(void);
int prompt_custom_edit_height(void);
int prompt_list_bottom_margin(void);

#endif
